#ifndef LIB_PHASE_3_H_INCLUDED
#define LIB_PHASE_3_H_INCLUDED

/* -- NECESSARY LIBRARIES -- */
#include "../../phase2/headers/lib.h"

/* -- MACROS -- */



/* -- VARIABLES -- */
extern state_t uProcState[UPROCMAX];
extern support_t supStruct[UPROCMAX];
extern swap_t swapPoolTable[POOLSIZE];

extern pcb_PTR printerPcbs[DEVPERINT];
extern pcb_PTR terminalPcbs[DEVPERINT];
extern pcb_PTR swap_mutex, testPcb;

extern void test();

/* -- FUNCTIONS PROTOTYPES -- */
/* init module */
void initSwapTable();
void initUProc(int asid);
void initSupportStruct(int asid);
void acq2relMutex();



/* utils (from p2test) */
extern pcb_PTR create_process(state_t *s, support_t *sup);

#endif





