#include "./headers/lib.h"

void uTLB_RefillHandler()
{
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)0x0FFFF000);
}

void syscallHandler()
{

}

void progTrapHandler()
{

}

/**
 * @brief The central exception handler, that redirects the control to the appropriate handler,
 *        based on the cause/type of the singular exception.
 *
 * @param void
 * @return int
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
    Then, the right shift (>>) will move the bits to the right, in position 0-4, so that the exception code will be in the least significant bits.
     */
    unsigned int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
    copyState((state_t *)BIOSDATAPAGE, &current_process->p_s);

    if (excCode == IOINTERRUPTS)
    {
        /* ExcCode = 0 it means interrupts and we pass the code  */
        // interruptHandler(); // TODO implement interruptHandler
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
