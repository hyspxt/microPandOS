#include "./headers/lib.h"


/*
    This module contains the test and initialization functions that will be 
    called by the Nucleus. The test function will initialize the SST and UPROCS.
*/

/* information that should be added to the Support
structure that is pointed by a U-Proc. To be created, a
PCB needs its state and support struct */
state_t uProcState[UPROCMAX];
support_t supStruct[UPROCMAX]; /* support struct that will contain page table */
state_t sstProcState[UPROCMAX]; /* in parallel, same thing for SST processes */
state_t printerState[UPROCMAX], terminalState[UPROCMAX];

/* each swap pool is a set of RAM frames, reserved for vm */
swap_t swapPoolTable[POOLSIZE];
/* these process will be test_pcb children */
pcb_PTR swap_mutex;  /* pcb that listens requests and GIVES the mutex */
pcb_PTR mutex_recvr; /* pcb that RECEIVE and RELEASE the mutex */

/* specs -> have a process for each device that waits for
messages and requests the single DoIO to the SSI */
pcb_PTR printerPcbs[UPROCMAX], terminalPcbs[UPROCMAX];
/* referring to specs diagram are children of */
pcb_PTR sstPcbs[UPROCMAX], uproc[UPROCMAX];/* DEBUGGING */
pcb_PTR testPcb;

memaddr ramtop; /* mind that grow downwards */

// struct list_head freeSupStructs;
// support_t *allocateSupStructs{
//     support_t *sup = container_of(freeSupStructs.next, support_t, s_list);
//     list_del(freeSupStructs.next);
//     return sup;
// }

// void freeSupStructs(support_t *sup){
//     list_add_tail(&sup->s_list, &freeSupStructs);
// }


/* pointer functions which address will be assigned to pc_epc field in pcb
   associated with peripheral devices (IL_TERMINAL and IL_PRINTER).
   IL_FLASH will be treated as backing store for uprocs */
void (*printer0()) { 
    printDevice(0, IL_PRINTER); return (void *)0;}
void (*printer1()) {
    printDevice(1, IL_PRINTER); return (void *)0;}
void (*printer2()) {
    printDevice(2, IL_PRINTER); return (void *)0;}
void (*printer3()) {
    printDevice(3, IL_PRINTER); return (void *)0;}
void (*printer4()) {
    printDevice(4, IL_PRINTER); return (void *)0;}
void (*printer5()) {
    printDevice(5, IL_PRINTER); return (void *)0;}
void (*printer6()) {
    printDevice(6, IL_PRINTER); return (void *)0;}
void (*printer7()) {
    printDevice(7, IL_PRINTER); return (void *)0;}

void (*terminal0()) {
    printDevice(0, IL_TERMINAL); return (void *)0;}
void (*terminal1()) {
    printDevice(1, IL_TERMINAL); return (void *)0;}
void (*terminal2()) {
    printDevice(2, IL_TERMINAL); return (void *)0;}
void (*terminal3()) {
    printDevice(3, IL_TERMINAL); return (void *)0;}
void (*terminal4()) {
    printDevice(4, IL_TERMINAL); return (void *)0;}
void (*terminal5()) {
    printDevice(5, IL_TERMINAL); return (void *)0;}
void (*terminal6()) {
    printDevice(6, IL_TERMINAL); return (void *)0;}
void (*terminal7()) {
    printDevice(7, IL_TERMINAL); return (void *)0;}

/**
 * @brief Initialize the user processes (UPROC) which will be used as SST child to write/read
 *        on their backing store that contains their full logical image. 
 *        Backing store, in this case, are the flash devices.
 *        Each UPROC will operate on its backing store.
 *
 * @param void
 * @return void
 */
