/*****************************************************************
* Name: runlogic.c
* Purpose: This file contains the functions for the runlogic feature of Netstorm.
* Author: Anuj
* Intial version date: 23/02/2008
* Last modification date
*****************************************************************/

#include <stdlib.h> // for random()
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // for getcwd()
#include <sys/wait.h>
#include <dlfcn.h>  // for dlsym
#include <regex.h>

#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_string.h"
#include "runlogic.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_exit.h"

/***************************************************************************/
// Start - Functions and variables used with in this file only.

// Default value for the LogicBlockElement type elements for switch block
static int numSwitchBlockDefaultValue=0;//make it 0 for backward compatiblity
static int g_max_runlogic_stack=0;

// This mthd for debugging only
static inline char *show_obj_char_type( int isBlock)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called, isBlock = %d", isBlock);
  if (isBlock == 0) return("Page");
  if (isBlock == 1) return("Block");
  if (isBlock == 2) return("Done"); 
  if (isBlock == 3) return("Error");
  else return("Invalid Type for object");
}

// This mthd for debugging only, This will show the all contents of LogicBlockElement
static inline char *show_block_element_contents(LogicBlockElement *blockElements)
{
  static char obj_buf[4096];

  NSDL2_RUNLOGIC(NULL, NULL, "Method called");
  sprintf(obj_buf, "The contents of this object are: ObjType: %s, objIndex: %d, data: %d", show_obj_char_type(blockElements->isBlock), blockElements->objIndex, blockElements->data);

  return (obj_buf);
}

// This mthd for debugging only
static inline char *show_block_char_type(int type)
{
  if(type == NS_CTRLBLOCK_SEQUENCE) return("Sequence");

  if(type == NS_CTRLBLOCK_COUNT) return("Count");

  if(type == NS_CTRLBLOCK_PERCENT) return("Percent");

  if(type == NS_CTRLBLOCK_WEIGHT) return("Weight");

  if(type == NS_CTRLBLOCK_DOWHILE) return("DoWhile");

  if(type == NS_CTRLBLOCK_WHILE) return("While");

  if(type == NS_CTRLBLOCK_SWITCH) return("Switch");

  fprintf(stderr, "Error: The BlockId (%d) is not a valid ID", type);
  return "NULL";
}

// This mthd for debugging only, This will show the all contents of LogicBlockCtrl
static inline char *show_block_contents (LogicBlockCtrl *ctrlBlock)
{
  static char contents_buf[4096];

  NSDL2_RUNLOGIC(NULL, NULL, "Method called");
  sprintf(contents_buf, "The contents of the Block are:- BlockType: %s, BlockName: %s, NumElements: %d, data: %d, Attribute: %s", show_block_char_type(ctrlBlock->blockType), ctrlBlock->blockname, ctrlBlock->numElements, ctrlBlock->data, ((ctrlBlock->attr)?(ctrlBlock->attr):"Not Applied"));

  return (contents_buf);
}

static int inline
ns_get_rand (int min, int max)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called. min=%d, max=%d", min, max);
  if (min == max)
    return min;

  return ((random()%(max-min+1)) + min);
}

static void inline
get_count_block_control (unsigned int state_var, unsigned short *max, unsigned short *cur)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called, state_var=%u, add of max=%p, add of cur=%p", state_var, max, cur);

  *cur = (short) (state_var & 0x0000FFFF);
  *max = (short) ((state_var & 0xFFFF0000) >> 16 );

  NSDL2_RUNLOGIC(NULL, NULL, "Method ended");
}

static void inline
set_count_block_control (unsigned int *state_var, unsigned short max, unsigned short cur)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called, add of state_var=%p, max=%u, cur=%u", state_var, max, cur);

  unsigned int num = max;

  num = num << 16;
  num |= (unsigned int)cur;
  *state_var = num;

  NSDL2_RUNLOGIC(NULL, NULL, "Method ended, &state_var=%u, state_var=%u", state_var, *state_var);
}

