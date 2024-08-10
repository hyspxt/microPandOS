#include "./headers/lib.h"
/*
 * @brief Creates a child process of the parent process. Then, the child process is inserted in the ready queue
 * 		  and the address of the child is returned in order to be the ssi response to a certain ssi request from
 * 		  the parent pcb. The service is associated with the mnemonic constant CREATEPROCESS = 1.
 *
 * @param sup the support structure of the new process
 * @param parent the parent process
 * @return int the address of the new process
 */
unsigned int createProcess(pcb_PTR parent, ssi_create_process_PTR sup)
{
	if (emptyProcQ(&pcbFree_h))
		return NOPROC;
	pcb_PTR child = allocPcb();
	if (child == NULL)
		return NOPROC;

	child->p_supportStruct = sup->support;

	/* copy the state sent along with the request in the
		new child pcb and insert the newborn the readyQueue*/
	stateCpy(sup->state, &child->p_s);
	insertProcQ(&readyQueue, child);
	insertChild(parent, child);
	processCount++;
	return (unsigned int)child;
}

/**
 * @brief Check if the PCB is blocked on a device, looking at all device queues.
 *
 * @param sender the pcb that should be checked
 * @return 1 if the PCB is blocked on a device, 0 otherwise
 */
unsigned int isPcbBlockedOnDevice(pcb_PTR sender)
{
	return (outProcQ(&blockedTerminalRecvQueue, sender) != NULL || outProcQ(&blockedTerminalTransmQueue, sender) != NULL ||
			outProcQ(&blockedDiskQueue, sender) != NULL || outProcQ(&blockedFlashQueue, sender) != NULL ||
			outProcQ(&blockedEthernetQueue, sender) != NULL || outProcQ(&blockedPrinterQueue, sender) != NULL ||
			outProcQ(&pseudoClockQueue, sender) != NULL);
}

/**
 * @brief Kill a process. If the process is the one that requested the service, then the process is terminated.
 * 		  When a process is terminated, in addiction, all his progeny (children) must be terminated too.
 * 		  The service is associated with the mnemonic constant TERMPROCESS = 2.
 *
 * @param sender the process that requested the service
 * @return void
 */
void terminateProcess(pcb_PTR sender)
{
	/* iteratively, we explore the PCB tree structure using sender as a root, then recursively we kill his PCB children.
	This continue while sender has any children. */
	if (sender == ssi_pcb)
		PANIC();

	while (!emptyChild(sender))
	{
		terminateProcess(removeChild(sender));
	}

	if (isPcbBlockedOnDevice(sender))
		softBlockCount--;

	outChild(sender);
	freePcb(sender);
	processCount--;
}

/**
 * @brief Insert the process in the curresponding device queue.
 *
 * @param interruptLine the interrupt line of the device
 * @param sender the process that requested the service
 * @return void
 */
void insertDeviceQ(unsigned int interruptLine, pcb_PTR sender)
{
	switch (interruptLine)
	{
	case IL_DISK:
		insertProcQ(&blockedDiskQueue, sender);
		break;
	case IL_FLASH:
		insertProcQ(&blockedFlashQueue, sender);
		break;
	case IL_ETHERNET:
		insertProcQ(&blockedEthernetQueue, sender);
		break;
	case IL_PRINTER:
		insertProcQ(&blockedPrinterQueue, sender);
		break;
	}
}


void blockInDevice(memaddr deviceCommand, pcb_PTR sender){
	/* We need to distinguish between terminal and not-terminal devices, that are transm and recv
	so, according to uMPS3 - Principles of Operation, each device is identified by the interrupt line
	it is attached to and its device number, an integer in the range 0-7. Important! in order of priority */

	/* If the device isn't a terminal device, then it is a disk, flash, ethernet or printer device
	   so, we iter on those basing on their identifiers.
	   According to uMPS3 - Principles of Operation p.29 devices have interrupts pending on interrupt lines 3-7.
	   That means that when bit i in word j is set to one, then device i attached to interrupt line j + 3 has a pending interrupt. */

	for (unsigned int devNo = 0; devNo < N_DEV_PER_IL; devNo++)
	{
		/* calculate the address for this device's terminal device register */
		termreg_t *devAddrBase = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, devNo);
		if ((unsigned int)&devAddrBase->transm_command == deviceCommand)
		{
			sender->blockedOnDevice = devNo;
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalTransmQueue, sender);
			return;
		}
		else if ((unsigned int)&devAddrBase->recv_command == deviceCommand)
		{
			sender->blockedOnDevice = devNo;
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalRecvQueue, sender);
			return;
		}
	}

	for (unsigned int interruptLine = DEV_IL_START; interruptLine < N_INTERRUPT_LINES - 1; interruptLine++)
	{
		for (unsigned int devNo = 0; devNo < N_DEV_PER_IL; devNo++)
		{ /* calculate base address for non-terminal devices */
			dtpreg_t *devAddrBase = (dtpreg_t *)DEV_REG_ADDR(interruptLine, devNo);
			if ((unsigned int)&devAddrBase->command == deviceCommand)
			{
				sender->blockedOnDevice = devNo;
				outProcQ(&readyQueue, sender);
				insertDeviceQ(interruptLine, sender);
				return;
			}
		}
	}
}


/**
 * @brief Handles the synchronous I/O requests.
 *
 * @param doioPTR the IO request, as a pointer to the doio struct
 * @param sender the process that requested the service
 * @return void
 */
