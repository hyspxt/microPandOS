#include "./headers/lib.h"

/*
 * @brief Creates a child process of the parent process. Then, the child process is inserted in the ready queue.
 *
 * @param sup the support structure of the new process
 * @param parent the parent process
 * @return int the address of the new process
 */
unsigned int createProcess(ssi_create_process_PTR sup, pcb_PTR parent)
{
	pcb_PTR new_pcb = allocPcb();
	if (new_pcb == NULL)
	{
		/* There aren't any PCBs available */
		// sender->p_s.reg_v0 = NOPROC;
		return NOPROC;
	}
	else
	{
		/* There is at least one PCB available */
		new_pcb->p_supportStruct = sup->support;
		new_pcb->p_time = 0;
		new_pcb->p_pid = pidCount++;
		;
		processCount++;
		stateCpy(sup->state, &new_pcb->p_s);

		insertProcQ(&readyQueue, new_pcb);
		insertChild(parent, new_pcb);
		return (unsigned int)new_pcb;
	}
}

/**
 * @brief Terminates/kill a process. If the process is the one that requested the service, then the process is terminated.
 * 		  When a process is terminated, in addiction all progeny of that process must be terminated.
 *
 * @param sender the process that requested the service
 * @return void
 */
void terminateProcess(pcb_PTR sender)
{
	if (sender != NULL)
	{
		/* iteratively, we explore the PCB tree structure using sender as a root, then recursively we kill his PCB childrenn.
		   This continue while sender has any children.*/
		while (!emptyChild(sender))
		{
			terminateProcess(removeChild(sender));
		}
	}
	processCount--;

	if (outProcQ(&blockedTerminalTransmQueue, sender) != NULL ||
		outProcQ(&blockedTerminalRecvQueue, sender) != NULL ||
		outProcQ(&blockedDiskQueue, sender) != NULL ||
		outProcQ(&blockedFlashQueue, sender) != NULL ||
		outProcQ(&blockedEthernetQueue, sender) != NULL ||
		outProcQ(&blockedPrinterQueue, sender) != NULL ||
		outProcQ(&pseudoClockQueue, sender) != NULL)
	{
		softBlockCount--;
	}

	outChild(sender);
	freePcb(sender);
}

/**
 * @brief Calculate the device number and the interrupt line of the device that the process is waiting for.
 *
 * @param doioPTR the IO request
 * @param sender the process that requested the service
 * @return void
 */
void deviceRequest(pcb_PTR sender, memaddr addr)
{
	/* We need to distinguish betweeen terminal and not-terminal devices, that are transm and recv
	so, according to uMPS3 - Principles of Operation, each device is identified by the interrupt line
	it is attached to and its device number, an integer in the range 0-7. */
	for (int deviceLine = 0; deviceLine < DEVPERINT; deviceLine++)
	{

		/* DEV_REG_ADDR(..) is a macro defined in arch.h that return the adress of the register of the device, using
		its type and its identifier. MAX 8 instances supported by default.
		Terminal devices are identified by the IL_TERMINAL constant, that is 7. */
		termreg_t *termreg = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, deviceLine);
		if ((memaddr)&termreg->transm_command == addr)
		{
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalTransmQueue, sender);
		}
		else if ((memaddr)&termreg->recv_command == addr)
		{
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalRecvQueue, sender);
		}
		return;
	}

	/* If the device is not a terminal device, then it is a disk, flash, ethernet or printer device
	so, we iter on those basing on their identifiers.
	According to uMPS3 - Principles of Operation p.29 devices have interrupts pending on interrupt lines 3-7.
	That means that when bit i in word j is set to one, then device i attached to interrupt line j + 3 has a pending interrupt. */
	for (int iter_dev = 3; iter_dev < IL_TERMINAL; iter_dev++)
	{
		for (int deviceLine = 0; deviceLine < DEVPERINT; deviceLine++)
		{
			dtpreg_t *dtpreg = (dtpreg_t *)DEV_REG_ADDR(iter_dev, deviceLine);
			if ((memaddr)&dtpreg->command == addr)
			{
				outProcQ(&readyQueue, sender);
				switch (iter_dev)
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
				return;
			}
		}
	}
}

/**
 * @brief Returns the TOD (Time Of Day) clock value.
 *
 * @param sender the process that requested the service
 * @return unsigned int the CPU time of the process
 */
unsigned int getTOD()
{
	unsigned int TOD_LOAD;
	return (STCK(TOD_LOAD));
}

