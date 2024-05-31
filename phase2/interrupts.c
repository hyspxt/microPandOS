#include "./headers/lib.h"

/**
 * @brief PLT interrupts is one of the three types of thing that a "running" process
 *        can do, other than transitioning running to blocked state and terminate.
 *        The PLT interrupts happens when the current process has used up its time slice
 *        but has not completed its CPU burst. So, the process is put back in the ready queue.
 *
 * @param void
 * @return void
 */
void PLTHandler()
{
    /* According to uMPS3 - pops 4.1.4: PLT interrupts are always on interrupt line 1 and
    they acknowledged by writing a new value into the CP0 Timer register */
    setPLT(0);
    updatePCBTime(current_process);
    stateCpy(&current_process->p_s, EXCEPTION_STATE);
    insertProcQ(&readyQueue, current_process);
    scheduler();
}

/**
 * @brief The interval timer interrupt is used to wake up processes that are waiting for a pseudo-clock tick.
 *        The interval timer is set to 100 milliseconds, so every 100 milliseconds the interrupt is triggered.
 *        To wait for this pseudo-clock tick, the process is put in the queue requesting a WaitForClock service.
 *        This type of interrupts are always on interrupt line 2.
 *
 * @param void
 * @return void
 */
void intervalTimerHandler()
{
    pcb_PTR awknPcb;
    LDIT(PSECOND); /* we ack the interrupt loading 100 milliseconds in the IT */
    awknPcb = removeProcQ(&pseudoClockQueue);

    /* unlock all PCBs waiting a pseudo-clock tick in the queue */
    while (awknPcb != NULL)
    { /* sender is not needed to be current_process, it can be ssi_pcb too */
        msg_PTR msg = allocMsg();
        msg->m_sender = ssi_pcb;
        msg->m_payload = 0;
        insertMessage(&awknPcb->msg_inbox, msg);
        insertProcQ(&readyQueue, awknPcb);
        softBlockCount--;
        awknPcb = removeProcQ(&pseudoClockQueue);
    }

    if (current_process != NULL)
        LDST(EXCEPTION_STATE);
    else
        scheduler();
}

/**
 * @brief Get the device bitmap corresponding to the interrupt line.
 *        When bit i (corresponding to device i) in word j (corresponding to interrupt line j) is 1
 *        then the device i attached to interrupt line j + 3
 *
 * @param interruptLine the numer of the interrupt line.
 * @return bitmap corresponding to interrupt line.
 */
unsigned int getDeviceBitmap(unsigned int interruptLine)
{
    devregarea_t *devRegArea = (devregarea_t *)BUS_REG_RAM_BASE;
    return devRegArea->interrupt_dev[interruptLine - 3];
}

/**
 * @brief Get the device number corresponding to the device bitmap.
 *
 * @param mask the bitmap of the device.
 * @return the device number.
 */
unsigned int getDeviceNo(unsigned int mask)
{
    if (mask & DEV7ON)
        return 7;
    else if (mask & DEV6ON)
        return 6;
    else if (mask & DEV5ON)
        return 5;
    else if (mask & DEV4ON)
        return 4;
    else if (mask & DEV3ON)
        return 3;
    else if (mask & DEV2ON)
        return 2;
    else if (mask & DEV1ON)
        return 1;
    else if (mask & DEV0ON)
        return 0;
    return -1;
}

/**
 * @brief Get the PCB from the blocked queue corresponding to the interrupt line number.
 *
 * @param devNo the device number.
 * @param interruptLine the interrupt line.
 * @return the PCB from the blocked queue.
 */
pcb_PTR getPcbFromLine(unsigned int interruptLine, unsigned int devNo)
{
    switch (interruptLine)
    { /* exploiting fifoness */
    case IL_DISK:
        return unblockPcbDevNo(devNo, &blockedDiskQueue);
    case IL_FLASH:
        return unblockPcbDevNo(devNo, &blockedFlashQueue);
    case IL_ETHERNET:
        return unblockPcbDevNo(devNo, &blockedEthernetQueue);
    case IL_PRINTER:
        return unblockPcbDevNo(devNo, &blockedPrinterQueue);
    default:
        return NULL;
    }
}

/**
 * @brief Calculate the interrupt line, corresponding subdevice and send ack to the device.
 *
 * @param interruptLine the device line on which the interrupt is triggered
 * @return void
 */
void deviceHandler(unsigned int interruptLine)
{
    unsigned int devNo, mask, devStatus;
    pcb_PTR outPcb;

    /* according to specs, use a switch and DEVxON costant to determine device number */
    mask = getDeviceBitmap(interruptLine);
    devNo = getDeviceNo(mask); /* get the device number using bitmap*/

    if (interruptLine == IL_TERMINAL)
    { /* In this case, the device is terminal transm or recv, so
      we must distinguish between the two subdevices.*/
        termreg_t *termReg = (termreg_t *)DEV_REG_ADDR(interruptLine, devNo);
        /* According to pops section 5.7 p.42, status code 5 for both recv_status and trasm_status
        imply a successful operation on the device.*/
        if ((unsigned char)termReg->transm_status == OKCHARTRANS)
        { /* unsigned char allow us to retrieve the last byte of the status*/
            outPcb = unblockPcbDevNo(devNo, &blockedTerminalTransmQueue);
            devStatus = termReg->transm_status;
            termReg->transm_command = ACK;
        }
        else if ((unsigned char)termReg->recv_status == CHARRECV)
        { /* in this case, the device is in recv */
            outPcb = unblockPcbDevNo(devNo, &blockedTerminalRecvQueue);
            devStatus = termReg->recv_status;
            termReg->recv_command = ACK;
        }
        else
            PANIC();
    }
    else
    { /* In this case, the device is not terminal, neither transm or recv, so
        it could be disk, flash, ethernet or printer.*/
        dtpreg_t *not_termReg = (dtpreg_t *)DEV_REG_ADDR(interruptLine, devNo);
        outPcb = getPcbFromLine(interruptLine, devNo);
        devStatus = not_termReg->status; /* save off the status code from the device's device register */
        not_termReg->command = ACK;      /* acknowledgement of the interrupt */
    }

    if (outPcb != NULL)
    { /* without passing by the ssi, we put the status in the inbox */
        outPcb->p_s.reg_v0 = devStatus;
        msg_PTR msg = allocMsg();
        msg->m_sender = ssi_pcb;
        msg->m_payload = (memaddr)devStatus;
        insertMessage(&outPcb->msg_inbox, msg);
        insertProcQ(&readyQueue, outPcb);
        softBlockCount--;
    }

    if (current_process != NULL)
        LDST(EXCEPTION_STATE);
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
void interruptHandler(unsigned int cause)
{ /* we can ignore IL_IPI (Inter-Processor interrupts) since microPandOs is intended
    for uniprocessor environments more info on chapter 5 pops. */
    if (CAUSE_IP_GET(cause, IL_CPUTIMER))
        PLTHandler();
    else if (CAUSE_IP_GET(cause, IL_TIMER))
        intervalTimerHandler();
    else if (CAUSE_IP_GET(cause, IL_DISK)) /* from here, interrupt devices*/
        deviceHandler(IL_DISK);
    else if (CAUSE_IP_GET(cause, IL_FLASH))
        deviceHandler(IL_FLASH);
    else if (CAUSE_IP_GET(cause, IL_ETHERNET))
        deviceHandler(IL_ETHERNET);
    else if (CAUSE_IP_GET(cause, IL_PRINTER))
        deviceHandler(IL_PRINTER);
    else if (CAUSE_IP_GET(cause, IL_TERMINAL))
        deviceHandler(IL_TERMINAL);
}