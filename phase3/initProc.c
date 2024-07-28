#include "./headers/lib.h"

/* information that should be added to the Support 
structure that is pointed by a U-Proc. To be created, a 
PCB needs its state and support struct */
state_t uProcState[UPROCMAX];
support_t supStruct[UPROCMAX]; /* support struct that will contain page table */
/* each swap pool is a set of RAM frames, reserved for vm */
swap_t swapPoolTable[POOLSIZE];
pcb_PTR swap_mutex;

/* sharable peripheral I/O (printer with spooling 
and terminal) */
pcb_PTR printerPcbs[UPROCMAX];
pcb_PTR terminalPcbs[UPROCMAX];

pcb_PTR testPcb;
unsigned int current_memaddr;

/**
 * @brief Initialize the swap pool table, by putting a default value
 *        in swap_t structure fields. 
 *          
 * @param void
 * @return void
 */
void initSwapTable(){
    for (int i = 0; i < POOLSIZE; i++){
        swapPoolTable[i].sw_asid = OFF; /* 6bit identifier
          to distinguish the process' kuseg from the others*/
        swapPoolTable[i].sw_pageNo = OFF; /* sw_pageNo is the
          virtual page number in the swap pool.*/
        swapPoolTable[i].sw_pte = NULL; /* pointer to the page table entry */
        /* TODO check if this causes problems */
    }
}

/**
 * @brief Initialize a single user process setting up its fields.
 *          
 * @param int asid - the address space identifier
 * @return void
 */
void initUProc(int asid){
    /* program counter set to start .text section */
    uProcState[asid].pc_epc = uProcState[asid].reg_t9 = (memaddr) UPROCSTARTADDR; 
    uProcState[asid].reg_sp = (memaddr) USERSTACKTOP;
    uProcState[asid].status = ALLOFF | USERPON | IEPON | IMON | TEBITON;
    uProcState[asid].entry_hi = (asid + 1) << ASIDSHIFT; /* asid is in 6-11 bits in entryHi */
}

/**
 * @brief Initialize the support structure for a user process
 *        setting up its fields.
 *          
 * @param int asid - the address space identifier
 * @return void
 */
void initSupportStruct(int asid){
    supStruct[asid].sup_asid = asid + 1;
    RAMTOP(current_memaddr);
    /* starting 3 frames from (under) RAMTOP since first frame is taken
    by ssi and second by test pcb */
    current_memaddr -= (3 + asid) * PAGESIZE;
    /* TLB exceptions */
    supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr) current_memaddr;
    supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
    supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr) pager;

    current_memaddr -= PAGESIZE;
    /* general exceptions */
    supStruct[asid].sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr) current_memaddr;
    supStruct[asid].sup_exceptContext[GENERALEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
    supStruct[asid].sup_exceptContext[GENERALEXCEPT].pc = (memaddr) exceptionHandler;

    /* page table initialization */
    for (int i = 0; i < MAXPAGES; i++){ /* from specs -> need to set VPN, ASID, V and D */
        /* these are 64 bits to set for each entry: 32 for entryHi and 32 for entryLo,
        since a page table is an array of TLB entries */
        if (i != 31){ /* setting up virtual page number and asid */
            supStruct[asid].sup_privatePgTbl[i].pte_entryHI = PRESENTFLAG; 
            supStruct[asid].sup_privatePgTbl[i].pte_entryHI |= (i << VPNSHIFT);
        }
        else /* entry 31 is the stack page, that should be set to 0xBFFFF */
            supStruct[asid].sup_privatePgTbl[i].pte_entryHI = (GETSHAREFLAG - PAGESIZE);
        supStruct[asid].sup_privatePgTbl[i].pte_entryHI |= (asid + 1) << ASIDSHIFT;

        /* valid bit off and dirty bit on */
        supStruct[asid].sup_privatePgTbl[i].pte_entryLO = ~VALIDON | DIRTYON;
    }
}

/**
 * @brief Allow a process to ask with message passing the mutex
 *        for accessing the swap pool table. If it can't acquire that mutex, 
 *        it will be blocked, until the mutex is released by another pcb
 *        that has acquired it before and therefore is exiting swap pool.
 *          
 * @param void
 * @return void
 */
void acq2relMutex(){
    
}

void test()
{
    /* Swap table initialization */
    initSwapTable();
    klog_print("Swap table initialized");

    /* following the structure in p2test -> test func 
    no need to alloc this, this is already done in ph2*/
    testPcb = current_process;

    /* user process (UPROC) initialization - 10.1 specs */
    for (int i = 0; i < UPROCMAX; i++){
        initUProc(i);
        initSupportStruct(i);
    }

    /* mutex process */
    // swap_mutex = allocPcb(); no need to allocate
    current_memaddr -= PAGESIZE;
    state_t mutex_s;
    mutex_s.status = ALLOFF | IECON | IMON | TEBITON;
    mutex_s.pc_epc = mutex_s.reg_t9 = (memaddr) acq2relMutex;
    mutex_s.reg_sp = (memaddr) current_memaddr;
    swap_mutex = create_process(&mutex_s, 0);

    



}