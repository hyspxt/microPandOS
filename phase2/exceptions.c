#include "./headers/lib.h"

/**
 * @brief Sends a message to a PCB identified by dest address. If the process is not in the system, the message is not sent.
 *        If the process is waiting for a message, it is awakened and put in the ready queue.
 *
 * @param dest the destination process
 * @param payload the message to be sent
 * @return int
 */
int send(unsigned int dest, unsigned int payload)
{
    msg_PTR msg = allocMsg();
    if (msg == NULL)
        return MSGNOGOOD;

    /* parameter comes in memory address form, so we need to do a casting */
    pcb_PTR destptr = (pcb_PTR)dest;

    msg->m_sender = current_process;
    msg->m_payload = payload;

    if (searchProcQ(&pcbFree_h, destptr) == NULL)
        return DEST_NOT_EXIST;
    else if (!(searchProcQ(&readyQueue, destptr) != NULL || (destptr == current_process)))
    {
        /* if dest was waiting for a message, we awaken it*/
        insertProcQ(&readyQueue, destptr);
    }
    insertMessage(&destptr->msg_inbox, msg);
    return 0;
}

/**
 * @brief Receives a message from a PCB identified by sender address. If the process is not in the system, the message is not received.
 *        If the process is waiting for a message, it is put in the waiting queue.
 *
 * @param sender the sender PCB address
 * @param payload the message to be received
 * @return int
 */
int recv(unsigned int sender, unsigned int payload)
{


    // TODOOO might check on this, it could be incorrect


    /* parameter comes in memory address form, so we need to do a casting */
    pcb_PTR senderptr = (pcb_PTR)sender;
    msg_PTR msg;

    if (sender == ANYMESSAGE)
        msg = popMessage(&current_process->msg_inbox, NULL);
    else
        msg = popMessage(&current_process->msg_inbox, senderptr);

    if (msg == NULL) /* so there aren't any message in the inbox, we should put it in waiting state */
    {
        /* This becomes a blocking RECV*/
        stateCpy(EXCEPTION_STATE, &current_process->p_s);
        updateCPUTime(current_process);
        scheduler();
    }
    else
    {
        if (payload != (unsigned int)NULL)
            *&payload = msg->m_payload;
    }
    unsigned int result = (unsigned int)msg->m_sender;
    freeMsg(msg);
    return (result); // return the sender's identifier in v0 register
}

/**
 * @brief SYSCALL handler. A System call exception occurs when the SYSCALL is executed.
 *
 * @param void
 * @return void
 */
void syscallHandler(state_t *excState)
{
    /* Information is stored in a0, a1, a2, a3 general purpose registers.
    Futhermore, a SYSCALL request can be only done in kernel-mode, and if a0 contained
    a value in the range [-1...-2]/  */

    /* We check if the process is in kernel mode looking up at the bit 1 (of 31) of the status register:
    if is 0, then is in Kernel mode, else it's in user mode*/

    if ((excState->status << 30) >> 31)
    {
        /* We check if that PCB status is in USER mode
        in which case, should trigger a Program Trap (PassUp or Die) exception response and the subsequent handler.
        According to princOfOperations, Reserved instruction should be code 10. */
        excState->cause = ((excState->cause) & CLEAREXECCODE) | (PRIVINSTR << CAUSESHIFT);
        passUpOrDie(excState, GENERALEXCEPT);
    }
    else if (excState->reg_a0 >= 1)
        passUpOrDie(excState, GENERALEXCEPT);
    else
    {
        unsigned int syscallCode = excState->reg_a0;
        unsigned int payload = excState->reg_a2;
        switch (syscallCode)
        {
        case SENDMESSAGE:
            unsigned int dest = excState->reg_a1;

            excState->reg_v0 = send(dest, payload);
            break;
        case RECEIVEMESSAGE:
            unsigned int sender = excState->reg_a1;

            excState->reg_v0 = recv(sender, payload);
            break;
        }

        excState->pc_epc += WORDLEN; // to avoid infinite loop of SYSCALLs
        LDST(excState);
    }
}

void passUpOrDie(state_t *excState, unsigned int excType)
{
    if (current_process->p_supportStruct == NULL)
    {
        terminateProcess(current_process);
        scheduler();
    }
    else
    {
        stateCpy(excState, &current_process->p_supportStruct->sup_exceptState[excType]);
        LDCXT(current_process->p_supportStruct->sup_exceptContext[excType].stackPtr,
              current_process->p_supportStruct->sup_exceptContext[excType].status,
              current_process->p_supportStruct->sup_exceptContext[excType].pc);
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
{

    // state_t *excState = (state_t *)BIOSDATAPAGE;
    /* When the exception is raised, this function will be called (new stack), TLB-refill events excluded.
    We can distinguish the type of exception by reading the cause in the processor state at the time of the exception.
    In particular, that value will be at the start of BIOS data page. In advance, we assume that PCB is already set to kernel mode and have interrupts disabled. */
    unsigned int cause = getCAUSE();
    /* according to uMPS3: Principles of Operation:
    cause is a 32bit register which bits 2-6 provides a code that identifies the type of exception that occurred.
    The bitwise operation (&) with GETEXECCODE will isolate the exception code from the cause register.
    Then, the right shift (>>) will move the bits to the right, in position 0-4, so that the exception code will be in the least significant bits. */
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;

    if (excCode == IOINTERRUPTS)
    {
        /* ExcCode = 0 it means interrupts and we pass the code  */
        // interruptHandler(); // TODO implement interruptHandler in interrupt.c module
    }
    else if (excCode > IOINTERRUPTS && excCode <= TLBINVLDS) /* ExcCode is between 1 and 3 */
    {
        passUpOrDie(EXCEPTION_STATE, PGFAULTEXCEPT);
    }
    else if (excCode == SYSEXCEPTION) /* ExcCode = 8 */
        syscallHandler(EXCEPTION_STATE);
    else if (excCode <= 12) /* ExCode in 4-9 and 9-12 means program trap*/
        passUpOrDie(EXCEPTION_STATE, GENERALEXCEPT);
    else /* ExcCode is not in the range [0...12] */
        PANIC();
}
