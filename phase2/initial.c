#include "./headers/lib.h"

pcb_PTR current_process;
unsigned int processCount, softBlockCount;
unsigned int startTOD;

/* Queue of PCBs that are in READY state. It is a tail pointer. */
struct list_head readyQueue;

struct list_head blockedDiskQueue, blockedFlashQueue, blockedEthernetQueue, blockedPrinterQueue, blockedTerminalTransmQueue, blockedTerminalRecvQueue;
struct list_head pseudoClockQueue;

unsigned int pidCount;

pcb_PTR ssi_pcb, new_pcb;


void uTLB_RefillHandler()
{
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)0x0FFFF000);
}

/**
 * @brief Copies all values from the PCB state source to dest.
 *          
 * @param void
 * @return int
 */
void stateCpy(state_t *src, state_t *dest){
    dest->status = src->status;
	dest->pc_epc = src->pc_epc;
    dest->cause = src->cause;
    dest->entry_hi = src->entry_hi;
    dest->hi = src->hi;
	dest->lo = src->lo;
	for(int i=0; i<STATE_GPR_LEN; i++)
		dest->gpr[i] = src->gpr[i];
}

void stateCPY4debug(state_t *src, state_t *dest){
    klog_print("Copying...\n");

    if (dest != NULL)
        klog_print("dest is not NULL\n");

    if (src != NULL)
        klog_print("src is not NULL!\n");

    if (dest->status == 0)
        klog_print("dest->status is not NULL\n");
    if (src->status == 0)
        klog_print("src->status is not NULL\n");

    dest->status = src->status;
    klog_print("1...\n");
	dest->pc_epc = src->pc_epc;
    klog_print("2...\n");
    dest->cause = src->cause;
    klog_print("3...\n");
    dest->entry_hi = src->entry_hi;
    klog_print("4...\n");
    dest->hi = src->hi;
    klog_print("5...\n");
	dest->lo = src->lo;
    klog_print("6...\n");
	for(int i=0; i<STATE_GPR_LEN; i++)
		dest->gpr[i] = src->gpr[i];
    klog_print("Finish all ok\n");
}


/**
 * @brief This module contains the microPandOS entry point, that is the Nucleus initialization.
 *        after the initialization, the Nucleus will call the scheduler.
 *          
 * @param void
 * @return int
 */
int main()
{
    /* Here we should populate the processor 0 - Pass Up Vector */
    passupvector_t *pUV = (passupvector_t *)PASSUPVECTOR;
    pUV->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    pUV->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
    pUV->exception_handler = (memaddr)exceptionHandler;
    pUV->exception_stackPtr = (memaddr)KERNELSTACK;

    /* Initialize the structures defined in phase1 */
    initPcbs();
    initMsgs();

    /* Global variables initializations */
    processCount = 0;
    softBlockCount = 0;
    current_process = NULL;
    pidCount = 0;

    mkEmptyProcQ(&readyQueue);
    /* Blocked PCBs Queues */
      /* blocked PCBs for each external (sub)device*/
    mkEmptyProcQ(&blockedDiskQueue);
    mkEmptyProcQ(&blockedFlashQueue);
    mkEmptyProcQ(&blockedEthernetQueue);
    mkEmptyProcQ(&blockedPrinterQueue);
    mkEmptyProcQ(&blockedTerminalTransmQueue);
    mkEmptyProcQ(&blockedTerminalRecvQueue);
      /* queue of waiting PCBs that requested a WaitForClock service to the SSI */
    mkEmptyProcQ(&pseudoClockQueue);


    /* Load system-wide interval timer with 100000 ms */
    LDIT(PSECOND); 

    /* Instantiate the first PCB, the SSI with pid 1
         This one need to have interrupts enabled, kernel-mode on, the SP set to RAMTOP 
         and the PC set to the address of the SSI_function_entry_point */
    ssi_pcb = NULL;
    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 1;
    pidCount++;

    /* IEc -> represents the CURRENT interrupt enable bit 
       KUc > represents the CURRENT state of Kernel/User mode
    It is important to mention that to setting up a new processor state, we should set 
    PREVIOUS bits and not CURRENT ones (so basically, IEp and KUp counterparts).
    */
    ssi_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON; // kernel mode is by default when KUc = 0
    RAMTOP(ssi_pcb->p_s.reg_sp);
    ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9  = (memaddr) SSILoop;
    insertProcQ(&readyQueue, ssi_pcb);
    processCount++;
    
    /* Instantiate the test PCB, with pid 2
        This one need to have interrupts enabled, processor local-timer enabled, the SP set to RAMTOP 
        and the PC set to the address of test */
    new_pcb = NULL;
    new_pcb = allocPcb();
    new_pcb->p_pid = 2;
    pidCount = 2;
    new_pcb->p_s.status = ALLOFF | IEPON | IMON | TEBITON; // Enable interrupts, set interrupt mask to all 1s, enable PLT 
    RAMTOP(new_pcb->p_s.reg_sp);
    new_pcb->p_s.reg_sp -= 2 * PAGESIZE; // FRAMESIZE according to specs??
    new_pcb->p_s.pc_epc = new_pcb->p_s.reg_t9  = (memaddr) test;
    insertProcQ(&readyQueue, new_pcb);
    processCount++;

    scheduler();
    return 0;
}