#include "./headers/lib.h"


/**
 * @brief Terminate the calling process. Wrapper for the 
 *        TerminateProcess service. Causes the termination of the
 *        SST process and its UProc child.
 *
 * @param void
 * @return void
 */
void terminate(){
    /* since a TerminateProcess kill also the process progeny 
    recursively, one call (that kills the caller) is sufficient */
    ssi_payload_t sst_payload = {
        .service_code = TERMINATE,
        .arg = NULL,
    };

    /* SST termination */
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&sst_payload, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
    /* communicate the termination to test */
    SYSCALL(SENDMESSAGE, (unsigned int) testPcb, 0, 0);
}

/**
 * @brief This service cause the print of a string of characters to the printer device.
 *        Sender must wait an empty response from the SST.
 *
 * @param asid - the address space identifier
 * @param print - the struct containing the string and its length
 * @return void
 */
void writePrinter(int asid, sst_print_PTR print)
{ /* the empty response is sent in SST() */
    SYSCALL(SENDMESSAGE, (unsigned int) printerPcbs[asid], (unsigned int)print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int) printerPcbs[asid], 0, 0);
}

/**
 * @brief This service cause the print of a string of characters to terminal device.
 *        Sender must wait an empty response from the SST.
 *
 * @param asid - the address space identifier
 * @param print - the struct containing the string and its length
 * @return void
 */
void writeTerminal(int asid, sst_print_PTR print)
{ /* the empty response is sent in SST() */
    SYSCALL(SENDMESSAGE, (unsigned int) terminalPcbs[asid], (unsigned int)print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int) terminalPcbs[asid], 0, 0);
}



/**
 * @brief The SST service. It is responsible for handling the SST requests.
 * 		  
 * 		  If SSI ever gets terminated, the system must be stopped performing an emergency shutdown.
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
`
    /* create the child */
    pcb_PTR sst_child = create_process(sstState, sstSup);

	while (1)
	{   /* SST child must wait for an answer */
		unsigned int *senderAddr, result;
		pcb_PTR payload;

		SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		senderAddr = (unsigned int *)EXCEPTION_STATE->reg_v0;

        /* no typing differencce actually */
		ssi_payload_PTR sstpyld = (ssi_payload_PTR)payload;
		result = SSTRequest((pcb_PTR)senderAddr, sstpyld->service_code, sstpyld->arg, sup->sup_asid);
		if (result != NOPROC) 
		{ /* everything went fine, so we obtained the result of SST the request, now send it back*/
			SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, result, 0);
		}
	}
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
        terminate();
        res = ON;
    case WRITEPRINTER:
        writePrinter(asid - 1, (sst_print_PTR) arg);
        res = ON;
        break;
    case WRITETERMINAL:

	default:
		terminateProcess(sender);
		res = MSGNOGOOD;
		break;
	}
	return res;
}