void initUProc()
{
    for (int asid = 0; asid < UPROCMAX; asid++)
    { /* program counter set to start .text section */
        uProcState[asid].pc_epc = uProcState[asid].reg_t9 = (memaddr)UPROCSTARTADDR;
        uProcState[asid].reg_sp = (memaddr)USERSTACKTOP;
        /* all interrupts, user-mode, plt enabled */
        uProcState[asid].status = ALLOFF | USERPON | IEPON | IMON | TEBITON;
        uProcState[asid].entry_hi = (asid + 1) << ASIDSHIFT; /* asid is in 6-11 bits in entryHi */
    }
}

/**
 * @brief Initialize the support structures for each UPROC, which will contain the page table
 *        entries for each UPROC. Page table entries are 64 bits long.
 *        Specs -> only context[2], pgtbl[32] and asid are needed.
 *
 * @param int asid - the address space identifier
 * @return void
 */
void initSupportStruct()
{
    for (int asid = 0; asid < UPROCMAX; asid++)
    {
        supStruct[asid].sup_asid = asid + 1;
        /* TLB exceptions */
        supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)ramtop;
        supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
        supStruct[asid].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;
        ramtop -= PAGESIZE;

        /* general exceptions */
        supStruct[asid].sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)ramtop;
        supStruct[asid].sup_exceptContext[GENERALEXCEPT].status = ALLOFF | IEPON | IMON | TEBITON;
        supStruct[asid].sup_exceptContext[GENERALEXCEPT].pc = (memaddr)supExceptionHandler; /* !! */
        ramtop -= PAGESIZE;

        /* from specs -> need to set VPN, ASID, V and D */
        /* these are 64 bits to set for each entry: 32 for entryHi and 32 for entryLo,
        since a page table is an array of TLB entries */

        /* page table initialization */
        for (int i = 0; i < MAXPAGES - 1; i++)
        {
            supStruct[asid].sup_privatePgTbl[i].pte_entryHI = KUSEG + (i << VPNSHIFT) + ((asid + 1) << ASIDSHIFT);
            supStruct[asid].sup_privatePgTbl[i].pte_entryLO = DIRTYON;
        } /* valid bit off and dirty bit on  */
        /* entry 31 -> stack page */
        supStruct[asid].sup_privatePgTbl[MAXPAGES - 1].pte_entryHI = (GETSHAREFLAG - PAGESIZE) + ((asid + 1) << ASIDSHIFT);
        supStruct[asid].sup_privatePgTbl[MAXPAGES - 1].pte_entryLO = DIRTYON;
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
void mutex()
{
    unsigned int senderAddr;
    while (1)
    {  /* listens for a mutex request */
        senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
        /* give to the pcb that requested, the mutex */

        /* send a msg to unblock the process */
        mutex_recvr = (pcb_PTR)senderAddr;
        SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, 0, 0);

        /* listens for a mutex release */
        SYSCALL(RECEIVEMESSAGE, (unsigned int)senderAddr, 0, 0);
    }
}

/**
 * @brief Initialize UPROCMAX SST processes, which will create then the child
 *        user processes. Each SST process will then be delegated to resolve
 *        the requests that the child process submits to it.
 *
 * @param void
 * @return void
 */
void initSST()
{
    for (int i = 0; i < UPROCMAX; i++)
    {
        sstProcState[i].pc_epc = (memaddr)SST;
        sstProcState[i].reg_sp = (memaddr)ramtop;
        sstProcState[i].status = ALLOFF | IEPON | IMON | TEBITON;
        sstProcState[i].entry_hi = (i + 1) << ASIDSHIFT;
        /* create the SST process, supStruct will be used for retrieving asid
        to let the SST create the father -> child association. Technically,
        asid is non-retrievable from the state, hence in this way a certain
        child can inherit father (SST) support struct */
        sstPcbs[i] = create_process(&sstProcState[i], &supStruct[i]);
        ramtop -= PAGESIZE;
    }
}

/**
 * @brief Initialize the device processes, which will be used to handle
 *        the print function of the terminal and printer devices.
 *        Mind that the OS can handle max UPROC devices.
 *
 * @param int asid - address space identifier
 * @param int devNo - device number (IL_PRINTER or IL_TERMINAL)
 * @return void
 */
