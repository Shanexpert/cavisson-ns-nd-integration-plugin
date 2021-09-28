#include <stdio.h>
#include <stdlib.h>

//blockTypes

#define NS_CTRLBLOCK_SEQUENCE 0
#define NS_CTRLBLOCK_COUNT 1
#define NS_CTRLBLOCK_PERCENT 2
#define NS_CTRLBLOCK_WEIGHT 3
#define NS_CTRLBLOCK_DOWHILE 4
#define NS_CTRLBLOCK_WHILE 5
#define NS_CTRLBLOCK_CASE 6

//Return of processBlock
#define NS_CTRLLOGIC_PAGE 0
#define NS_CTRLLOGIC_BLOCK 1
#define NS_CTRLLOGIC_DONE 2
#define NS_CTRLLOGIC_ERROR 3

typedef  struct {
	int isBlock;
	int objIndex;
	int data;  //only used for NS_CTRLBLOCK_PERCENT & NS_CTRLBLOCK_WEIGHT  and Case
} LogicBlockElement;

typedef struct {
	char *blockName;
	int blockType;
	int numElements;
	LogicBlockElement *blockElements;
	int data;
	/* elements have overloaded meanings
	block type 		numElements 	data
	NS_CTRLBLOCK_SEQUENCE 	as-is		not-used
	NS_CTRLBLOCK_COUNT	min-count	max-count
	NS_CTRLBLOCK_PERCENT	as-is		not-used
	NS_CTRLBLOCK_WEIGHT	as-is		not-used
	NS_CTRLBLOCK_DOWHILE	not-used	int func(void)
	NS_CTRLBLOCK_WHILE	not-used	int func(void)
	NS_CTRLBLOCK_CASE		as-is		int func(void)
	
	Note: numBlockElements is always 1 for NS_CTRLBLOCK_COUNT, NS_CTRLBLOCK_DOWHILE, NS_CTRLBLOCK_DOWHILE
	*/
	char *attr;
} LogicBlockCtrl;

/*typedef struct {
	int mainBlock;
	int numBlock;
	LogicBlockCtrl *ctrlBlocks;
} ControlLogic;*/

typedef struct {
	int blockNum;
	int state_var;
} ProcessLogicStack;

typedef struct {
	ProcessLogicStack *stack;
	int cur_depth;
	LogicBlockCtrl *ctrlBlocks;
	//ControlLogic *controlLogic;
} ProcessLogicControl;

LogicBlockElement elementsCtrBlock_1[] = {
		{ 0, 0 },
		{ 0, 1 },
		{ 1, 4 },
		{ 0, 8 },
	};

LogicBlockElement elementsCtrBlock_2[] = {
		{ 0, 2 },
		{ 0, 3 },
		{ 0, 4 },
	};

LogicBlockElement elementsCtrBlock_3[] = {
		{ 0, 5 },
		{ 0, 6 },
		{ 0, 7 },
	};

LogicBlockElement elementsCtrBlock_4[] = {
		{ 1, 1, 25 },
		{ 1, 2, 75 },
	};

LogicBlockElement elementsCtrBlock_5[] = {
		{ 1, 3 },
	};

LogicBlockCtrl ctrlBlock[] = {
		{ "Blk1", NS_CTRLBLOCK_SEQUENCE, sizeof(elementsCtrBlock_1)/sizeof(LogicBlockElement), elementsCtrBlock_1, ""},
		{ "Blk2", NS_CTRLBLOCK_SEQUENCE, sizeof (elementsCtrBlock_2)/sizeof (LogicBlockElement), elementsCtrBlock_2, ""},
		{ "Blk3", NS_CTRLBLOCK_SEQUENCE, sizeof (elementsCtrBlock_3)/sizeof (LogicBlockElement), elementsCtrBlock_3, ""},
		{ "Blk4", NS_CTRLBLOCK_PERCENT, sizeof (elementsCtrBlock_4)/sizeof (LogicBlockElement), elementsCtrBlock_4, ""},
		{ "Blk5", NS_CTRLBLOCK_COUNT, 5, elementsCtrBlock_5, 5, ""},
	};

//ControlLogic controlLogic = { 0, sizeof(ctrlBlock)/sizeof(LogicBlockCtrl), ctrlBlock };
int numControlBlocks = sizeof(ctrlBlock)/sizeof(LogicBlockCtrl);

