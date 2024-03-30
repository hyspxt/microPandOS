#include "./headers/lib.h"
struct list_head waitingMsgQueue; /* additional queue to keep PCBs who are attending msgs*/

int send(pcb_PTR dest, unsigned int payload) // we don't need the sender cause it's async
{
    msg_PTR msg = allocMsg();
    if (msg == NULL)
        return MSGNOGOOD;
    msg->m_sender = current_process;
    msg->m_payload = payload;

    if (searchProcQ(&pcbFree_h, dest) == dest)
        return DEST_NOT_EXIST;
    else
    {
        if (searchProcQ(&waitingMsgQueue, dest) == dest) {// awaken the process and put it in the ready queue
            pcb_PTR awaitProc = outProcQ(&waitingMsgQueue, dest);
            insertProcQ(&readyQueue, awaitProc);
        }
        pushMessage(&dest->msg_inbox, msg);
        return 0; // place 0 in v0 register if successful
    }
}

void recv()
{
    // TODO
}

/**
 * @brief SYSCALL handler. A System call exception occurs when the SYSCALL is executed.
 *
 * @param void
 * @return void
 */
void syscallHandler()
{
    /* Information is stored in a0, a1, a2, a3 general purpose registers.
    Futhermore, a SYSCALL request can be only done in kernel-mode, and if a0 contained
    a value in the range [-1...-2]/  */

    if (current_process->p_s.status & (USERPON != 0))
    {
        /* We check if that PCB status is in USER mode
        in which case, we should trigger a Program Trap exception response and the subsequent handler.
        According to princOfOperations, Reserved instruction should be code 10. */
        current_process->p_s.cause = ((current_process->p_s.cause) & ~GETEXECCODE) | (PRIVINSTR << CAUSESHIFT);
        progTrapHandler();
    }

    int syscallCode = current_process->p_s.reg_a0;

    switch (syscallCode)
    {
    case SENDMESSAGE:
        current_process->p_s.reg_v0 = send((pcb_PTR) current_process->p_s.reg_a1, current_process->p_s.reg_a2);  // TODO casting might actually break something
        break;
    case RECEIVEMESSAGE:
        recv();
        break;
    }
}

void progTrapHandler()
{
}

/**
 * @brief The central exception handler, that redirects the control to the appropriate handler,
 *        based on the cause/type of the singular exception.
 *
 * @param void
 * @return void
 */
void exceptionHandler()
{
    /* When the exception is raised, this function will be called (new stack), TLB-refill events excluded.
    We can distinguish the type of exception by reading the cause in the processor state at the time of the exception.
    In particular, that value will be at the start of BIOS data page. In advance, we assume that PCB is already set to kernel mode and have interrupts disabled. */
    unsigned int cause = getCAUSE();

    /* according to uMPS3: Principles of Operation:
    cause is a 32bit register which bits 2-6 provides a code that identifies the type of exception that occurred.
    The bitwise operation (&) with GETEXECCODE will isolate the exception code from the cause register.
    Then, the right shift (>>) will move the bits to the right, in position 0-4, so that the exception code will be in the least significant bits. */
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    stateCpy((state_t *)BIOSDATAPAGE, &current_process->p_s);

    if (excCode == IOINTERRUPTS)
    {
        /* ExcCode = 0 it means interrupts and we pass the code  */
        // interruptHandler(); // TODO implement interruptHandler in interrupt.c module
    }
    else if (excCode > IOINTERRUPTS && excCode <= TLBINVLDS) /* ExcCode is between 1 and 3 */
    {
        /* TLB exception handler*/
    }
    else if (excCode == SYSEXCEPTION) /* ExcCode = 8 */
        syscallHandler();
    else if (excCode <= 12) /* ExCode in 4-9 and 9-12 means program trap*/
        progTrapHandler();
}
