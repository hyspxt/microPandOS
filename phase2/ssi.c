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
		processCount++;
		stateCpy(sup->state, &new_pcb->p_s);
		new_pcb->p_supportStruct = sup->support;
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
		processCount--;
		outChild(sender);
		freePcb(sender);
	}
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
	for (int deviceLine = 0; deviceLine < DEVPERINT; deviceLine++){

		/* DEV_REG_ADDR(..) is a macro defined in arch.h that return the adress of the register of the device, using
		its type and its identifier. MAX 8 instances supported by default. 
		Terminal devices are identified by the IL_TERMINAL constant, that is 7. */
		termreg_t *termreg = (termreg_t *)DEV_REG_ADDR(IL_TERMINAL, deviceLine);
		if((memaddr) &termreg->transm_command == addr) {
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalTransmQueue, sender);
		}
		else if ((memaddr) &termreg->recv_command == addr) {
			outProcQ(&readyQueue, sender);
			insertProcQ(&blockedTerminalRecvQueue, sender);
		}
		return;
	}

	/* If the device is not a terminal device, then it is a disk, flash, ethernet or printer device 
	so, we iter on those basing on their identifiers. 
	According to uMPS3 - Principles of Operation p.29 devices have interrupts pending on interrupt lines 3-7.
	That means that when bit i in word j is set to one, then device i attached to interrupt line j + 3 has a pending interrupt. */
	for (int iter_dev = 3; iter_dev < IL_TERMINAL; iter_dev++){
		for (int deviceLine = 0; deviceLine < DEVPERINT; deviceLine++){
			dtpreg_t *dtpreg = (dtpreg_t *)DEV_REG_ADDR(iter_dev, deviceLine);
			if ((memaddr) &dtpreg->command == addr)
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

		senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0); // TODO keep an eye on this casting
		/* When a process requires a SSI service it must wait for an answer, so we use the blocking synchronous recv
		The idea behind using this system comes from the statement: "SSI request should be implemented using SYSCALLs and message passing" in specs. */
		result = SSIRequest((pcb_PTR)senderAddr, (ssi_payload_t *)payload);

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
	unsigned int code, res = 0;
	code = payload->service_code;

	switch (code)
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
		{
			terminateProcess(payload->arg);
		}
		break;
	case DOIO:
		/* TODO implement the IO service */
		break;
	}
	return res;
}