#ifndef LIB_PHASE_3_H_INCLUDED
#define LIB_PHASE_3_H_INCLUDED

/* -- NECESSARY LIBRARIES -- */
#include "../../phase2/headers/lib.h"

/* -- MACROS -- */



/* -- VARIABLES -- */
extern state_t uProcState[UPROCMAX];
extern support_t pageTable[UPROCMAX];
extern swap_t swapPoolTable[POOLSIZE];

extern pcb_PTR printerPcbs[DEVPERINT];
extern pcb_PTR terminalPcbs[DEVPERINT];
extern pcb_PTR swap_mutex, testPcb;

extern void test();

/* -- FUNCTIONS PROTOTYPES -- */
/* init module */
void initSwapTable();

#endif





