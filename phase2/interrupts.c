#include "./headers/lib.h"

/**
 * @brief
 *
 * @param excState the state of the processor
 * @return void
 */
void PLTHandler(state_t *excState)
{

    /* According to uMPS3 - pops: PLT interrupts are always on interrupt line 1 and
    they acknowledged by writing a new value into the CP0 Timer register.*/
    setPLT(0);
    updateCPUTime(current_process);

    stateCpy(&current_process->p_s, excState); 
    insertProcQ(&readyQueue, current_process);

    scheduler();
}

void intervalTimerHandler(state_t *excState)
{

    pcb_PTR awknPcb;
    /* We aknowledge the interrupt loading 100 milliseconds in the Interval Timer */
    LDIT(PSECOND);

    awknPcb = removeProcQ(&pseudoClockQueue);

    /* unlock all PCBs waiting a pseudo-clock tick in the queue */
    while (awknPcb != NULL)
    {
        softBlockCount--;
        /* sender is not needed to be current_process, in this case is ssi_pcb TODO*/
        send((unsigned int)ssi_pcb, (unsigned int)awknPcb, 0);
        insertProcQ(&readyQueue, awknPcb);
    }

    if (current_process == NULL)
        scheduler();
    else
        LDST(excState);
}

/**
 * @brief
 *
 * @param excState the state of the processor
 * @param cause the cause of the interrupt
 * @return void
 */
void interruptHandler(state_t *excState, unsigned int cause)
{
    if (CAUSE_IP_GET(IL_CPUTIMER, cause))
        PLTHandler(excState);
    else if (CAUSE_IP_GET(IL_TIMER, cause))
        intervalTimerHandler(excState);
}