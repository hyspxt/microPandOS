#include "./headers/lib.h"

/*
    This module contains the support level General Exeption Handler that, with the proper
    initialization in the nucleus, is able to handle the passed-up non-TLB exceptions.
    It manages the support SYSCALL (numbered 1 and above) and the program trap exceptions.
*/

/**
 * @brief Support level SYSCALL handler. A kernel-mode Syscall exception happens when SYSCALL 
 *        is called, either by USYS1, USYS2,.... Handles the sytem calls only for the support 
 *        level, working as a wrapper for the services defined in the nucleus.
 *
 * @return void
 */
void supSyscallHandler(state_t *excState)
{
    unsigned int syscallCode = excState->reg_a0;
    unsigned int dest = excState->reg_a1;
    switch (syscallCode)
    { /* positive numbered syscall */
    case SENDMSG:
        if (dest == PARENT) /* the requesting process send a message to its SST (his parent in fact)*/
            SYSCALL(SENDMESSAGE, (unsigned int)current_process->p_parent, excState->reg_a2, 0);
        else /* otherwise kernel restricted send to another process */
            SYSCALL(SENDMESSAGE, dest, excState->reg_a2, 0);
        break;
    case RECEIVEMSG: /* kernel restricted recv */
        SYSCALL(RECEIVEMESSAGE, dest, excState->reg_a2, 0);
        break;
    default:
        break;
    }
}

/**
 * @brief Program trap exception handler. It is called when an unexpected exception occurs.
 *        Eventually, release the mutex after the exception handling is "passed up" by the
 *        nucleus exception handler.
 *
 * @param void
 * @return void
 */
void programTrapHandler()
{
    for (int i = 0; i < POOLSIZE; i++)
    { /* clean frames */
        if (swapPoolTable[i].sw_asid == current_process->p_supportStruct->sup_asid)
            swapPoolTable[i].sw_asid = NOASID;
    } /* if the calling process is the mutex holder
    we need to release it before the termination, otherwise the
    mutex will be locked indefinetely */
    if (current_process == mutex_recvr) /* not swap_mutex is the mutex giver/releaser */
        SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0); /* send to unblock mutex */
    /* kill the calling sst */
    sendKillReq(NULL);
}

/**
 * @brief General exception handler for the Support Level that redirects to the appropriate handler
 *        based on the cause/type of the singular exception occurred.
 *
 * @param void
 * @return void
 */
void supExceptionHandler()
{ /* EXCEPTION_STATE not in BIOSDATAPAGE */
    support_t *sup = getSupStruct();
    state_t *excState = &(sup->sup_exceptState[GENERALEXCEPT]);
    int excCode = CAUSE_GET_EXCCODE(excState->cause);
    switch (excCode)
    { /* same as nucleus */
    case SYSEXCEPTION:
        supSyscallHandler(excState);
        break;
    default:
        programTrapHandler();
        break;
    }
    excState->pc_epc += WORDLEN;
    LDST(excState);
}
