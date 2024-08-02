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
void initUProc(int);
void initSupportStruct(int);
void mutex();
memaddr nextFrame(memaddr);
void initDeviceProc(int asid, int devNo);

/* SST module */
void terminate();
void writePrinter(int, sst_print_PTR);
void writeTerminal(int, sst_print_PTR);
void SST();
unsigned int SSTRequest(pcb_PTR, unsigned int, void*, int);

/* vmSupport module */
void uTLB_RefillHandler();
void initSwapStructs(int);
void pager();

/* sysSupport module */
void supExceptionHandler();
void supSyscallHandler(state_t*);
void programTrapHandler(state_t*);

/* utils (from p2test) */
pcb_PTR create_process(state_t*, support_t*);

#endif





