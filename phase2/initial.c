#include "./headers/lib.h"
// global variables declarations
pcb_PTR current_process;
struct list_head readyQueue;
unsigned int processCount, softBlockCount;
pcb_PTR blockedPcb_list[SEMDEVLEN - 1], waitPClock_list[SEMDEVLEN - 1];

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

    initPcbs();
    initMsgs();

    processCount = 0;
    softBlockCount = 0;
    mkEmptyProcQ(&readyQueue);
    current_process = NULL;
    for (int i = 0; i < SEMDEVLEN; i++)
    {
        blockedPcb_list[i] = NULL;
        waitPClock_list[i] = NULL;
    }

    LDIT(PSECOND); // load system-wide interval timer with 100000 ms

    // instantiate the init process
    pcb_PTR init = allocPcb();

    /*
    first of all, let's set up the processor state for the init process
    In order:
    - ALLOFF set all the bits to 0 (ergo reset everything)
    - IEPON set the Interrupt Enable Previous bit to 1 (interrupts are enabled)
    - TEBITON set the Timer Interrupt bit to 1 (timer interrupt is enabled)
    */
    init->p_s.status = ALLOFF | IEPON | IECON | TEBITON;
    // set SP to RAMTOP and PC to the address of SSI_function_entry_point
    RAMTOP (init->p_s.reg_sp);
   
    // init->p_s.reg_t9 = init->p_s.pc_epc = (memaddr)test;
    init->p_s.pc_epc = init->p_s.reg_t9  = (memaddr) test;

    // set all process tree fields to NULL
    init->p_parent = NULL;
    init->p_child.next = NULL;
    init->p_child.prev = NULL; 
    init->p_sib.next = NULL;
    init->p_sib.prev = NULL;

    init->p_time = 0;
    init->p_supportStruct = NULL;

    // place its pcb in the ready queue and increment process count
    insertProcQ(&readyQueue, init);
    processCount++;

    return 0;
}