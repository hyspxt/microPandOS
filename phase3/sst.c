#include "./headers/lib.h"


/**
 * @brief Terminate the calling process. Wrapper for the 
 *        TerminateProcess service. Causes the termination of the
 *        SST process and its UProc child.
 *
 * @param void
 * @return void
 */
void terminate(int asid, int notify)
{ /* in case, free the frame */
    for (int i = 0; i < POOLSIZE; i++){
        if (swapPoolTable[i].sw_asid == asid)
            swapPoolTable[i].sw_asid = NOASID;
    }
    SYSCALL(SENDMESSAGE, (unsigned int) testPcb, 0, 0);
    /* since a TerminateProcess kill also the process progeny 
    recursively, one call (that kills the caller) is sufficient */
    sendKillReq(); /* kill the SST */
    /* if it's NULL, SSI terminate the sender and its UPROC child */
}

/**
 * @brief This service cause the print of a string of characters to the printer device.
 *        Sender must wait an empty response from the SST.
 *
 * @param asid - the address space identifier
 * @param print - the struct containing the string and its length
 * @return void
 */
void writePrinter(int asid, sst_print_PTR print, pcb_PTR sender)
{ /* the empty response is sent in SST() */
    if (print->string[print->length] != EOS)
        print->string[print->length] = EOS;
    printDevice(asid, IL_PRINTER, print);
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
}

/**
 * @brief This service cause the print of a string of characters to terminal device.
 *        Sender must wait an empty response from the SST.
 *
 * @param asid - the address space identifier
 * @param print - the struct containing the string and its length
 * @return void
 */
void writeTerminal(int asid, sst_print_PTR print, pcb_PTR sender)
{ /* the empty response is sent in SST() */
    if (print->string[print->length] != EOS)
        print->string[print->length] = EOS;
    printDevice(asid, IL_TERMINAL, print);
    
}


/**
 * @brief Handles the SST requests during SST loop, dispatching the actual service called
 * 		  and returning the address of the eventual process.
 *
 * @param sender the child that requested the service
 * @param payload the payload of the message
 * @param arg the argument of the message (not always needed)
 * @param asid the asid to determine which child process is calling the service
 * @return unsigned int the result of the service
 */
unsigned int SSTRequest(pcb_PTR sender, unsigned int service, void *arg, int asid)
{   /* can't use 0 for non-returning service -> interpreted as NULL pyld! */
	unsigned int res = OFF;
	switch (service)
	{
	case GET_TOD: /* returns the TOD clock value */
        res = getTOD();
        break;
    case TERMINATE: /* this should kill both the Uproc and the SST */
        terminate(asid, ON); /* notify the test */
        res = ON;
    case WRITEPRINTER: /* asid - 1 cause the param should be used as a index */
        writePrinter(asid, (sst_print_PTR) arg, sender); 
        res = ON;
        break;
    case WRITETERMINAL:
        writeTerminal(asid, (sst_print_PTR) arg, sender);
        res = ON;
        break;
	default:
		terminate(asid, ON);
		res = ON;
		break;
	}
	return res;
}

/**
 * @brief The SST service. It is responsible for handling the SST requests.
 * 		  It follows the protocol listen -> call a service -> reply with result.
 *
 * @param void
 * @return void
 */
void SST()
{ /* The idea is that this process constantly listens to 
    requests done by its children, and then it responds */
    /* get the structures to creating child process */
    support_t *sstSup = getSupStruct();
    int asidIndex = sstSup->sup_asid - 1;
    state_t *sstState = &uProcState[asidIndex];

    /* create the child */
    uproc[asidIndex] = create_process(sstState, sstSup);
	while (1)
	{   /* SST children (UPROC) must wait for an answer */
		unsigned int senderAddr, result;
		pcb_PTR payload;
		senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);

        /* mind that this is a SST payload, despite the struct it's the same */
		ssi_payload_PTR sstpyld = (ssi_payload_PTR)payload;
		result = SSTRequest((pcb_PTR)senderAddr, sstpyld->service_code, sstpyld->arg, asidIndex);
		if (result != NOPROC) 
		{ /* everything went fine, so we obtained the result of SST the request, now send it back*/
			SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, result, 0);
		}
	}
}
