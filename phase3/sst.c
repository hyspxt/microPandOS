#include "./headers/lib.h"

/**
 * @brief The SST service. It is responsible for handling the SSI requests.
 * 		  If everything goes well, the SSI loop will send a message to the process that requested the service.
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
    case TERMINATE:
        terminate(asid);
        res = ON;
        
    case WRITEPRINTER:

    case WRITETERMINAL:

	default:
		terminateProcess(sender);
		res = MSGNOGOOD;
		break;
	}
	return res;
}