// This fn is for NS_CTRLBLOCK_COUNT only
// This mthd will store the max count on the left 2 bytes and cur count on the right 2 bytes of state_var 
static unsigned int inline
init_state_var (LogicBlockCtrl *ctrlBlock)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called, for LogicBlockCtrl *ctrlBlock, %s",show_block_contents(ctrlBlock));

  unsigned short cur, max;
  unsigned int svar;

  if (ctrlBlock->blockType != NS_CTRLBLOCK_COUNT)
    return 0;

  cur = 0;
  max = ns_get_rand (ctrlBlock->numElements, ctrlBlock->data);
  set_count_block_control ((unsigned int *)&svar, max, cur);
  NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_COUNT: state_var=%u, cur=%hd, max=%hd", svar, cur, max);
  return svar;
}

//Called for each script initialization. Does the validation for runlogic components
static int init_control_logic (int numBlocks, LogicBlockCtrl *ctrlBlock)
{
  LogicBlockCtrl *cb;
  int i, type, j, k, l, num, sum, debug_count;
  int switch_elements[500];  //Assumption:User will not give more than 500 values in switch block
  LogicBlockElement *elementsCtrBlock;

  NSDL2_RUNLOGIC(NULL, NULL, "Method called, numBlocks=%d, LogicBlockCtrl *ctrlBlock=%p", numBlocks, ctrlBlock);

  //Set individidual weights or pct to cumulatices for PCT or WEIGHT block types
  for (i = 0; i < numBlocks; i++ ) 
  {
    cb = &ctrlBlock[i];
    type = cb->blockType;
    elementsCtrBlock = cb->blockElements;
    num = cb->numElements;
     
    NSDL1_RUNLOGIC(NULL, NULL, " Info of LogicBlockCtrl: %s", show_block_contents(&ctrlBlock[i]));

    // This for debugging 
    for (debug_count = 0; debug_count < num; debug_count++)
    {
      NSDL1_RUNLOGIC(NULL, NULL, "Info of children (%d) under above LogicBlockCtrl, %s", (debug_count + 1), show_block_element_contents(&elementsCtrBlock[debug_count])); 
    }
     
    if ((num < 1) && (type != NS_CTRLBLOCK_COUNT))
    {
      NS_EXIT(1, "Error: Block '%s' of type '%s' must have atleast one child\n", cb->blockname, show_block_char_type(type));
    }
     
    if ((type == NS_CTRLBLOCK_DOWHILE) || (type == NS_CTRLBLOCK_WHILE))
    {
      if (num != 1)
      {
        NS_EXIT(1, "Error: Block '%s' of type '%s' can not have more than one child\n", cb->blockname, show_block_char_type(type));
      }
    }

    //We can not put this cheack, since elementsCtrBlock is a pointer and it will always return the size of ptr = 4 bytes: remaining : Anuj,  
/*    if ((type == NS_CTRLBLOCK_COUNT) || (type == NS_CTRLBLOCK_DOWHILE) || (type == NS_CTRLBLOCK_WHILE))
    {
      //sizeof(elementsCtrBlock) != sizeof(St)
      if (sizeof(elementsCtrBlock) != sizeof(St))
    }
*/

    if (type == NS_CTRLBLOCK_COUNT)
    {
      // no need of checking this (cb->numElements < 0) its been done priorly, how do we cheak the min_val is < 0
      //if ((cb->numElements < 0) || (cb->data < 0)), 
      if ((cb->data < 0))  // Max must be +ve
      {
        NS_EXIT(1, "Error: The min and max values for Block '%s' of type '%s' should be greater than 0\n", cb->blockname, show_block_char_type(type));
      }

      if ((cb->numElements) > (cb->data)) // Max should be <= min
      {
        NS_EXIT(1, "Error: The min value for Block '%s' of type '%s' can not be greater than max value, The current Min value (%d), Max value (%d)\n", cb->blockname, show_block_char_type(type), cb->numElements, cb->data);
      }
    }
  
    if ((type == NS_CTRLBLOCK_PERCENT) || (type == NS_CTRLBLOCK_WEIGHT)) 
    {
      sum = 0;
      for (j = 0; j < num; j++) 
      {
        if (elementsCtrBlock[j].data < 0)
        {
          fprintf(stderr, "Error: The value (%d) supplied for the childId (%d) for block '%s' of type '%s' must be greater than 0", elementsCtrBlock[j].data, (j+1), cb->blockname, show_block_char_type(type));
        }
        sum += elementsCtrBlock[j].data;
        elementsCtrBlock[j].data = sum;
      }
      cb->data = sum;
      if ((type == NS_CTRLBLOCK_PERCENT) && (sum != 100)) 
      {
        NS_EXIT(1, "Error: Percentage summation for Block '%s' of type '%s' must add to 100, The current summation is (%d)\n", cb->blockname, show_block_char_type(type), sum);
      }
    }

    if ((type == NS_CTRLBLOCK_DOWHILE) || (type == NS_CTRLBLOCK_WHILE) || (type == NS_CTRLBLOCK_SWITCH))
    {
      NSDL2_RUNLOGIC(NULL, NULL, "Block type=%d, NS variable given by the user=%s", type, cb->attr);
      //no check for special chars
      if ((strlen(cb->attr) < 1) || (strlen(cb->attr) > 32))
      {
        NS_EXIT(1, "Error: The length of NS variable (%s) for Block '%s' of type '%s' should be greater than 1 and less than 32 characters, The current lenth of NS variable is (%u)\n", cb->attr, cb->blockname, show_block_char_type(type), (unsigned int)strlen(cb->attr));
      }
      /** Need to check if var are processed at this point and how to check if it is not declared
      if (!ns_eval_string("{" + cb->attr) "}")
      {
        printf("The variable %s given by the user is not an NS variable\n", cb->attr);
        exit(1);
      }
      **/
    }

    // Checking the duplicity in the data, and the last data is default or not 
    if (type == NS_CTRLBLOCK_SWITCH)
    {
      for (k = 0; k < num; k++)
      {
        if ((k == (num - 1)) && (cb->blockElements[k].data != numSwitchBlockDefaultValue))
        {
          NS_EXIT(1, "Error: The last (%d) element of Block '%s' of type '%s' must have default value as (%d), The current value of the last elemant is (%d)\n", (k+1), cb->blockname, show_block_char_type(type), numSwitchBlockDefaultValue, cb->blockElements[k].data);
        }
        
        for (l = 0; l < k; l++)// intializing with 1 since 1st elem will be unique always
        {
          if (k == 0)
            break;
          if (switch_elements[l] == (cb->blockElements[k].data))
          {
            NS_EXIT(1, "Error: The elements of Block '%s' of type '%s' must have unique values, The value supplied for (%d) element (%d) has been already assigned\n", cb->blockname, show_block_char_type(type), cb->blockElements->data, (k+1));
          }
        }
        switch_elements[k] = cb->blockElements[k].data;
      }
    }
  }
  NSDL2_RUNLOGIC(NULL, NULL, "Method Ended Sucessfully");

  return 0;
}

