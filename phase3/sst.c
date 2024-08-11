#include "./headers/lib.h"


/**
 * @brief Terminate the calling process. Wrapper for the 
 *        TerminateProcess service. Causes the termination of the
 *        SST process and its UProc child.
 *
 * @param void
 * @return void
 */
void terminate(int asid)
{ /* in case, free the frame */
    for (int i = 0; i < POOLSIZE; i++){
        if (swapPoolTable[i].sw_asid == asid)
            swapPoolTable[i].sw_asid = NOASID;
    }
    SYSCALL(SENDMESSAGE, (unsigned int) testPcb, 0, 0);
    /* since a TerminateProcess kill also the process progeny 
    recursively, one call (that kills the caller) is sufficient */
    sendKillReq(printerPcbs[asid]);
    sendKillReq(terminalPcbs[asid]);
    sendKillReq(NULL); /* kill the SST */
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

    dev_payload_t payload = {
        .asid = asid,
        .device = IL_PRINTER,
        .print = print,
    };
    // SYSCALL(SENDMESSAGE, (unsigned int)terminalPcbs[asid], (unsigned int)(&payload), 0);
    SYSCALL(SENDMESSAGE, (unsigned int)printerPcbs[asid], (unsigned int) print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)printerPcbs[asid], 0, 0);
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
    dev_payload_t payload = {
        .asid = asid,
        .device = IL_TERMINAL,
        .print = print,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)terminalPcbs[asid], (unsigned int) print->string, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)terminalPcbs[asid], 0, 0);
}

unsigned int sst_write(unsigned int asid, unsigned int device_type,
                       sst_print_t *payload) {
  if (payload->string[payload->length] != '\0')
    payload->string[payload->length] = '\0';

  pcb_t *dest = NULL;
  switch (device_type) {
  case 6:
    dest = printerPcbs[asid];
    break;
  case 7:
    dest = terminalPcbs[asid];
    break;
  default:
    break;
  }

  SYSCALL(SENDMESSAGE, (unsigned int)dest, (unsigned int)payload->string, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)dest, 0, 0);

  return 1;
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
        terminate(asid); /* notify the test */
        res = ON;
    case WRITEPRINTER: /* asid - 1 cause the param should be used as a index */
        // writePrinter(asid, (sst_print_PTR) arg, sender); 
        // res = ON;
        res = sst_write(asid, 6, (sst_print_t *)arg);
        break;
    case WRITETERMINAL:
        // writeTerminal(asid, (sst_print_PTR) arg, sender);
        // res = ON;
        res =  sst_write(asid, 7, (sst_print_t *)arg);
        break;
	default:
		terminate(asid);
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
	    SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, result, 0);
	}
}
