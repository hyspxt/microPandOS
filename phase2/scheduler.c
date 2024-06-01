#include "./headers/lib.h"

/**
 * @brief The nuceleus scheduler. The implementation is pre-emptive round robin algorithm with
 *        a time slice of 5ms. Its main goal is to dispatch the next process in the readyQueue.
 *
 * @param void
 * @return void
 */
void scheduler()
{
    if (emptyProcQ(&readyQueue))
    { /* Empty Ready Queue case
     Check with process counters if any kind of deadlock situation is happening */
        if (processCount == 1)
            HALT(); /* that means only SSI_pcb is active. */
        else if (processCount > 1 && softBlockCount == 0)
            PANIC(); /* Deadlock situation, invoke PANIC BIOS service/instruction. */
        else if (processCount > 1 && softBlockCount > 0)
        { /* Enter Wait State, waiting for a device interrupts
             should enable interrupts and disable plt */
            current_process = NULL;
            setSTATUS(ALLOFF | IMON | IECON);
            WAIT();
        }
    }
    else
    { /* The ready queue is not empty
      so dispatch and sets another PCB in the readyQueue to currentProcess. */
        current_process = removeProcQ(&readyQueue);
        setPLT(TIMESLICE);
        startTOD = getTOD();
        LDST(&(current_process->p_s));
    }
}
