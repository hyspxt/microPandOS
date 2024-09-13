#include "./headers/lib.h"

/*
    This module contains the services provided by the SST process.
    These can be requested by the UProc children.
*/

/**
 * @brief Terminate the calling process. Wrapper for the 
 *        TerminateProcess service. Causes the termination of the
 *        SST process and its UProc child.
 *
 * @param int asid - address identifier
 * @return void
 */
void terminate(int asid)
{ /* in case, free the frames */
    for (int i = 0; i < POOLSIZE; i++){
        if (swapPoolTable[i].sw_asid == asid) 
            swapPoolTable[i].sw_asid = NOASID;
    } /* notify the termination */
    SYSCALL(SENDMESSAGE, (unsigned int) testPcb, 0, 0);
    /* since a TerminateProcess kill also the process progeny 
    recursively, one call (that kills the caller) is sufficient */
    sendKillReq(printerPcbs[asid]);
    sendKillReq(terminalPcbs[asid]);
    sendKillReq(NULL); /* kill the SST */
    /* if it's NULL, SSI terminate the sender and its UPROC child */
}

/**
 * @brief This service cause the print of a string of characters to a printer device.
 *        Sender must wait an empty response from the SST.
 *
 * @param int asid - the address space identifier
 * @param sst_print_ptr print - the struct containing the string and its length
 * @return void
 */
void writePrinter(int asid, sst_print_PTR print)
{ /* the empty response is sent in SST() */
    SYSCALL(SENDMESSAGE, (unsigned int)printerPcbs[asid], (unsigned int) print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)printerPcbs[asid], 0, 0);
}

/**
 * @brief This service cause the print of a string of characters to terminal device.
 *        Sender must wait an empty response from the SST.
 *
 * @param asid - the address space identifier
 * @param sst_print_ptr print - the struct containing the string and its length
 * @return void
 */
void writeTerminal(int asid, sst_print_PTR print)
{ /* the empty response is sent in SST() */
    SYSCALL(SENDMESSAGE, (unsigned int)terminalPcbs[asid], (unsigned int)print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)terminalPcbs[asid], 0, 0);
}

/**
 * @brief Handles the SST requests during SST loop, dispatching the actual service called
 * 		  and returning a value / ACK.
 *
 * @param pcb_PTR sender - the child that requested the service
 * @param unsigned int service - code of the service requested
 * @param void *arg - the argument of the message (used only in writeX)
 * @param int asid - the asid to determine which child process is calling the service
 * @return unsigned int - service result
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
        terminate(asid); 
        res = ON;
    case WRITEPRINTER:
        writePrinter(asid, (sst_print_PTR) arg); 
        res = ON;
        break;
    case WRITETERMINAL:
        writeTerminal(asid, (sst_print_PTR) arg);
        res = ON;
        break;
	default:
		terminate(asid);
        res = ON;
		break;
	}
	return res;
}

/**
 * @brief The SST service. It is responsible for handling the SST requests.
 * 		  It follows the protocol listen -> call a service -> reply with result.
 *        (similar to SSI, but support level).
 *
 * @param void
 * @return void
 */
void SST()
{ /* listens to requests done by its children, and then it responds */
    /* get the structures to creating child process */
    support_t *sstSup = getSupStruct();
    state_t *sstState = &uProcState[sstSup->sup_asid - 1];
    /* create the child */
    uproc[sstSup->sup_asid - 1] = create_process(sstState, sstSup);
    
	while (1)
	{   /* SST children (UPROC) must wait for an answer */
		unsigned int senderAddr, result;
		pcb_PTR payload;
		senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);

        /* mind that this is a SST payload, despite the struct it's the same */
		ssi_payload_PTR sstpyld = (ssi_payload_PTR)payload;
		result = SSTRequest((pcb_PTR)senderAddr, sstpyld->service_code, sstpyld->arg, sstSup->sup_asid - 1);
        /* everything went fine, so we obtained the result of SST the request, now send it back*/
	    SYSCALL(SENDMESSAGE, senderAddr, result, 0);
	}
}
