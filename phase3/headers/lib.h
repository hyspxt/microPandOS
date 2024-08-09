#ifndef LIB_PHASE_3_H_INCLUDED
#define LIB_PHASE_3_H_INCLUDED

/* -- NECESSARY LIBRARIES -- */
#include "../../phase2/headers/lib.h"

/* -- MACROS -- */



/* -- VARIABLES -- */
extern state_t uProcState[UPROCMAX];
extern support_t supStruct[UPROCMAX];
extern swap_t swapPoolTable[POOLSIZE];

extern pcb_PTR printerPcbs[UPROCMAX];
extern pcb_PTR terminalPcbs[UPROCMAX];
extern pcb_PTR sstPcbs[UPROCMAX];
extern pcb_PTR uproc[UPROCMAX];
extern pcb_PTR swap_mutex, testPcb, mutex_recvr;

extern void test();

/* -- FUNCTIONS PROTOTYPES -- */
/* init module */
void initUProc();
void initSupportStruct();
void initSST();
void mutex();
memaddr nextFrame(memaddr);
void initDeviceProc(int, int);
extern void (*terminals[8])();
extern void (*printers[8])();

/* SST module */
void terminate(int, int);
void writePrinter(int, sst_print_PTR);
void writeTerminal(int, sst_print_PTR);
void SST();
unsigned int SSTRequest(pcb_PTR, unsigned int, void*, int);

/* vmSupport module */
// void uTLB_RefillHandler();
int pick_frame();
int isFrameFree(int);
void interrupts_off();
void interrupts_on();
void updateTLB(pteEntry_t);
int flashOp(int, int, memaddr, int);
void initSwapStructs(int);
void pager();
void askMutex();
void uTLB_RefillHandler();

/* sysSupport module */
void supExceptionHandler();
void supSyscallHandler(state_t*);
void programTrapHandler();

/* utils (from p2test) */
pcb_PTR create_process(state_t*, support_t*);
support_t *getSupStruct();
void *printDevice(int, int);
void sendKillReq();

#endif