void initDeviceProc(int asid, int devNo)
{
    memaddr assignmentToPc = 0;
    switch (devNo)
    { /* assignment to PC may be only done with pointers-returning function */
    case IL_PRINTER:
        switch (asid){
            case 0: assignmentToPc = (memaddr) printer0; break;
            case 1: assignmentToPc = (memaddr) printer1; break;
            case 2: assignmentToPc = (memaddr) printer2; break;
            case 3: assignmentToPc = (memaddr) printer3; break;
            case 4: assignmentToPc = (memaddr) printer4; break;
            case 5: assignmentToPc = (memaddr) printer5; break;
            case 6: assignmentToPc = (memaddr) printer6; break;
            case 7: assignmentToPc = (memaddr) printer7; break;
        }
        printerState[asid].pc_epc = assignmentToPc;
        printerState[asid].reg_sp = (memaddr)ramtop;
        printerState[asid].status = ALLOFF | IEPON | IMON | TEBITON;
        printerState[asid].entry_hi = (asid + 1) << ASIDSHIFT;
        printerPcbs[asid] = create_process(&printerState[asid], &supStruct[asid]);
        break;
    case IL_TERMINAL:
        switch (asid){
            case 0: assignmentToPc = (memaddr) terminal0; break;
            case 1: assignmentToPc = (memaddr) terminal1; break;
            case 2: assignmentToPc = (memaddr) terminal2; break;
            case 3: assignmentToPc = (memaddr) terminal3; break;
            case 4: assignmentToPc = (memaddr) terminal4; break;
            case 5: assignmentToPc = (memaddr) terminal5; break;
            case 6: assignmentToPc = (memaddr) terminal6; break;
            case 7: assignmentToPc = (memaddr) terminal7; break;
        }
        terminalState[asid].pc_epc = assignmentToPc;
        terminalState[asid].reg_sp = (memaddr)ramtop;
        terminalState[asid].status = ALLOFF | IEPON | IMON | TEBITON;
        terminalState[asid].entry_hi = (asid + 1) << ASIDSHIFT;
        terminalPcbs[asid] = create_process(&terminalState[asid], &supStruct[asid]);
        break;
    } /* get another frame */
    ramtop -= PAGESIZE;
}

/**
 * @brief Test function that initializes the SST and UPROCS.
 *
 * @param void
 * @return void
 */
void test()
{ /* following the structure in p2test -> test func
    no need to alloc this, this is already done in ph2*/
    testPcb = current_process;

    /* setting memory to RAMTOP and then go downward */
    RAMTOP(ramtop);
    /* starting 3 frames from (under) RAMTOP since first frame is taken
    by ssi and second by test pcb */
    ramtop -= (3 * PAGESIZE);

    /* Swap table initialization */
    for (int i = 0; i < POOLSIZE; i++)
        initSwapStructs(i);

    /* user process (UPROC)/flash initialization - 10.1 specs */
    /* also SST processes are initialized here (their fathers) */
    initUProc();
    initSupportStruct();

    /* mutex process, tried ssi but another proc is needed */
    // swap_mutex = allocPcb(); no need to allocate
    state_t mutex_s;
    mutex_s.pc_epc = (memaddr)mutex;
    mutex_s.reg_sp = (memaddr)ramtop;
    mutex_s.status = ALLOFF | IECON | IMON | TEBITON;
    ramtop -= PAGESIZE;
    swap_mutex = create_process(&mutex_s, NULL);

    initSST();

    /* mutex request are now active */
    for (int i = 0; i < UPROCMAX; i++)
    {
        initDeviceProc(i, IL_PRINTER);
        initDeviceProc(i, IL_TERMINAL);
    }

    for (int i = 0; i < UPROCMAX; i++)
    {
        SYSCALL(RECEIVEMESSAGE, (unsigned int)sstPcbs[i], 0, 0);
        outProcQ(&readyQueue, sstPcbs[i]);
        freePcb(sstPcbs[i]);
    } /* kill the test and its progeny */
    sendKillReq(NULL);
}