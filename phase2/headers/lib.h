#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

// necessary libraries
// #include "umps3/umps/arch.h"
// #include "umps3/umps/cp0.h"
// #include "umps3/umps/libumps.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

// macros
#define EXCEPTION_STATE ((state_t * )BIOSDATAPAGE)

// variables
extern unsigned int processCount, softBlockCount;
extern struct list_head readyQueue;
extern pcb_PTR current_process;
extern pcb_PTR blockedPcb_list[SEMDEVLEN - 1], waitPClock_list[SEMDEVLEN -1];


// function prototypes

#endif
