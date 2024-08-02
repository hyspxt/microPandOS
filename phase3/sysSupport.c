#include "./headers/lib.h"


/**
 * @brief SYSCALL handler. A kernel-mode Syscall exception happens when SYSCALL is called, either by USYS1, USYS2,....
 *        Handles the sytem calls only for the support level, working as a wrapper
 *        for the services defined in the nucleus.
 *
 * @return void
 */
void supExceptionHandler(state_t *excState){
    unsigned int syscallCode = excState->reg_a0;
    unsigned int dest = excState->reg_a1;
        switch (syscallCode)
        { /* here, positive numbered syscall */
        case SENDMSG:
            if (dest == PARENT) /* the requesting process send a message to its SST (his parent in fact)*/
                SYSCALL(SENDMESSAGE, (unsigned int) current_process->p_parent, excState->reg_a2, 0);
            else /* otherwise kernel restricted send */
                SYSCALL(SENDMESSAGE, dest, excState->reg_a2, 0);
            excState->pc_epc += WORDLEN; /* to avoid infinite loop of SYSCALLs */
            LDST(excState);
            break;
        case RECEIVEMSG: /* kernel restricted recv */
            SYSCALL(RECEIVEMESSAGE, dest, excState->reg_a2, 0);
            excState->pc_epc += WORDLEN;
            LDST(excState);
            break;
        default:
            break;
        }
}

/**
 * @brief Program trap exception handler. It is called when a program trap exception occurs.
 *        Eventually, release the mutex after the exception handling is "passed up" by the 
 *        nucleus exception handler.
 *
 * @param void
 * @return void
 */
void programTrapHandler(state_t *excState, int releaseMutex){
    /* first of all, if the calling process is the mutex holder
    we need to release it before the termination, otherwise the 
    mutex will be locked indefinetely */
    if (current_process == mutex_rcvr || releaseMutex) /* mind that mutex_rcvr is the 
    mutex holder and swap_mutex is the mutex giver/releaser */
        SYSCALL(SENDMESSAGE, (unsigned int) swap_mutex, 0, 0); /* send to unblock */
    terminate(OFF); /* SST killing service */
    LDST(excState);
}

/**
 * @brief The central exception handler for the Support Level that redirects the control
 *        to the appropriate handler, based on the cause/type of the singular exception occurred.
 *
 * @param void
 * @return void
 */
void supExceptionHandler()
{
    support_t *supStruct = getSupStruct();
    /* can't use EXCEPTION_STATE macro, it's not in BIOSDATAPAGE
    but in sup_exceptState */
    state_t *excState = &(supStruct->sup_exceptState[GENERALEXCEPT]);
    int excCode = (excState->cause & GETEXECCODE) >> CAUSESHIFT;
    switch (excCode)
    {  /* same as nucleus */
    case SYSEXCEPTION:
        supSyscallHandler(excState);
        break;
    default:
        programTrapHandler(excState, OFF);
        break;
    }
}
