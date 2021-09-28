
#include "../../include/runlogic.h"


LogicBlockElement elementsCtrBlock_1 [] = {
	{0, 0, 0},
	{1, 1, 0},
};

LogicBlockElement elementsCtrBlock_2 [] = {
	{1, 2, 0},
};

LogicBlockElement elementsCtrBlock_3 [] = {
	{0, 11, 3},
	{0, 12, 4},
	{0, 8, 5},
	{0, 2, 6},
	{0, 1, 7},
	{0, 7, 8},
	{0, 4, 9},
	{0, 9, 10},
	{0, 10, 11},
	{0, 3, 12},
	{0, 6, 0},
};

LogicBlockCtrl ctrlBlock[] = {
	{ "TR069Main", NS_CTRLBLOCK_SEQUENCE, 2, elementsCtrBlock_1, 0, ""},
	{ "TR069CheckSessionDone", NS_CTRLBLOCK_WHILE, 1, elementsCtrBlock_2, 0, "IsSessionInProgress"},
	{ "TR069ExecuteRPC", NS_CTRLBLOCK_SWITCH, 11, elementsCtrBlock_3, 0, "RpcMethod"},
};

int numCtrlBlocks = 3;
