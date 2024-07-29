#include "./headers/lib.h"

/* information that should be added to the Support 
structure that is pointed by a U-Proc. To be created, a 
PCB needs its state and support struct */
state_t uProcState[UPROCMAX];
support_t supStruct[UPROCMAX]; /* support struct that will contain page table */
/* in parallel, same thing for SST processes */
state_t sstProcState[UPROCMAX];
support_t sstSupStruct[UPROCMAX];

/* each swap pool is a set of RAM frames, reserved for vm */
swap_t swapPoolTable[POOLSIZE];
pcb_PTR swap_mutex; /* pcb that listens requests and GIVES the mutex */
pcb_PTR mutex_recvr; /* pcb that RECEIVE and RELEASE the mutex */

/* sharable peripheral I/O (printer with spooling 
and terminal) */
pcb_PTR printerPcbs[UPROCMAX];
pcb_PTR terminalPcbs[UPROCMAX];

pcb_PTR testPcb;
unsigned int current_memaddr;

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
void mutex(){
    while(1){
        unsigned int *senderAddr, result;

        /* listens for a mutex request */
        SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
        senderAddr = (unsigned int *)EXCEPTION_STATE->reg_v0;
        /* give to the pcb that requested, the mutex */
        mutex_recvr = (pcb_PTR) senderAddr;
        if (mutex_recvr != NULL) /* send a msg to unblock the process */
          SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, 0, 0);

        /* listens for a mutex release */
        SYSCALL(RECEIVEMESSAGE, (unsigned int)senderAddr, 0, 0);
    }
}

void test()
{
    /* Swap table initialization */
    initSwapStructs();

    /* following the structure in p2test -> test func 
    no need to alloc this, this is already done in ph2*/
    testPcb = current_process;

    /* user process (UPROC) initialization - 10.1 specs */
    for (int i = 0; i < UPROCMAX; i++){
        initUProc(i);
        initSupportStruct(i);
    }

    /* mutex process, tried by ssi but another proc is needed */
    // swap_mutex = allocPcb(); no need to allocate
    current_memaddr -= PAGESIZE;
    state_t mutex_s;
    mutex_s.status = ALLOFF | IECON | IMON | TEBITON;
    mutex_s.pc_epc = mutex_s.reg_t9 = (memaddr) mutex;
    mutex_s.reg_sp = (memaddr) current_memaddr;
    swap_mutex = create_process(&mutex_s, 0);

    /* here the mutex is obtained */

}