static int inline
ns_get_rand (int min, int max)
{
	if (min == max)
	    return min;

	return ((random()%(max-min+1)) + min);
}

static void inline
get_count_block_control (unsigned int state_var, unsigned short *max, unsigned short *cur) 
{
	*cur = (short) (state_var & 0x0000FFFF);
	*max = (short) ((state_var & 0xFFFF0000) >> 16 );
}

static void inline
set_count_block_control (unsigned int *state_var, unsigned short max, unsigned short cur) 
{
unsigned int num = max;

	//printf ("set_count_block_control 1: 0x%x\n", num);
	num = num << 16;
	//printf ("set_count_block_control 2: 0x%x\n", num);
	num |= (unsigned int)cur;
	//printf ("set_count_block_control 3: 0x%x\n", num);
	*state_var = num;
}

static int inline
init_state_var (LogicBlockCtrl *ctrlBlock)
{
unsigned short cur, max;
int svar;

	if (ctrlBlock->blockType != NS_CTRLBLOCK_COUNT)
	    return 0;

	cur = 0;
	max = ns_get_rand (ctrlBlock->numElements, ctrlBlock->data);
	//printf ("init:cur=%hd, max=%hd\n", cur, max);
	set_count_block_control ((unsigned int *)&svar, max, cur);
	//printf ("init:svar=%d cur=%hd, max=%hd\n", svar, cur, max);
	return svar;
}

void inline
init_process_logic (ProcessLogicControl *processLogic)
{
//int numBlocks, type, i;
//ControlLogic *controlLogic = processLogic->controlLogic;
//LogicBlockCtrl *ctrlBlock = controlLogic->ctrlBlocks;
//update for Vuser *, init ctrlBlck from session table
LogicBlockCtrl *ctrlBlock = processLogic->ctrlBlocks;

	processLogic->cur_depth = 1;
	//processLogic->stack[0].blockNum = controlLogic->mainBlock;
	processLogic->stack[0].blockNum = 0;
	//processLogic->stack[0].state_var = init_state_var (&ctrlBlock[controlLogic->mainBlock]);
	processLogic->stack[0].state_var = init_state_var (&ctrlBlock[0]);
}

//Called for each script initialization.
//return the max stack depth need for process logic
int 
//init_control_logic (ControlLogic *controlLogic)
init_control_logic (int numBlocks, LogicBlockCtrl *ctrlBlock)
{
//int numBlocks = controlLogic->numBlock;
//LogicBlockCtrl *cb, *ctrlBlock = controlLogic->ctrlBlocks;
LogicBlockCtrl *cb;
int i, type, j, num, sum;
LogicBlockElement *elementsCtrBlock;
int blockType;
int numElements;
LogicBlockElement *blockElements;
ProcessLogicStack stack[numBlocks];
int max_depth=1, cur_depth=1;
ProcessLogicControl pc;
int svar, ret; 
//int k=0;

	//Set individidual weights or pct to cumulatices for PCT or WEIGHT block types
	for (i = 0; i < numBlocks; i++ ) {
	    cb = &ctrlBlock[i];
	    type = cb->blockType;
	    if ((type == NS_CTRLBLOCK_PERCENT) || (type == NS_CTRLBLOCK_WEIGHT)) {
		elementsCtrBlock = cb->blockElements;
		num = cb->numElements; 
		sum = 0;
		for (j = 0; j < num; j++) {
			sum += elementsCtrBlock[j].data;
			elementsCtrBlock[j].data = sum;
		}
	        cb->data = sum;
	        if ((type == NS_CTRLBLOCK_PERCENT) && (sum != 100)) {
			printf ("Percentage summation for Percent Logic Block must add to 100 (%d)\n", sum);
			exit(1);
		}
	    }
	}

	//Find max depth
	pc.stack = stack;
	pc.ctrlBlocks = ctrlBlock;
	//pc.controlLogic = controlLogic;
        init_process_logic (&pc);
	while (1) {
#if 0
	   if (k++ > 100) {
		printf("looks like in loop\n");
	   	return max_depth;
	   }
#endif
	/*
	when a block is processed: 
	PAGE: got page_index, return, and keep stack as is
	BLOCK: Add Block to stack, incresae cur_depth, and contune;
	Done: if (cur_depth != 1) Remove block from Stack, reduce cur_dept and contnue, 
	      else, return -1 //iteration done
	ERR: Give error, retrun -1. kill all stack, 
	*/

	   cb = &ctrlBlock[stack[cur_depth-1].blockNum];    
	   ret = processCtrlLogicBlock (cb, &(stack[cur_depth-1].state_var), &i);
	   switch(ret) {
	   case NS_CTRLLOGIC_PAGE:
	     printf ("Next Page :%d \n", i);
	     break;
	   case NS_CTRLLOGIC_BLOCK:
	     stack[cur_depth].blockNum = i;
	     stack[cur_depth].state_var = 0; //better init with block data
	     stack[cur_depth].state_var = init_state_var (&ctrlBlock[i]);
	     cur_depth++;
	     if (max_depth < cur_depth) max_depth = cur_depth;
	     printf ("Next Block: %d max_depth %d cur_depth=%d\n", i, max_depth, cur_depth);
	     break;
	   case NS_CTRLLOGIC_DONE:
	     cur_depth--;
	     if (cur_depth == 0)
		return max_depth;
	     break;
	   default:
		printf ("ERROR ret=%d\n", ret);
		exit(1);
	   }
	}
}

