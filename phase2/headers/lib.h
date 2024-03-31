#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

// necessary libraries
// #include "umps3/umps/arch.h"
// #include "umps3/umps/cp0.h"
#include "/usr/include/umps3/umps/libumps.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

// macros
#define EXCEPTION_STATE ((state_t * )BIOSDATAPAGE)

// variables
extern unsigned int processCount, softBlockCount;
extern pcb_PTR current_process;
extern struct list_head readyQueue, blockedPcbQueue, pseudoClockQueue;
extern struct list_head waitingMsgQueue;
extern struct list_head pcbFree_h;

// function prototypes
void stateCpy(state_t *, state_t *);

void SSILoop();
unsigned int SSIRequest(pcb_PTR, ssi_payload_t *);
unsigned int createProcess(ssi_create_process_PTR, pcb_PTR);

void uTLB_RefillHandler();
void exceptionHandler();
void interruptHandler();
void syscallHandler();
int send(unsigned int, unsigned int);
int recv(unsigned int, unsigned int);
void progTrapHandler();




void scheduler();



#endif
