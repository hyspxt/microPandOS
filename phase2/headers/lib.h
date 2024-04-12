#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

/* -- NECESSARY LIBRARIES -- */
#include "/usr/include/umps3/umps/libumps.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

/* -- MACROS -- */
#define EXCEPTION_STATE ((state_t * )BIOSDATAPAGE)

/* -- VARIABLES -- */
extern unsigned int processCount, softBlockCount;
extern pcb_PTR current_process;

extern struct list_head readyQueue;

// we need one list of blocked pcb for every device, each one described in Section 5 in uMPS3 - Principles of Operation
extern struct list_head blockedDiskQueue, blockedFlashQueue, blockedEthernetQueue, blockedPrinterQueue, blockedTerminalTransmQueue,blockedTerminalRecvQueue;
extern struct list_head pseudoClockQueue;

extern struct list_head waitingMsgQueue;

extern struct list_head pcbFree_h;

/* -- FUNCTIONS PROTOTYPE -- */
/* nucleus module */
void stateCpy(state_t *, state_t *);
void uTLB_RefillHandler();

/* ssi module*/
void SSILoop();
unsigned int SSIRequest(pcb_PTR, ssi_payload_t *);
unsigned int createProcess(ssi_create_process_PTR, pcb_PTR);

/* exception module*/
void exceptionHandler();
void syscallHandler();
int send(unsigned int, unsigned int);
int recv(unsigned int, unsigned int);
void interruptHandler();
void progTrapHandler();

/* scheduler module */
void scheduler();

#endif