static int inline 
get_ctlLogic_distIdx (LogicBlockCtrl *ctrlBlock)
{
LogicBlockElement *blockElements = ctrlBlock->blockElements;
int numElements = ctrlBlock->numElements;
int max_val = ctrlBlock->data;
int j, rnum;

	rnum = random()%max_val;
	rnum++;

	for (j=0; j < numElements; j++) {
	    if (rnum <= blockElements[j].data)
		break;
	}

	return j;
}

int
processCtrlLogicBlock (LogicBlockCtrl *ctrlBlock, int *state_var, int *objIndex)
{
LogicBlockElement *blockElements;
unsigned int idx, val, done, i, numBlocks;

	//printf ("proicess %d type block\n", ctrlBlock->blockType);
	switch (ctrlBlock->blockType) {
	case NS_CTRLBLOCK_SEQUENCE: {
	    idx = (unsigned int)*state_var;
	    if (idx >= ctrlBlock->numElements) {
		*state_var = 0;
		return NS_CTRLLOGIC_DONE;
	    } else {
		blockElements = ctrlBlock->blockElements;
		*state_var = idx + 1;
		*objIndex = blockElements[idx].objIndex;
		return ((blockElements[idx].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
	    }
	    break;
	    }
	case NS_CTRLBLOCK_COUNT: {
	    unsigned short cur, max;
	    idx = (unsigned int)*state_var;
	    get_count_block_control (idx, &max, &cur); 
	    //printf ("idx = %d cur=%hd and max=%hd\n", idx, cur, max);
	    if (cur >= max) {
		cur = 0;
		max = ns_get_rand (ctrlBlock->numElements, ctrlBlock->data);
		set_count_block_control ((unsigned int *)state_var, max, cur);
		return NS_CTRLLOGIC_DONE;
	    } else {
		blockElements = ctrlBlock->blockElements;
		cur += 1;
		set_count_block_control ((unsigned int *)state_var, max, cur);
		*objIndex = blockElements[0].objIndex;
		return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
	    }
	    break;
	    }
	case NS_CTRLBLOCK_PERCENT:
	case NS_CTRLBLOCK_WEIGHT:
	    //printf ("state_var = %d\n", *state_var);
	    if (*state_var) {
		*state_var = 0;
		return NS_CTRLLOGIC_DONE;
	    } else {
		blockElements = ctrlBlock->blockElements;
	        idx = get_ctlLogic_distIdx(ctrlBlock);
		//printf ("idx is %d\n", idx);
		*state_var = 1;
	        *objIndex = blockElements[idx].objIndex;
		//printf ("isBlock=%d onidx=%d \n", blockElements[idx].isBlock, blockElements[idx].objIndex);
	        return ((blockElements[idx].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
	    }
	case NS_CTRLBLOCK_DOWHILE:
	    if (*state_var) {
		if (ns_get_int_val (ctrlBlock->attr)) {
	            blockElements = ctrlBlock->blockElements;
	            *objIndex = blockElements[0].objIndex; //can be only one child element
	            return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
		} else {
		    *state_var = 0;
		    return NS_CTRLLOGIC_DONE;
		}
	    } else {
		*state_var = 1;
	        blockElements = ctrlBlock->blockElements;
	        *objIndex = blockElements[0].objIndex; //can be only one child element
	        return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
	    }
	case NS_CTRLBLOCK_WHILE:
	    //state_var not used in this case
	    if (ns_get_int_val (ctrlBlock->attr)) {
	        blockElements = ctrlBlock->blockElements;
	        *objIndex = blockElements[0].objIndex; //can be only one child element
	        return ((blockElements[0].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
	    } else {
	       return NS_CTRLLOGIC_DONE;
	    }
	case NS_CTRLBLOCK_CASE:
	    if (*state_var) {
		*state_var = 0;
		return NS_CTRLLOGIC_DONE;
	    } else {
		blockElements = ctrlBlock->blockElements;
		*state_var = 1;
	 	val = ns_get_int_val (ctrlBlock->attr);
	 	numEl = ctrlBlock->numElements;
		numEl--;
		for (i = 0; i < numEl; i++) {
		    if (blockElements[i].data == val) {
	                *objIndex = blockElements[i].objIndex; //can be only one child element
	                return ((blockElements[i].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
		    }
	            *objIndex = blockElements[numEl].objIndex; //Make sure, default elemet always the last during generating code
	            return ((blockElements[numEl].isBlock)? NS_CTRLLOGIC_BLOCK: NS_CTRLLOGIC_PAGE);
                }
 	    }
	default:
	    printf ("bad block type %d\n", ctrlBlock->blockType);
	    return NS_CTRLLOGIC_ERROR;
	}
}

main()
{
//ControlLogic *cl = &controlLogic;
//LogicBlockCtrl *ctrlBlocks = cl->ctrlBlocks;
LogicBlockCtrl *ctrlBlocks = ctrlBlock;
//int numBlocks = cl->numBlock;
int numBlocks = numControlBlocks;
LogicBlockElement *blockElements;
int i, j, num;

	printf ("Start = %d numBlock = %d\n", 0, numBlocks);
	//printf ("Start = %d numBlock = %d\n", cl->mainBlock, numBlocks);
	for (i = 0; i < numBlocks; i++ ) {
	    switch (ctrlBlocks[i].blockType) { 
	    case NS_CTRLBLOCK_SEQUENCE:
		num = ctrlBlocks[i].numElements;
	 	blockElements = ctrlBlocks[i].blockElements;
	        printf ("Block %d: blockType:Seq numElements = %d\n", i, num);
		for (j = 0; j < num; j++ ) {
		    printf ("is_block = %d index = %d\n", blockElements[j].isBlock,  blockElements[j].objIndex);
		}
	    break;
	    case NS_CTRLBLOCK_COUNT:
		num = ctrlBlocks[i].numElements;
	 	blockElements = ctrlBlocks[i].blockElements;
	        printf ("Block %d: blockType:Count CountMin = %d Countmax=%lu\n", i, num, ctrlBlocks[i].data);
		for (j = 0; j < 1; j++ ) {
		    printf ("is_block = %d index = %d\n", blockElements[j].isBlock,  blockElements[j].objIndex);
		}
	    break;
	    case NS_CTRLBLOCK_PERCENT:
		num = ctrlBlocks[i].numElements;
	 	blockElements = ctrlBlocks[i].blockElements;
	        printf ("Block %d: blockType:Pct numElements = %d\n", i, num);
		for (j = 0; j < num; j++ ) {
		    printf ("is_block = %d index = %d pct=%d\n", blockElements[j].isBlock,  blockElements[j].objIndex, blockElements[j].data);
		}
	    break;
	    case NS_CTRLBLOCK_WEIGHT:
	    case NS_CTRLBLOCK_DOWHILE:
	    case NS_CTRLBLOCK_WHILE:
	    case NS_CTRLBLOCK_CASE:
	    default:
		printf ("bad block %d\n", i);
	    }
	}
	
	//i = init_control_logic (cl);
	i = init_control_logic (numBlocks, ctrlBlocks);
	printf ("Init control: max_depth = %d\n", i);
}

//returns next page or -1 on last page
int
get_next_page_using_cflow  (Vuser *vptr)
{
ProcessLogicControl *pc = vptr->pcRunLogic;
int i;
LogicBlockCtrl *cb, *ctrlBlock = pc->ctrBlock;
ProcessLogicStack *stack = pc->stack;
int cur_depth= pc->cur_depth;


    while (1) 
    {
	/*
	when a block is processed: 
	PAGE: got page_index, return, and keep stack as is
	BLOCK: Add Block to stack, incresae cur_depth, and contune;
	Done: if (cur_depth != 1) Remove block from Stack, reduce cur_dept and contnue, 
	      else, return -1 //iteration done
	ERR: Give error, retrun -1. kill all stack, 
	*/

	   cb = &ctrlBlock[stack[cur_depth-1].blockNum];    
	   ret = processCtrlLogicBlock (cb, &(stack[cur_depth-1].state_var), &i);
	   switch(ret) {
	   case NS_CTRLLOGIC_PAGE:
	     if (debug) printf ("Next Page :%d \n", i);
	     return i;
	     break;
	   case NS_CTRLLOGIC_BLOCK:
	     stack[cur_depth].blockNum = i;
	     /*stack[cur_depth].state_var = 0; //better init with block data */
	     stack[cur_depth].state_var = init_state_var (&ctrlBlock[i]);
	     cur_depth++;
	     if (max_depth < cur_depth) max_depth = cur_depth;
	     if (debug) printf ("Next Block: %d max_depth %d cur_depth=%d\n", i, max_depth, cur_depth);
	     break;
	   case NS_CTRLLOGIC_DONE:
	     if (cur_dept == 1) {
	        if (debug) printf ("Done\n");
		return -1;
	     } else  {
		cur_dept--;
	        if (debug) printf ("cur_depth is now %d\n", cur_depth);
	     }
	     break;
	   default:
		printf ("ERROR: Processing runlogic failed, %d\n", ret);
		cur_depth = 1;
		return(-1);
	   }
	}
      }
    }

}
/*********************************************************************************************
Integration Notes:

create_page_script_ptr() in url.c returns -1 on failure and 0 on sucess.
It will be modified as follows:

0.  we can eliminate mainBlock of ControlLogic. starting block must  always be set
be 0 (first block).

1. Add a new filed on g_sessionTable (for both memory and shm tables), 
LogicBlockCtrl *scrCtrlBlocks;

2. if a runlogic datastructures for a script is created, it will be created in 
the file .runlogic.c (hidden file). This will include runlogic.h (which include
all the data structures defined in this file. runlogic.h will be exported 
to include dir of export tree).
	
3. All runlogic related functions will be defined in new file runlogic.c

4. define one global var 
int g_max_runlogic_stack

3. define a new function 
int create_cflow_ptr( (char* sess_name, char* c_file_name, int sess_idx)
 on the lines of
int
create_page_script_ptr(char* sess_name, char* c_file_name, int sess_idx)

create_cflow_ptr() will be called from within create_page_script_ptr() at the end
of it.

create_cflow_ptr() will make a shlib (similar to done by create_page_script_ptr) 
for .runlogic.c. Firstly, check, if .runlogic.c exist.
If none, do not create shm, and mark the scrCtrlBlocks field to NULL.
If yes, create shlib and resolve the symbols, ctrlBlock and numControlBlocks 

printf main for loop of the "main" function as the debug info for runlogic.

call the first for loop of the init_control_logic( numBlocks, ctrlBlocks) to set the 
cumlative value and also to check, switch has default statment.


set max runlogic depth,

if (g_max_runlogic_stack < numControlBlocks)
    g_max_runlogic_stack = numControlBlocks;
	
save the scrCtrlBlocks of session table to ctrlBlock.

4. make sure to copy the scrCtrlBlocks elemnt from ememory to shm session table

5. add  ProcessLogicControl * pcRunlogic on Vuser structure.

6. define fllowing function and call it from alloc_users (netstorm.c)  at the end just before return.
void alloc_runlogic_stack( Vuser* vuser_chunk, int num_users) 
{
    if (g_max_runlogic_stack) {
	//alloc num_user * g_max_runlogic_stack * sizeof (ProcessLogicStack);
	//alloc num_users * size of (ProcessLogicControl)
	//stack of ProcessLogicControl using earlier alloc
	//set  pcRunLogic of Vuser
     }
}

7. just before calling init_script, call init_process_logic (Vuser *)  if vptr->pcRunlogic
// above function is taking ProcessLogicControl *, can take this from Vuser *
8. call get_next_page_using_cflow () if session's scrCtrlBlocks is not NULL

next_page = get_next_page_using_cflow (Vuser *, int nextpg);

return next_page
or -1 when done
or -2 when exception

**********************************************************************************************/
