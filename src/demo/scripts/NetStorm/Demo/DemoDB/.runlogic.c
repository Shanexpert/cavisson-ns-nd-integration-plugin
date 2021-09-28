
#include "../../include/runlogic.h"


LogicBlockElement elementsCtrBlock_1 [] = {
	{1, 1, 0},
	{1, 2, 0},
	{1, 3, 0},
	{1, 4, 0},
};

LogicBlockElement elementsCtrBlock_2 [] = {
	{0, 0, 0},
};

LogicBlockElement elementsCtrBlock_3 [] = {
	{0, 1, 0},
	{0, 2, 0},
};

LogicBlockElement elementsCtrBlock_4 [] = {
	{0, 3, 0},
	{0, 2, 0},
};

LogicBlockElement elementsCtrBlock_5 [] = {
	{0, 4, 0},
};

LogicBlockCtrl ctrlBlock[] = {
	{ "DemoDB", NS_CTRLBLOCK_SEQUENCE, 4, elementsCtrBlock_1, 0, ""},
	{ "DBCreate", NS_CTRLBLOCK_SEQUENCE, 1, elementsCtrBlock_2, 0, ""},
	{ "DBInsert", NS_CTRLBLOCK_SEQUENCE, 2, elementsCtrBlock_3, 0, ""},
	{ "DBUpdate", NS_CTRLBLOCK_SEQUENCE, 2, elementsCtrBlock_4, 0, ""},
	{ "DBInsertALL", NS_CTRLBLOCK_SEQUENCE, 1, elementsCtrBlock_5, 0, ""},
};

int numCtrlBlocks = 5;

int switchBlockDefaultValue = -1;
