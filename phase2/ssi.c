#include "./headers/lib.h"

// if SSI ever gets terminated, the system must be stopped performing an emergency shutdown

unsigned int createProcess(ssi_create_process_PTR sup, pcb_PTR parent)
{
	pcb_PTR new_pcb = allocPcb();
	if (new_pcb == NULL)
	{
		/* There aren't any PCBs available */
		// sender->p_s.reg_v0 = NOPROC;
		return NOPROC;
	}
	else {
		/* There is at least one PCB available */
		processCount++;
		stateCpy(sup->state, &new_pcb->p_s);
		new_pcb->p_supportStruct = sup->support;
		insertProcQ(&readyQueue, new_pcb);
		insertChild(parent, new_pcb);
		return (unsigned int) new_pcb;;
	}
}

void terminateProcess(pcb_PTR sender)
{
	// TODO 
}


void SSILoop(){
	while(TRUE){
		unsigned int senderAddr, result;
		unsigned int payload;

		senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int) &payload, 0);  // TODO keep an eye on this casting
		/* When a process requires a SSI service it must wait for an answer, so we use the blocking synchronous recv 
		The idea behind using this system comes from the statement: "SSI request should be implemented using SYSCALLs and message passing" in specs. */
		result = SSIRequest((pcb_PTR) senderAddr, (ssi_payload_t *) payload);

		if (result != NOPROC){
			SYSCALL(SENDMESSAGE, senderAddr, result, 0);
		}
	}
}

unsigned int SSIRequest(pcb_PTR sender, ssi_payload_PTR payload){
	unsigned int code, res = 0;
	code = payload->service_code;


	switch(code){
		case CREATEPROCESS:
			res = createProcess(payload->arg, sender);
			break;
		case TERMPROCESS:
			if (payload->arg == NULL){  // if the argument is NULL, then the process that requested the service must be terminated
				res = NOPROC;
				terminateProcess(sender);
			}
			else{
				res = 0;
				terminateProcess(payload->arg);
			}
			break;
	}
	return res;
}