// This mthd will be called for only NS_CTRLBLOCK_WEIGHT 
static int inline
get_ctlLogic_distIdx (LogicBlockCtrl *ctrlBlock)
{
  LogicBlockElement *blockElements = ctrlBlock->blockElements;
  int numElements = ctrlBlock->numElements;
  int max_val = ctrlBlock->data;
  int j, rnum;

  NSDL2_RUNLOGIC(NULL, NULL, "Method called, *ctrlBlock=%p, numElements=%d, The total weight=%d", ctrlBlock, numElements, max_val);

  if (max_val == 0){
    return -1;
  }
  rnum = random()%max_val;
  rnum++;

  for (j=0; j < numElements; j++) 
  {
    if (rnum <= blockElements[j].data)
      break;
  }

  return j;
}

// objIndex is to store the value of next page or block to be called,assiging value to it in mthd
static int
process_ctrl_logic_block (LogicBlockCtrl *ctrlBlock, unsigned int *state_var, int *objIndex)
{
  LogicBlockElement *blockElements;
  unsigned int idx = 0, val = 0, i, numEl = 0;
  int api_ret = 0;

  NSDL2_RUNLOGIC(NULL, NULL, "Method called, state_var=%d, objIndex=%d, %s", *state_var, *objIndex, show_block_contents(ctrlBlock));

  switch (ctrlBlock->blockType) 
  {
    case NS_CTRLBLOCK_SEQUENCE: 
    {
      idx = (unsigned int)*state_var;
      if (idx >= ctrlBlock->numElements)// but we have stored the two values in the state_var 
      {
        *state_var = 0;
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_SEQUENCE: Objidx=%d, returning DONE", idx); 
        return NS_CTRLLOGIC_DONE;
      }
      else
      {
        blockElements = ctrlBlock->blockElements;
        *state_var = idx + 1;
        *objIndex = blockElements[idx].objIndex;
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_SEQUENCE: returning '%s' ObjIndex=%d", show_obj_char_type(blockElements[idx].isBlock), *objIndex);
        return ((blockElements[idx].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }
      break;
    }

    case NS_CTRLBLOCK_COUNT: 
    {
      unsigned short cur, max;
      idx = (unsigned int)*state_var;
      get_count_block_control (idx, &max, &cur);
      NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_COUNT: Objidx=%d cur=%hd and max=%hd", idx, cur, max);
      if (cur >= max)
      {
        cur = 0;
        max = ns_get_rand (ctrlBlock->numElements, ctrlBlock->data);
        set_count_block_control ((unsigned int *)state_var, max, cur);
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_COUNT: state_var=%d, Returning NS_CTRLLOGIC_DONE", state_var);
        return NS_CTRLLOGIC_DONE;
      }
      else
      {
        blockElements = ctrlBlock->blockElements;
        cur += 1;
        set_count_block_control ((unsigned int *)state_var, max, cur);
        *objIndex = blockElements[0].objIndex;
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_COUNT: Returning '%s' type of Object, objIndex=%d", show_obj_char_type(blockElements[0].isBlock),objIndex); 
        return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }
      break;
    }

    case NS_CTRLBLOCK_PERCENT:

    case NS_CTRLBLOCK_WEIGHT:
      if (*state_var) 
      {
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_WEIGHT: state_var = %d", *state_var);
        *state_var = 0;
        return NS_CTRLLOGIC_DONE;
      }
      else
      {
        blockElements = ctrlBlock->blockElements;
        idx = get_ctlLogic_distIdx(ctrlBlock);
        if(idx == -1)
          return NS_CTRLLOGIC_ERROR;
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_WEIGHT: idx is %d", idx);
        *state_var = 1;
        *objIndex = blockElements[idx].objIndex;
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_WEIGHT: objidx=%d", *objIndex);
        return ((blockElements[idx].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }

    case NS_CTRLBLOCK_DOWHILE:
      if (*state_var) 
      {
        api_ret = ns_get_int_val (ctrlBlock->attr);
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_DOWHILE: The ns_get_int_val() has returned the = %d", api_ret);
        if (api_ret) 
        {
          blockElements = ctrlBlock->blockElements;
          *objIndex = blockElements[0].objIndex; //can be only one child element
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_DOWHILE:ns_get_int_val (ctrlBlock->attr)=%d, *blockElements=%p, blockElements[0].isBlock=%d", ns_get_int_val (ctrlBlock->attr), (LogicBlockElement *)ctrlBlock->blockElements, (blockElements[0].isBlock));
          return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
        }
        else
        {
          *state_var = 0;
          NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_DOWHILE:*ns_get_int_val (ctrlBlock->attr)=%d", ns_get_int_val (ctrlBlock->attr));
          return NS_CTRLLOGIC_DONE;
        }
      }
      else
      {
        *state_var = 1;
        blockElements = ctrlBlock->blockElements;
        *objIndex = blockElements[0].objIndex; //can be only one child element
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_DOWHILE: blockElements[0].isBlock=%d", (blockElements[0].isBlock));
        return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }

    case NS_CTRLBLOCK_WHILE:
      //state_var not used in this case
      NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_WHILE The current value of NS varriable %s is=%d", ctrlBlock->attr, ns_get_int_val (ctrlBlock->attr));
      api_ret = ns_get_int_val (ctrlBlock->attr);
      if (api_ret)
      //if (ns_get_int_val (ctrlBlock->attr)) 
      {
        blockElements = ctrlBlock->blockElements;
        *objIndex = blockElements[0].objIndex; //can be only one child element
        NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_WHILE: Returning the '%s' type of object", (show_obj_char_type(blockElements[0].isBlock)));
        return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }
      else
      {
        return NS_CTRLLOGIC_DONE;
      }

    case NS_CTRLBLOCK_SWITCH:
     NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_SWITCH The value of state_var=%d", *state_var); 
      if (*state_var)
      {
        *state_var = 0;
        return NS_CTRLLOGIC_DONE;
      }
      else
      {
        blockElements = ctrlBlock->blockElements;
        *state_var = 1; // if we are doing this then it will nvr process next page
        val = ns_get_int_val (ctrlBlock->attr);
        NSDL2_RUNLOGIC(NULL, NULL, "The value of NS variable '%s' is %d, The number of elements in the current Block are %d", ctrlBlock->attr, val, ctrlBlock->numElements);
        numEl = ctrlBlock->numElements;
        numEl--; //Commented by anuj, since we need do decrement the numElements, if Element has been processed, need to be checked?????
        //ctrlBlock->numElements = --numEl; // Anuj
        for (i = 0; i < numEl; i++) 
        {
          if (blockElements[i].data == val) 
          {
            NSDL2_RUNLOGIC(NULL, NULL, "NS_CTRLBLOCK_SWITCH: The value of NS variable '%s' is %d and the value of blockElements[%d].data == %d", ctrlBlock->attr, val, i,  blockElements[i].data );
            *objIndex = blockElements[i].objIndex; 
            return ((blockElements[i].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
          }
        }
        *objIndex = blockElements[numEl].objIndex; //Make sure, default elemet always the last during generating code
        return ((blockElements[numEl].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
      }

    default:
      fprintf (stderr, "Error: bad block type %d\n", ctrlBlock->blockType);
      return NS_CTRLLOGIC_ERROR;
  }
}

// For printing the debug info of runlogic
static void print_control_logic(LogicBlockCtrl *ctrlBlock, int numBlocks)
{
  NSDL2_RUNLOGIC(NULL, NULL, "Method called, LogicBlockCtrl *ctrlBlock=%p, numBlocks=%d", ctrlBlock, numBlocks);
  IW_UNUSED(LogicBlockElement *blockElements);
  int i, j, num;

  NSDL2_RUNLOGIC(NULL, NULL, "Start = %d numBlock = %d", 0, numBlocks);

  for (i = 0; i < numBlocks; i++ )
  {
    switch (ctrlBlock[i].blockType)
    {
      case NS_CTRLBLOCK_SEQUENCE:
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: BlockType Seq: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType Seq: is_block=%d index=%d", blockElements[j].isBlock,  blockElements[j].objIndex);
        }
      break;

      case NS_CTRLBLOCK_COUNT:
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType Count: CountMin = %d Countmax=%lu", ctrlBlock[i].blockname, i, num, ctrlBlock[i].data);
      for (j = 0; j < 1; j++ )
      {
        NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType Count: is_block = %d index = %d", blockElements[j].isBlock, blockElements[j].objIndex);
      }
      break;

      case NS_CTRLBLOCK_PERCENT:
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType Pct: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType Pct: is_block = %d index = %d pct=%d", blockElements[j].isBlock,  blockElements[j].objIndex, blockElements[j].data);
        }
      break;

      case NS_CTRLBLOCK_WEIGHT:  // Anuj
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType WEIGHT: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType WEIGHT: is_block = %d index = %d Weight=%d", blockElements[j].isBlock,  blockElements[j].objIndex, blockElements[j].data);
        }
      break;

      case NS_CTRLBLOCK_DOWHILE: // Anuj
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType DOWHILE: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType DOWHILE: is_block = %d index = %d ", blockElements[j].isBlock,  blockElements[j].objIndex);
        }
      break;

      case NS_CTRLBLOCK_WHILE: // Anuj
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType WHILE: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType WHILE: is_block = %d index = %d ", blockElements[j].isBlock,  blockElements[j].objIndex);
        }
      break;

      case NS_CTRLBLOCK_SWITCH: // Anuj
        num = ctrlBlock[i].numElements;
        IW_UNUSED(blockElements = ctrlBlock[i].blockElements);
        NSDL2_RUNLOGIC(NULL, NULL, "BlockName %s: BlockId %d: blockType SWITCH: numElements = %d", ctrlBlock[i].blockname, i, num);
        for (j = 0; j < num; j++ )
        {
          NSDL2_RUNLOGIC(NULL, NULL, "Child of BlockType SWITCH: is_block = %d index = %d ", blockElements[j].isBlock,  blockElements[j].objIndex);
        }
      break;

      default:
        printf ("bad block %d\n", i);
    }
  }
  NSDL2_RUNLOGIC(NULL, NULL, "Method ended");
}

// End - Functions and variables used with in this file only.
/****************************************************************************/

/****************************************************************************/
// Start - Functions used outside of this file.

// This mthd will be called from the netstorm.c, just before the init_script() gets called
void 
init_process_logic (char *vuser_char_ptr)
{
  VUser *vuser_ptr = (VUser *)vuser_char_ptr;

  ProcessLogicControl *processLogic = (ProcessLogicControl *) vuser_ptr->pcRunLogic;
  NSDL2_RUNLOGIC(vuser_ptr, NULL, "Method called, char *processLogic=%p", processLogic);

  /* We must check ctrlBlock since there might exist one grou with run logic and other group without run logic in the same scenario */
  if ((processLogic == NULL) || ((LogicBlockCtrl *)vuser_ptr->sess_ptr->ctrlBlock == NULL)) return;

  processLogic->cur_depth = 1;
  processLogic->stack[0].blockNum = 0;
  processLogic->stack[0].state_var = init_state_var ((LogicBlockCtrl *)vuser_ptr->sess_ptr->ctrlBlock);
  NSDL2_RUNLOGIC(vuser_ptr, NULL, "Method ended");
}

int
get_next_page_using_cflow (char *vuser_ptr)
{
  VUser *vptr = (VUser *) vuser_ptr;
  ProcessLogicControl *pc = (ProcessLogicControl *) vptr->pcRunLogic;
  int objIndex = 0, ret = 0;
  LogicBlockCtrl *cb, *ctrlBlock = (LogicBlockCtrl *)vptr->sess_ptr->ctrlBlock;
  ProcessLogicStack *stack = pc->stack;
  int cur_depth = pc->cur_depth;

  NSDL2_RUNLOGIC(vptr, NULL, "Method called, Vuser *vptr=%p, The current depth is = %d", vptr, cur_depth);

  while (1)
  {
    /*
    when a block is processed:
    PAGE: got page_index, return, and keep stack as is
    BLOCK: Add Block to stack, incresae cur_depth, and contune;
    Done: if (cur_depth != 1) Remove block from Stack, reduce cur_dept and contnue, else, return -1 //iteration done
    ERR: Give error, retrun -1. kill all stack,
    */

    cb = &ctrlBlock[stack[cur_depth-1].blockNum]; // What we r doing here :Anuj
    ret = process_ctrl_logic_block (cb, &(stack[cur_depth-1].state_var), &objIndex);
    NSDL2_RUNLOGIC(vptr, NULL, "The process_ctrl_logic_block() is returning %s type of Object, Next objIndex: %d", show_obj_char_type(ret), objIndex);
    switch(ret) 
    {
      case NS_CTRLLOGIC_PAGE:
        NSDL2_RUNLOGIC(vptr, NULL, "Next Page :%d", objIndex);
        return objIndex;
      break;

      case NS_CTRLLOGIC_BLOCK:
        stack[cur_depth].blockNum = objIndex;
        stack[cur_depth].state_var = init_state_var (&ctrlBlock[objIndex]);
        pc->cur_depth = ++cur_depth;
        /*if ( g_max_runlogic_stack < cur_depth)
          g_max_runlogic_stack = cur_depth;*/
        NSDL2_RUNLOGIC(vptr, NULL, "Next Block: %d, max_depth %d, cur_depth=%d", objIndex, g_max_runlogic_stack, cur_depth);
      break;

      case NS_CTRLLOGIC_DONE:
        if (cur_depth == 1) 
        {
          NSDL2_RUNLOGIC(vptr, NULL, "The Control Logic has been Done");
          return -1;
        }
        else  
        {
          pc->cur_depth = --cur_depth;
          NSDL2_RUNLOGIC(vptr, NULL, "The Block has been Done: cur_depth is now %d", cur_depth);
        }
      break;
      
      default:
        fprintf (stderr, "ERROR: Processing runlogic failed\n");
        cur_depth = 1;
        pc->cur_depth = cur_depth;
        return(-1);
    }
  }
}

// for runlogic, Anuj 21/02, called from the create_page_script_ptr() in url.c
int create_cflow_ptr (char* sess_name, int sess_idx)
{
  char cmd_buffer[MAX_LINE_LENGTH];
  char sess_name_with_proj_subproj[MAX_LINE_LENGTH]="\0";
  char *runlogic_fname = ".runlogic.c"; 
  char so_fpath[MAX_FILE_NAME]; 
  char runlogic_fpath[MAX_FILE_NAME]; 
  void* handle;
  char* error;
  int return_value;
  struct stat stat_buf;
  int *numControlBlocks_ptr;
  int numCtrlBlocks=0;

  NSDL2_RUNLOGIC(NULL, NULL, "Method method, sess_name = %s, sess_idx = %d", sess_name, sess_idx);

  strcpy(sess_name_with_proj_subproj, get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "-"));
  //Previously taking with only script name
  //strcpy(sess_name_with_proj_subproj, get_sess_name_with_proj_subproj(sess_name));


  //sprintf(runlogic_fpath, "%s/scripts/%s/%s", g_ns_wdir, sess_name, runlogic_fname);
  //Previously taking with only script name
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(runlogic_fpath, "%s/%s/%s", GET_NS_TA_DIR(), get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), runlogic_fname);
  NSDL2_RUNLOGIC(NULL, NULL, "runlogic_fpath with NS_TA_DIR(%s) = %s", GET_NS_TA_DIR(), runlogic_fpath);
  if (stat(runlogic_fpath, &stat_buf))
  {
    gSessionTable[sess_idx].ctrlBlock = NULL; // setting CtrlBlock NULL if file not present
    return(0);
  }

  //sprintf(so_fpath, "%s/%s/%s.rl.so", g_ns_wdir, g_ns_tmpdir, sess_name);
  sprintf(so_fpath, "%s/%s/%s.rl.so", g_ns_wdir, g_ns_tmpdir, sess_name_with_proj_subproj);

  //complie the .runlogic.c file
  //sprintf(cmd_buffer, "gcc -g -m%d -fpic -shared -Wall -I%s/scripts -o %s %s", 
  //                     NS_BUILD_BITS, g_ns_wdir, so_fpath, runlogic_fpath);
  sprintf(cmd_buffer, "gcc -g -m%d -fpic -shared -Wall -I%s/ -o %s %s", 
                       NS_BUILD_BITS, GET_NS_TA_DIR(), so_fpath, runlogic_fpath);
  NSDL2_RUNLOGIC(NULL, NULL, "cmd_buffer = %s", cmd_buffer);
  //printf("cmd_buffer = %s\n", cmd_buffer);
  return_value = system(cmd_buffer);
  if (WEXITSTATUS(return_value) == 1)
  {
    NS_EXIT(-1, "Error in compiling the runlogic file, Session name=%s, Sess IDX=%d\n", 
                            get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), sess_idx);
  }

  handle = dlopen (so_fpath, RTLD_LAZY);
  if ((error = dlerror()))
  {
    NS_EXIT(-1, "%s\n", error);
  }

  numControlBlocks_ptr = (int *) dlsym (handle, "numCtrlBlocks");
  NSDL2_RUNLOGIC(NULL, NULL, "numControlBlocks_ptr = %p",  numControlBlocks_ptr);
  if ((error = dlerror()))
  {
    NS_EXIT(-1, "Error in getting the numCtrlBlocks: %s\n", error);
  }

  // Default value of switch block
  int *switchBlockDefaultValue_ptr = (int *) dlsym (handle, "switchBlockDefaultValue");
  NSDL2_RUNLOGIC(NULL, NULL, "switchBlockDefaultValue_ptr = %p", switchBlockDefaultValue_ptr);

  if ((error = dlerror()) == NULL) // Found switchBlockDefaultValue
  {
    numSwitchBlockDefaultValue = *switchBlockDefaultValue_ptr;
    NSDL2_RUNLOGIC(NULL, NULL, "Setting default value for switch block to %d", numSwitchBlockDefaultValue);
  }

  numCtrlBlocks = *numControlBlocks_ptr;

  if(numCtrlBlocks == 0)
  {
    NSDL1_RUNLOGIC(NULL, NULL, "The Number of control blocks is 0, Session name: %s\n", 
                                  get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"));
    return 0;
  }

  if ( g_max_runlogic_stack < numCtrlBlocks)
    g_max_runlogic_stack = numCtrlBlocks;

  gSessionTable[sess_idx].ctrlBlock = dlsym(handle, "ctrlBlock");
  if ((error = dlerror()))
  {
    NS_EXIT(-1, "Error in getting the ctrlBlock: %s\n", error);
  }

  if (gSessionTable[sess_idx].ctrlBlock)
    print_control_logic ( (LogicBlockCtrl *)gSessionTable[sess_idx].ctrlBlock, numCtrlBlocks);

  NSDL2_RUNLOGIC(NULL, NULL, "The g_max_runlogic_stack = %d, sess_name = %s, sess_idx = %lu,ctrlBlock = %p, numCtrlBlocks=%d", g_max_runlogic_stack, get_sess_name_with_proj_subproj_int(sess_name, sess_idx, "/"), sess_idx, &gSessionTable[sess_idx].ctrlBlock, numCtrlBlocks);

  // calling init_control_logic for the session, from the runlogic.c
  if (init_control_logic(numCtrlBlocks, (LogicBlockCtrl *)gSessionTable[sess_idx].ctrlBlock))
    NS_EXIT(-1, "init_control_logic() failed");

  NSDL1_RUNLOGIC(NULL, NULL, "Method Ended Sucessfully");

  return 0; 
}

// This will allocate the memory for the runlogic (ProcessLogicControl and ProcessLogicStack)
// called from the allocate_user_tables() in netstorm.c
void alloc_runlogic_stack(char *vuser_ptr, int num_users)
{
  VUser *vuser_chunk = (VUser *) vuser_ptr;
  NSDL2_RUNLOGIC(vuser_chunk, NULL, "Method called, Vuser* vuser_chunk=%p, num_users=%d", vuser_chunk, num_users);
  int i;
  ProcessLogicControl *plc_Ptr;
  ProcessLogicStack *pls_Ptr;

  if(g_max_runlogic_stack == 0) return;

  MY_MALLOC(plc_Ptr, (num_users * sizeof (ProcessLogicControl)), "alloc_runlogic_stack() - Allocating memory for ProcessLogicControl", -1);

  MY_MALLOC(pls_Ptr, (num_users * g_max_runlogic_stack * sizeof (ProcessLogicStack)), "alloc_runlogic_stack() - Allocating memory for ProcessLogicStack", -1);

  for (i = 0; i < num_users; i++)
  {
    NSDL2_RUNLOGIC(vuser_chunk, NULL, "setting the (plc_Ptr + i)->stack for user=%d, total users are=%d", i, num_users);
    (plc_Ptr + i)->stack = pls_Ptr + (i * g_max_runlogic_stack);
    vuser_chunk[i].pcRunLogic = (char *)(plc_Ptr + i);
  }
  NSDL2_RUNLOGIC(vuser_chunk, NULL, "Method ended");
}

/****************************************************************************/
// End - Functions used outside of this file.

// End of file

