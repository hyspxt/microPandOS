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
    updatePCBTime(current_process);
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
        /* sender is not needed to be current_process, it can be ssi_pcb too */
        msg_PTR msg = allocMsg();
        msg->m_sender = ssi_pcb;
        msg->m_payload = 0;
        insertMessage(&awknPcb->msg_inbox, msg);
        insertProcQ(&readyQueue, awknPcb);
        softBlockCount--;
        awknPcb = removeProcQ(&pseudoClockQueue);
    }

    if (current_process != NULL)
        LDST(excState);
    else
        scheduler();
}

pcb_PTR unblockByDeviceNumber(unsigned int device_number, struct list_head *list)
{
    pcb_PTR tmp;
    list_for_each_entry(tmp, list, p_list)
    {
        if (tmp->blockedOnDevice == device_number)
            return outProcQ(list, tmp);
    }
    return NULL;
}

/**
 * @brief Calculate the interrupt line, corresponding subdevice and send ack to the device.
 *
 * @param cause the cause of the interrupt
 * @param excState the state of the processor
 * @param devLine the device line on which the interrupt is triggered
 * @return void
 */
void deviceHandler(unsigned int cause, state_t *excState, unsigned int interruptLine)
{
    devregarea_t *devRegArea = (devregarea_t *)BUS_REG_RAM_BASE;
    unsigned int devNo, mask, devStatus;
    /* according to specs, use a switch and DEVxON costant to determine device number */
    mask = devRegArea->interrupt_dev[interruptLine - 3];
    if (mask & DEV7ON)
        devNo = 7;
    else if (mask & DEV6ON)
        devNo = 6;
    else if (mask & DEV5ON)
        devNo = 5;
    else if (mask & DEV4ON)
        devNo = 4;
    else if (mask & DEV3ON)
        devNo = 3;
    else if (mask & DEV2ON)
        devNo = 2;
    else if (mask & DEV1ON)
        devNo = 1;
    else if (mask & DEV0ON)
        devNo = 0;


    pcb_PTR outPcb;
    if (interruptLine == IL_TERMINAL)
    {
        /* In this case, the device is terminal transm or recv, so
        we must distinguish between them.*/
        termreg_t *termReg = (termreg_t *)DEV_REG_ADDR(interruptLine, devNo);
        /* According to pops section 5.7 p.42, status code 5 for both recv_status and trasm_status
        imply a successful operation on the device.*/
        if ((termReg->transm_status & 0x000000FF) == OKCHARTRANS)
        {
            devStatus = termReg->transm_status;
            termReg->transm_command = ACK;
            outPcb = unblockByDeviceNumber(devNo, &blockedTerminalTransmQueue);
        }
        else
        {
            devStatus = termReg->recv_status;
            termReg->recv_command = ACK;
            outPcb = unblockByDeviceNumber(devNo, &blockedTerminalRecvQueue);
        }
    }
    else
    {
        /* In this case, the device is not terminal, neither transm or recv, so
        it could be disk, flash, ethernet or printer.*/
        dtpreg_t *not_termReg = (dtpreg_t *)DEV_REG_ADDR(interruptLine, devNo);
        devStatus = not_termReg->status; /* save off the status code from the device's device register */
        not_termReg->command = ACK;      /* acknowledgement of the interrupt */
        switch (interruptLine)
        {
        /* exploiting fifoness */
        case IL_DISK:
            outPcb = unblockByDeviceNumber(devNo, &blockedDiskQueue);
            break;
        case IL_FLASH:
            outPcb = unblockByDeviceNumber(devNo, &blockedFlashQueue);
            break;
        case IL_ETHERNET:
            outPcb = unblockByDeviceNumber(devNo, &blockedEthernetQueue);
            break;
        case IL_PRINTER:
            outPcb = unblockByDeviceNumber(devNo, &blockedPrinterQueue);
            break;
        default:
            outPcb = NULL;
            break;
        }
    }

    if (outPcb != NULL)
    {
        outPcb->p_s.reg_v0 = devStatus;
        msg_PTR msg = allocMsg();
        msg->m_sender = ssi_pcb;
        msg->m_payload = (memaddr)devStatus;
        insertMessage(&outPcb->msg_inbox, msg);
        insertProcQ(&readyQueue, outPcb);
        softBlockCount--;
    }

    if (current_process != NULL)
        LDST(excState);
    else
        scheduler();
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
    if (CAUSE_IP_GET(cause, IL_CPUTIMER))
        PLTHandler(excState);
    else if (CAUSE_IP_GET(cause, IL_TIMER))
        intervalTimerHandler(excState);
    else if (CAUSE_IP_GET(cause, IL_DISK)) /* from here, interrupt devices*/
        deviceHandler(cause, excState, IL_DISK);
    else if (CAUSE_IP_GET(cause, IL_FLASH))
        deviceHandler(cause, excState, IL_FLASH);
    else if (CAUSE_IP_GET(cause, IL_ETHERNET))
        deviceHandler(cause, excState, IL_ETHERNET);
    else if (CAUSE_IP_GET(cause, IL_PRINTER))
        deviceHandler(cause, excState, IL_PRINTER);
    else if (CAUSE_IP_GET(cause, IL_TERMINAL))
        deviceHandler(cause, excState, IL_TERMINAL);
}