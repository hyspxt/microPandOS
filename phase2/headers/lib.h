#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

/* -- NECESSARY LIBRARIES -- */
#include "/usr/include/umps3/umps/libumps.h"
#include "/usr/include/umps3/umps/arch.h"
#include "/usr/include/umps3/umps/const.h"
#include "/usr/include/umps3/umps/cp0.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

extern void klog_print(char *);
extern void klog_print_hex(unsigned int);   
extern void klog_print_dec(unsigned int);

/* -- MACROS -- */
#define EXCEPTION_STATE ((state_t * )BIOSDATAPAGE)

/* -- VARIABLES -- */
extern unsigned int processCount, softBlockCount;
extern pcb_PTR current_process, pcbIO;
extern unsigned int startTOD;
extern unsigned int pidCount;

extern struct list_head readyQueue;

// we need one list of blocked pcb for every device, each one described in Section 5 in uMPS3 - Principles of Operation
extern struct list_head blockedDiskQueue, blockedFlashQueue, blockedEthernetQueue, blockedPrinterQueue, blockedTerminalTransmQueue,blockedTerminalRecvQueue;
extern struct list_head pseudoClockQueue;

// extern struct list_head waitingMsgQueue;

extern struct list_head pcbFree_h;

extern pcb_PTR ssi_pcb, new_pcb;
extern void test();


/* -- FUNCTIONS PROTOTYPE -- */
/* nucleus module */
void stateCpy(state_t *, state_t *);
void stateCPY4debug(state_t *, state_t *);
void uTLB_RefillHandler();

/* ssi module*/
void SSILoop();
unsigned int SSIRequest(pcb_PTR, ssi_payload_t *);
unsigned int createProcess(ssi_create_process_PTR, pcb_PTR);
void terminateProcess(pcb_PTR);
void wait4Clock(pcb_PTR);
unsigned int getSupportData(pcb_PTR);
unsigned int getProcessID(pcb_PTR, pcb_PTR);

/* exception module*/
void exceptionHandler();
void syscallHandler(state_t *);
int send(unsigned int, unsigned int, unsigned int, state_t *);
void recv(unsigned int, unsigned int, state_t *);
void passUpOrDie(state_t *, unsigned int);

/* interrupt module */
void interruptHandler(state_t *, unsigned int);
void PLTHandler(state_t *);
void intervalTimerHandler(state_t *);
void deviceHandler(unsigned int, state_t *, unsigned int);
pcb_PTR unblockByDeviceNumber(unsigned int, struct list_head *);

/* scheduler module */
void scheduler();

/* timers related functions */
void setPLT(unsigned int);
unsigned int getCPUTime(pcb_PTR);
unsigned int getTOD();
void updateCPUTime(pcb_PTR);

/* miscellaneous  */
unsigned int searchPCB(pcb_PTR, struct list_head *);

#endif
