#include "./headers/lib.h"

/**
 * @brief PLT interrupts is one of the three types of thing that a "running" process
 *        can do, other than transitioning running to blocked state and terminate.
 *        The PLT interrupts happens when the current process has used up its time slice
 *        but has not completed its CPU burst. So, the process is put back in the ready queue.
 *
 * @param excState the state of cause register
 * @return void
 */
void PLTHandler(state_t *excState)
{
    /* According to uMPS3 - pops 4.1.4: PLT interrupts are always on interrupt line 1 and
    they acknowledged by writing a new value into the CP0 Timer register */
    setPLT(0);
    updateCPUTime(current_process);
    stateCpy(&current_process->p_s, excState);
    insertProcQ(&readyQueue, current_process);

    scheduler();
}

/**
 * @brief The interval timer interrupt is used to wake up processes that are waiting for a pseudo-clock tick.
 *        The interval timer is set to 100 milliseconds, so every 100 milliseconds the interrupt is triggered.
 *        To wait for this pseudo-clock tick, the process is put in the queue requesting a WaitForClock service.
 *        This type of interrupts are always on interrupt line 2.
 *
 * @param excState the state of the cause register
 * @return void
 */
void intervalTimerHandler(state_t *excState)
{
    pcb_PTR awknPcb;
    /* we aknowledge the interrupt loading 100 milliseconds in the Interval Timer */
    LDIT(PSECOND);
    awknPcb = removeProcQ(&pseudoClockQueue);

    /* unlock all PCBs waiting a pseudo-clock tick in the queue */
    while (awknPcb != NULL)
    {
        softBlockCount--;
        /* sender is not needed to be current_process, it can be ssi_pcb too */
        msg_PTR msg = allocMsg();
        msg->m_sender = ssi_pcb;
        msg->m_payload = 0;
        insertMessage(&awknPcb->msg_inbox, msg);
        insertProcQ(&readyQueue, awknPcb);
    }

    if (current_process != NULL)
        LDST(excState);
    else
        scheduler();
}


/**
 * @brief Calculate the interrupt line, corresponding subdevice and send ack to the device.
 *
 * @param cause the cause of the interrupt
 * @param excState the state of the processor
 * @param devLine the device line on which the interrupt is triggered
 * @return void
 */
void deviceHandler(unsigned int cause, state_t *excState, unsigned int devLine)
{
    /* according to p.24 pops, bus register area is described by devregarea struct */
    devregarea_t *devReg = (devregarea_t *)RAMBASEADDR;
    unsigned int devNo, devStatus, mask;

    /* mask represent the bitmap + interrupt line/device memory portion described in pops p.85 */
    mask = devReg->interrupt_dev[devLine - 3];

    /* according to specs, use a switch and DEVxON costant to determine device number */
    if (mask & DEV0ON) devNo = 0;
    else if (mask & DEV1ON) devNo = 1;
    else if (mask & DEV2ON) devNo = 2;
    else if (mask & DEV3ON) devNo = 3;
    else if (mask & DEV4ON) devNo = 4;
    else if (mask & DEV5ON) devNo = 5;
    else if (mask & DEV6ON) devNo = 6;
    else if (mask & DEV7ON) devNo = 7;

    if (devLine != IL_TERMINAL)
    {
        /* In this case, the device is not terminal, neither transm or recv, so 
        it could be disk, flash, ethernet or printer.*/
        dtpreg_t *not_termReg = (dtpreg_t *)DEV_REG_ADDR(devLine, devNo);
        devStatus = not_termReg->status;
        not_termReg->command = ACK;

    }
    else
    {
        /* In this case, the device is terminal transm or recv, so 
        we must distinguish between them.*/
    }
}

/**
 * @brief Handles the various interrupts in order of priority 1 to 7.
 *
 * @param excState the state of the processor
 * @param cause the cause of the interrupt
 * @return void
 */
void interruptHandler(state_t *excState, unsigned int cause)
{

    /* we can ignore IL_IPI (Inter-Processor interrupts) since microPandOs is intended 
    for uniprocessor environments more info on chapter 5 pops. */
    if (CAUSE_IP_GET(IL_CPUTIMER, cause))
        PLTHandler(excState);
    else if (CAUSE_IP_GET(IL_TIMER, cause))
        intervalTimerHandler(excState);
    else if (CAUSE_IP_GET(IL_DISK, cause)) /* from here, interrupt devices*/
        deviceHandler(cause, excState, IL_DISK);
    else if (CAUSE_IP_GET(IL_FLASH, cause))
        deviceHandler(cause, excState, IL_FLASH);
    else if (CAUSE_IP_GET(IL_ETHERNET, cause))
        deviceHandler(cause, excState, IL_ETHERNET);
    else if (CAUSE_IP_GET(IL_PRINTER, cause))
        deviceHandler(cause, excState, IL_PRINTER);
    else if (CAUSE_IP_GET(IL_TERMINAL, cause))
        deviceHandler(cause, excState, IL_TERMINAL);
    else
        PANIC();
}