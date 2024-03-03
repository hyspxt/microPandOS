#include "./headers/lib.h"
// global variables declarations
pcb_PTR current_process;
struct list_head readyQueue;
unsigned int processCount, softBlockCount;
pcb_PTR blockedPcb_list[SEMDEVLEN - 1], waitPClock_list[SEMDEVLEN - 1];

pcb_PTR ssi_pcb;

extern void test();
void uTLB_RefillHandler()
{
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)0x0FFFF000);
}

int main()
{
    // here we should populate the processor 0 - Pass Up Vector
    passupvector_t *pUV = (passupvector_t *)PASSUPVECTOR;
    pUV->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    pUV->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
    // pUV->exception_handler = (memaddr)exceptionHandler;
    // pUV->exception_stackPtr = (memaddr)KERNELSTACK;

    // initialize the data structures defined in phase1
    initPcbs();
    initMsgs();

    // initialize the global variables
    processCount = 0;
    softBlockCount = 0;
    mkEmptyProcQ(&readyQueue);
    current_process = NULL;
    mkEmptyProcQ(&blockedPcb_list);
    mkEmptyProcQ(&waitPClock_list);
    ssi_pcb = NULL;

    // load system-wide interval timer with 100000 ms
    LDIT(PSECOND); 

    // instantiate the SSI PCB
    ssi_pcb = allocPcb();
    ssi_pcb->p_pid = 0;
    ssi_pcb->p_s.status |= ALLOFF | IEPON | IMON | USERPON; // interrupts enabled setting mask bit to 1
    RAMTOP (ssi_pcb->p_s.reg_sp); // set SP to RAMTOP and PC to the address of SSI_function_entry_point
    ssi_pcb->p_s.pc_epc = ssi_pcb->p_s.reg_t9  = (memaddr) initSSI;
    // init->p_s.reg_t9 = init->p_s.pc_epc = (memaddr)test;
    insertProcQ(&readyQueue, ssi_pcb);
    processCount++;
    
    // instantiate the test PCB
    pcb_PTR test_pcb = allocPcb();
    test_pcb->p_pid = 1;
    test_pcb->p_s.status |= IEPON | IMON | TEBITON | USERPON; // Enable interrupts, set interrupt mask to all 1s, enable PLT              // Enable kernel mode
    RAMTOP(test_pcb->p_s.reg_sp);
    test_pcb->p_s.reg_sp -= 2 * PAGESIZE; // NOTE: specs say FRAMESIZE???
    test_pcb->p_s.pc_epc = test_pcb->p_s.reg_t9  = (memaddr) test;
    insertProcQ(&readyQueue, test_pcb);
    processCount++;

    return 0;
}