/**
 * @brief Allow the sender to get back the accumulated processor time used by the sender process.
 * 		  Hence, the nucleus records in sender->p_time the amount of processor time used by each process.
 *
 * @param sender the process that requested the service
 * @return unsigned int the CPU time of the process
 */
unsigned int getCPUTime(pcb_PTR sender)
{
	return (sender->p_time);
}

/**
 * @brief Update the CPU time of the process that requested the service.
 * 		  Start is called when the process is dispatched, while updateCPUTime is called when the process is preempted.
 *
 * @param sender the process that requested the service
 * @return void
 */
void updateCPUTime(pcb_PTR sender)
{
	unsigned int endTOD = getTOD();
	sender->p_time += endTOD - startTOD;
	startTOD = endTOD;
}

/**
 * @brief Set the Processor Local Timer described at p.22 of uMPS3 - Principles of Operation.
 *
 * @param time the time to set the timer
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
 *
 * @param sender the process that requested the service
 * @return unsigned int the CPU time of the process
 */
void wait4Clock(pcb_PTR sender)
{
	insertProcQ(&pseudoClockQueue, sender);
	softBlockCount++;
}

/**
 * @brief Allow the sender to get to obtain the process Support Structure.
 * 		  The support structure is a data structure that contains the information needed by the SSI to perform the service requested by the process.
 *
 * @param sender the process that requested the service
 * @return void
 */
unsigned int getSupportData(pcb_PTR sender)
{
	/* The support structure is a data structure that contains the information needed by the SSI to perform the service requested by the process. */
	return (unsigned int)sender->p_supportStruct;
}

/**
 * @brief Allow the sender to get the process ID of the process that requested the service.
 * 		  If arg is 0 return the sender's pid, or otherwise the sender's parent pid.
 * @param sender the process that requested the service
 * @return unsigned int the process ID
 */
unsigned int getProcessID(pcb_PTR sender, pcb_PTR arg)
{
	if (arg == NULL)
		return sender->p_pid;
	else
		return sender->p_parent->p_pid;
}

/**
 * @brief Handles the synchronous I/O requests. T
 *
 * @param doioPTR the IO request
 * @param sender the process that requested the service
 * @return void
 */
void doio(ssi_do_io_PTR doioPTR, pcb_PTR sender)
{

	/* if a process request the DOIO service, it must be blocked on the correct device */
	softBlockCount++;
	deviceRequest(sender, (memaddr)doioPTR->commandAddr); // save the waiting pcb in the correct device queue
	*(doioPTR->commandAddr) = doioPTR->commandValue;
}

/**
 * @brief The SSI loop. It is responsible for handling the SSI requests.
 * 		  If everything goes well, the SSI loop will send a message to the process that requested the service.
 * 		  If SSI ever gets terminated, the system must be stopped performing an emergency shutdown.
 *
 * @param void
 * @return void
 */
void SSILoop()
{
	while (TRUE)
	{
		unsigned int senderAddr, result;
		unsigned int payload;

		senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		// TODO for some strange reasons, it is blocking here

		/* When a process requires a SSI service it must wait for an answer, so we use the blocking synchronous recv
		The idea behind using this system comes from the statement: "SSI request should be implemented using SYSCALLs and message passing" in specs. */
		result = SSIRequest((pcb_PTR)senderAddr, (ssi_payload_PTR)payload);
		// pretty sure that the problem is here ------------

		if (result != NOPROC)
		{
			SYSCALL(SENDMESSAGE, senderAddr, result, 0);
		}
	}
}

/**
 * @brief Handles the SSI requests during SSILoop.
 *
 * @param sender the process that requested the service
 * @param payload the payload of the message
 * @return unsigned int the result of the service
 */
unsigned int SSIRequest(pcb_PTR sender, ssi_payload_PTR payload)
{
	unsigned int res = 0;

	switch (payload->service_code)
	{
	case CREATEPROCESS:
		res = createProcess(payload->arg, sender);
		break;
	case TERMPROCESS:
		if (payload->arg == NULL)
		{ // if the argument is NULL, then the process that requested the service must be terminated
			res = NOPROC;
			terminateProcess(sender);
		}
		else
			terminateProcess(payload->arg);
		break;
	case DOIO:
		doio(payload->arg, sender);
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
		res = getProcessID(sender, payload->arg);
		break;
	default:
		res = MSGNOGOOD;
		terminateProcess(sender);
		break;
	}
	return res;
}