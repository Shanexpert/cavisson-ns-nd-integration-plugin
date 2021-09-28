#ifndef _RUNLOGIC_H
#define _RUNLOGIC_H

//blockTypes
#define NS_CTRLBLOCK_SEQUENCE 0
#define NS_CTRLBLOCK_COUNT    1
#define NS_CTRLBLOCK_PERCENT  2
#define NS_CTRLBLOCK_WEIGHT   3
#define NS_CTRLBLOCK_DOWHILE  4
#define NS_CTRLBLOCK_WHILE    5
#define NS_CTRLBLOCK_SWITCH   6

//Return of processBlock
#define NS_CTRLLOGIC_PAGE  0
#define NS_CTRLLOGIC_BLOCK 1
#define NS_CTRLLOGIC_DONE  2
#define NS_CTRLLOGIC_ERROR 3


typedef  struct 
{
  int isBlock;         // Object is a Page or block
  int objIndex;        // Index for page or block
  int data;            // Only used for PERCENT, WEIGHT & SWITCH Block, Refer to below table
} LogicBlockElement;

  /* These variables have overloaded meanings
    block type               data         
  NS_CTRLBLOCK_SEQUENCE    not-used      
  NS_CTRLBLOCK_COUNT       max-count     
  NS_CTRLBLOCK_PERCENT          
  NS_CTRLBLOCK_WEIGHT            
  NS_CTRLBLOCK_DOWHILE     not-used      
  NS_CTRLBLOCK_WHILE       not-used      
  NS_CTRLBLOCK_SWITCH            
  */

typedef struct
{
  char *blockname;     // Block Name
  int blockType;       // Block Id (seq , cnt etc)
  int numElements;     // Number of children under this node
  LogicBlockElement *blockElements;
  int data;           // Refer to table below
  char *attr;          // Name of NS variable, presently used for while, do while, switch Blocks

  /* These variables have overloaded meanings
    block type            numElements    data          attr
  NS_CTRLBLOCK_SEQUENCE    as-is        not-used      not-used
  NS_CTRLBLOCK_COUNT      min-count     max-count     not-used
  NS_CTRLBLOCK_PERCENT     as-is        not-used      not-used
  NS_CTRLBLOCK_WEIGHT      as-is        not-used      not-used
  NS_CTRLBLOCK_DOWHILE       1          not-used      NS Var Name
  NS_CTRLBLOCK_WHILE         1          not-used      NS Var Name
  NS_CTRLBLOCK_SWITCH      as-is        not-used      NS Var Name
  Note: numBlockElements is always 1 for NS_CTRLBLOCK_COUNT, NS_CTRLBLOCK_DOWHILE, NS_CTRLBLOCK_DOWHILE
  */
} LogicBlockCtrl;

typedef struct 
{
  int blockNum;                // No of blocks in the Stack
  unsigned int state_var;      // Refer to below table
} ProcessLogicStack;

  /* This variables have overloaded meanings
    block type              state_var        Meaning
  NS_CTRLBLOCK_SEQUENCE       0,1,2....    No of page or block to be executed next time 
  NS_CTRLBLOCK_COUNT         max/cur       Left 2bytes-max, Right 2bytes-cur
  NS_CTRLBLOCK_PERCENT        0/1          1-done, 0-remaining
  NS_CTRLBLOCK_WEIGHT         0/1          1-done, 0-remaining    
  NS_CTRLBLOCK_DOWHILE        0/1          1-done, 0-remaining
  NS_CTRLBLOCK_WHILE          0/1          1-done, 0-remaining
  NS_CTRLBLOCK_SWITCH         0/1          1-done, 0-remaining
  */

typedef struct 
{
  ProcessLogicStack *stack;
  int cur_depth;               // Current depth
  //LogicBlockCtrl *ctrlBlock; // This is already available in VUser struct
} ProcessLogicControl;

extern void init_process_logic (char *vuser_char_ptr);//called from netstorm.c before init_script
extern int create_cflow_ptr (char* sess_name, int sess_idx);//url.c from create_page_script_ptr()
extern void alloc_runlogic_stack (char *vuser_ptr, int num_users);//netstrom.c from allocate_user_tables()
extern int get_next_page_using_cflow (char *vuser_ptr);//netstrom.c, before init and next page

#endif

