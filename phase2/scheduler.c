#include "./headers/lib.h"

/**
 * @brief The nuceleus scheduler. The implementation is pre-emptive round robin algorithm with a time slice of 5ms.
 *          
 * @param void
 * @return void
 */
void scheduler()
{
    /* If old PCBs was indeed saved, */
    if (emptyProcQ(&readyQueue))
    {
        if (processCount == 1) HALT(); /* but that queue is empty, that means only SSI_pcb is active. */
        else if (processCount > 1){
            if (softBlockCount == 0) /* Deadlock situation, invoke PANIC BIOS service/instruction. */
                PANIC();
            else if (softBlockCount > 0){ /* Enter Wait State, waiting for a device interrupts*/
                current_process = NULL;

                /* should enable interrupts and disable plt*/
                unsigned int status = getSTATUS();
                status &= !TEBITON | IEPON | IMON;
                setSTATUS(status);
                WAIT();
            }
        }
    }

    /* dispatch and sets another PCB in the readyQueue to currentProcess. */
    current_process = removeProcQ(&readyQueue);
    setTIMER(TIMESLICE);
    LDST(&(current_process->p_s));
}