void doio(ssi_do_io_PTR doioPTR, pcb_PTR sender)
{
	softBlockCount++;
	unsigned int deviceCommand = (unsigned int)doioPTR->commandAddr;
	blockInDevice(deviceCommand, sender);
	*(doioPTR->commandAddr) = doioPTR->commandValue;
	
}

/**
 * @brief Allow the sender to get back its own accumulated processor time..
 * 		  Hence, the nucleus records in sender->p_time the amount of processor time used by each process.
 *
 * @param sender the process that requested the service
 * @return unsigned int - the CPU time of the process
 */
unsigned int getCPUTime(pcb_PTR sender)
{
	return (sender->p_time);
}

/**
 * @brief Returns the TOD (Time Of Day) clock value.
 *
 * @param
 * @return unsigned int the CPU time of the process
 */
unsigned int getTOD()
{
	unsigned int TOD_LOAD;
	STCK(TOD_LOAD);
	return (TOD_LOAD);
}

/**
 * @brief Update the time of PCB that requested the service.
 * 		  Start is called when the process is dispatched, while updatePCBTime
 * 		  is called when the process is preempted.
 *        note. ndr an alternative way to do this could be disabling PLT
 *
 * @param sender the process that requested the service
 * @return void
 */
void updatePCBTime(pcb_PTR sender)
{
	unsigned int endTOD = getTOD();
	sender->p_time += endTOD - startTOD;
	startTOD = endTOD;
}

/**
 * @brief Set the value in Processor Local Timer as described at
 * 		  p.22 of uMPS3 - Principles of Operation.
 *
 * @param time number of ticks to load in the timer
 * @return void
 */
void setPLT(unsigned int time)
{
	setTIMER(time * TIMESCALEADDR);
}

/**
 * @brief Allow the sender to suspend its execution until the next pseudo-clock tick.
 * 		  The pseudo-clock is a software clock that generates a clock interrupt every 100 milliseconds (PSECOND).
 *
 * @param sender the process that requested the service
 * @return void
 */
void wait4Clock(pcb_PTR sender)
{
	insertProcQ(&pseudoClockQueue, sender);
	softBlockCount++;
}

/**
 * @brief Allow the sender to obtain the process' Support Structure.
 * 		  The support structure is a data structure that contains the information
 *        needed by the SSI to perform the service requested by the process.
 *
 * @param sender the process that requested the service
 * @return void
 */
unsigned int getSupportData(pcb_PTR sender)
{
	return (unsigned int)sender->p_supportStruct;
}

/**
 * @brief Allow the sender to get the process ID of the process that requested the service.
 * 		  If arg is 0 return the sender's pid, or otherwise the sender's parent pid.
 * @param sender the process that requested the service
 * @param arg the argument of the message
 * @return unsigned int the process ID
 */
unsigned int getProcessID(pcb_PTR sender, pcb_PTR arg)
{
	if (arg == NULL)
		return sender->p_pid;
	else
		return (sender->p_parent)->p_pid;
}

/**
 * @brief Handles the SSI requests during SSILoo, dispatching the actual service called
 * 		  and returning the address of the eventual process.
 *
 * @param sender the process that requested the service
 * @param payload the payload of the message
 * @param arg the argument of the message (not always needed)
 * @return unsigned int the result of the service
 */
unsigned int SSIRequest(pcb_PTR sender, unsigned int service, void *arg)
{
	unsigned int res = OFF;
	switch (service)
	{
	case CREATEPROCESS:
		res = createProcess(sender, (ssi_create_process_PTR)arg);
		break;
	case TERMPROCESS:
		if (arg != NULL) /* this case suggest that the process sent as payload should be killed */
			terminateProcess(arg);
		else /* if the argument is NULL, then the process that requested the service must be terminated */
			terminateProcess(sender);
		break;
	case DOIO:
		doio(arg, sender);
		res = NOPROC;
		break;
	case GETTIME:
		res = getCPUTime(sender);
		break;
	case CLOCKWAIT:
		wait4Clock(sender);
		res = NOPROC;
		break;
	case GETSUPPORTPTR:
		res = getSupportData(sender);
		break;
	case GETPROCESSID:
		res = getProcessID(sender, arg);
		break;
	default:
		terminateProcess(sender);
		res = MSGNOGOOD;
		break;
	}
	return res;
}

/**
 * @brief The SSI service. It is responsible for handling the SSI requests.
 * 		  If everything goes well, the SSI loop will send a message to the process that requested the service.
 * 		  If SSI ever gets terminated, the system must be stopped performing an emergency shutdown.
 *
 * @param void
 * @return void
 */
void SSI()
{ /* The idea is that this process constantly listens to requests, and then it responds */
	while (1)
	{
		unsigned int *senderAddr, result;
		pcb_PTR payload;

		SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		senderAddr = (unsigned int *)EXCEPTION_STATE->reg_v0;
		/* When a process requires a SSI service it must wait for an answer, so we use the blocking synchronous recv */
		ssi_payload_PTR ssipyld = (ssi_payload_PTR)payload;
		result = SSIRequest((pcb_PTR)senderAddr, ssipyld->service_code, ssipyld->arg);
		if (result != NOPROC) /* NOPROC is provided when requesting service that doesn't provide any pcb */
		{					  /* everything went fine, so we obtained the result of the request, now send it back*/
			SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, result, 0);
		}
	}
}
