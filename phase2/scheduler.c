#include "./headers/lib.h"

/**
 * @brief The nuceleus scheduler. The implementation is pre-emptive round robin algorithm with a time slice of 5ms.
 *        Its main goal is to dispatch the next process in the readyQueue.
 *
 * @param void
 * @return void
 */
void scheduler()
{
    if (!emptyProcQ(&readyQueue))
    {
        /* The ready queue is not empty */
        /* dispatch and sets another PCB in the readyQueue to currentProcess. */
        current_process = removeProcQ(&readyQueue);
        setTIMER(TIMESLICE);
        LDST(&(current_process->p_s));
    }
    else
    {
        /* Empty Ready Queue case */
        /* Check with process counters if any kind of deadlock situation is happening */
        if (processCount == 1)
            HALT(); /* that means only SSI_pcb is active. */
        else if (processCount > 1 && softBlockCount == 0)
            PANIC(); /* Deadlock situation, invoke PANIC BIOS service/instruction. */
        else if (processCount > 1 && softBlockCount > 0)
        {
            /* Enter Wait State, waiting for a device interrupts*/
            current_process = NULL;
            /* should enable interrupts and disable plt */
            setSTATUS(ALLOFF | IMON | IEPON);
            WAIT();
        }
    }
}
