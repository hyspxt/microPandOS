#include "./headers/lib.h"

/**
 * @brief Sends a message to a PCB identified by dest address. If the process is not in the system, the message is not sent.
 *        If the process is waiting for a message (so its performing a recv), it is awakened and put in the ready queue.
 *        After the message is put in the inbox of the corresponding PCB, it is loaded the new state.
 *
 * @param sender the address of sender pcb
 * @param dest the address of destination pcb - register a1 content
 * @param payload the address of the payload to be sent - register a2 content
 * @return int
 */
int send(unsigned int sender, unsigned int dest, unsigned int payload)
{
    msg_PTR msg = allocMsg();
    if (msg == NULL)
        return MSGNOGOOD;

    /* parameter comes in memory address form cause they're taken from the registers,
    so we need to do a casting for both. */
    pcb_PTR senderptr = (pcb_PTR)sender;
    pcb_PTR destptr = (pcb_PTR)dest;

    msg->m_sender = senderptr;
    msg->m_payload = payload;

    if (searchProcQ(destptr, &pcbFree_h))
        return DEST_NOT_EXIST;
    else if ((destptr != current_process) && !searchProcQ(destptr, &readyQueue))
    { /* if dest was waiting for a message, we awaken it*/
        insertProcQ(&readyQueue, destptr);
    }
    insertMessage(&destptr->msg_inbox, msg);
    /* providing 0 as returning value to identify a successful send operation */
    return 0;
}

/**
 * @brief Receives a message from a PCB identified by sender address. If the process is not in the system, the message is not received.
 *        If the process find no message, it is put in the waiting queue by calling the scheduler.
 *        After the message is received, it is loaded the new state.
 *        After the recv, in v0 there will be the sender address and in a2 the payload.
 *
 * @param sender the sender PCB address
 * @param payload the memory area in which the payload will be stored
 * @return void
 */
void recv(unsigned int sender, unsigned int payload)
{
    /* parameter comes from the registers in memory address form, so we need to do a casting */
    pcb_PTR senderptr = (pcb_PTR)sender;
    msg_PTR msg;

    if (sender == ANYMESSAGE)
        msg = popMessage(&current_process->msg_inbox, NULL);
    else
        msg = popMessage(&current_process->msg_inbox, senderptr);

    if (msg == NULL)
    { /* so there aren't any message in the inbox */
        stateCpy(EXCEPTION_STATE, &current_process->p_s);
        updatePCBTime(current_process);
        scheduler();
    }
    else
    { /* putting sender address in v0 register as returning value of recv */
        EXCEPTION_STATE->reg_v0 = (unsigned int)msg->m_sender;
        if (msg->m_payload != 0 && payload != 0)
        { /* we check if the payload should be ignored */
            unsigned int *recvd = (unsigned int *)payload;
            *recvd = msg->m_payload;
        }
        freeMsg(msg);
    }
}

/**
 * @brief SYSCALL handler. A Syscall exception happens when SYSCALL is called, either by SYS1, SYS2,....
 *        It is important to mention that SYSCALL return value is taken from v0 register
 *        (this because some functions in the test din't work properly otherwise).
 *
 * @return void
 */
void syscallHandler()
{
    /* Information is stored in a0, a1, a2, a3 general purpose registers.
    Futhermore, a SYSCALL request can be only done in kernel-mode, and only if a0 contained
    a value in the range [-1...-2]/  */

    /* We check if the processor is in kernel mode looking up at the bit 1 (of 31) of the status register:
    if is 0, then is in Kernel mode, else it's in user mode. */
    if ((EXCEPTION_STATE->status & USERPON))
    { /* We check if that PCB in user-mode checking the status
         in which case, should trigger a Program Trap (PassUp or Die) exception response and the subsequent handler.
         According to princOfOperations, Reserved instruction should be code 10. */
        EXCEPTION_STATE->cause = ((EXCEPTION_STATE->cause) & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
        passUpOrDie(GENERALEXCEPT);
    }
    else
    { /* otherwise, the process is in kernel mode and we can proceed */
        unsigned int syscallCode = EXCEPTION_STATE->reg_a0;
        switch (syscallCode)
        {
        case SENDMESSAGE:
            EXCEPTION_STATE->reg_v0 = send((memaddr)current_process, EXCEPTION_STATE->reg_a1, EXCEPTION_STATE->reg_a2);
            EXCEPTION_STATE->pc_epc += WORDLEN; /* to avoid infinite loop of SYSCALLs */
            LDST(EXCEPTION_STATE);
            break;
        case RECEIVEMESSAGE:
            recv(EXCEPTION_STATE->reg_a1, EXCEPTION_STATE->reg_a2);
            EXCEPTION_STATE->pc_epc += WORDLEN;
            LDST(EXCEPTION_STATE);
            break;
        default: /* trap vector*/
            passUpOrDie(GENERALEXCEPT);
        }
    }
}

/**
 * @brief "Passes up" exception handling storing the saved exception in a location
 *        accessible to the nucleus and pass the control to the proper handler.
 *
 * @param excType exception related constant
 * @return void
 */
void passUpOrDie(unsigned int excType)
{
    if (current_process->p_supportStruct == NULL)
    { /* If there's no support struct, the process and all his progeny will die */
        terminateProcess(current_process);
        scheduler();
    }
    else
    { /* the handling is passed up to a specified routine */
        context_t cxt = current_process->p_supportStruct->sup_exceptContext[excType];
        stateCpy(EXCEPTION_STATE, &current_process->p_supportStruct->sup_exceptState[excType]);
        LDCXT(cxt.stackPtr, cxt.status, cxt.pc);
    }
}

/**
 * @brief The central exception handler that redirects the control to the appropriate handler,
 *        based on the cause/type of the singular exception occurred.
 *
 * @param void
 * @return void
 */
void exceptionHandler()
{ /* When the exception is raised, this function will be called (new stack), TLB-refill events excluded.
  We can distinguish the type of exception by reading the cause in the processor state at the time of the exception.
  In particular, that value will be at the start of BIOS data page. In advance, we assume that PCB is already set to kernel mode and have interrupts disabled. */
    unsigned int cause = getCAUSE();
    /* according to uMPS3: Principles of Operation:
    cause is a 32bit register which bits 2-6 provides a code that identifies the type of exception that occurred.
    The bitwise operation (&) with GETEXECCODE will isolate the exception code from the cause register.
    Then, the right shift (>>) will move the bits to the right, in position 0-4, so that the exception code will be in the least significant bits. */
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    switch (excCode)
    {
    case IOINTERRUPTS: /* Interrupts */
        interruptHandler();
        break;
    case 1 ... TLBINVLDS: /* TLB exceptions */
        passUpOrDie(PGFAULTEXCEPT);
        break;
    case STARTPROGTRAPEXC ... STOPPROGTRAPEXC: /* Program Traps */
        passUpOrDie(GENERALEXCEPT);
        break;
    case SYSEXCEPTION: /* System Calls (SYS1 or SYS2) */
        syscallHandler();
        break;
    case BREAKEXCEPTION ... ENDPROGTRAPEXC: /* Program Traps */
        passUpOrDie(GENERALEXCEPT);
        break;
    }
}
