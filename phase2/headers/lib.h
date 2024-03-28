#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

// necessary libraries
// #include "umps3/umps/arch.h"
// #include "umps3/umps/cp0.h"
#include "umps/libumps.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

// macros
#define EXCEPTION_STATE ((state_t * )BIOSDATAPAGE)

// variables
extern unsigned int processCount, softBlockCount;
extern pcb_PTR current_process;
extern struct list_head readyQueue, blockedPcbQueue, pseudoClockQueue;

// function prototypes
void initSSI();
void systemServiceInterface();

void uTLB_RefillHandler();
void exceptionHandler();
void syscallHandler();
int receiveMessage(pcb_PTR, unsigned int);
int sendMessage(pcb_PTR, unsigned int);

void scheduler();

#endif
