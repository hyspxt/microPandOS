#include "./headers/lib.h"

/* information that should be added to the Support
structure that is pointed by a U-Proc. To be created, a
PCB needs its state and support struct */
state_t uProcState[UPROCMAX];
support_t supStruct[UPROCMAX]; /* support struct that will contain page table */
/* in parallel, same thing for SST processes */
state_t sstProcState[UPROCMAX];
// support_t sstSupStruct[UPROCMAX]; /* i think this is not needed */
state_t printerState[UPROCMAX];
state_t terminalState[UPROCMAX];

/* each swap pool is a set of RAM frames, reserved for vm */
swap_t swapPoolTable[POOLSIZE];
pcb_PTR swap_mutex;  /* pcb that listens requests and GIVES the mutex */
pcb_PTR mutex_recvr; /* pcb that RECEIVE and RELEASE the mutex */

/* specs -> have a process for each device that waits for
messages and requests the single DoIO to the SSI (SST?) */
pcb_PTR printerPcbs[UPROCMAX], terminalPcbs[UPROCMAX];
/* referring to specs diagram are children of */
pcb_PTR sstPcbs[UPROCMAX], uproc[UPROCMAX]; /* DEBUGGING */

pcb_PTR testPcb;
memaddr ramtop; /* mind that it's a reference! not a const! */

/**
 * @brief Initialize a single user process setting up its fields.
 *
 * @param int asid - the address space identifier
 * @return void
 */
void initUProc()
{
    for (int asid = 0; asid < UPROCMAX; asid++)
    {
        /* program counter set to start .text section */
        uProcState[asid].pc_epc = uProcState[asid].reg_t9 = (memaddr)UPROCSTARTADDR;
        uProcState[asid].reg_sp = (memaddr)USERSTACKTOP;
        /* all interrupts, user-mode, plt enabled */
        uProcState[asid].status = ALLOFF | USERPON | IEPON | IMON | TEBITON;
        uProcState[asid].entry_hi = (asid + 1) << ASIDSHIFT; /* asid is in 6-11 bits in entryHi */
    }
}

/**
 * @brief Initialize the support structure for a single user process
 *        setting up its fields.
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
        }
        /* valid bit off and dirty bit on */
        supStruct[asid].sup_privatePgTbl[MAXPAGES - 1].pte_entryHI = 0xBFFFF000 + ((asid + 1) << ASIDSHIFT);
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
    unsigned int senderAddr, retr;
    while (1)
    {

        /* listens for a mutex request */
        senderAddr = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
        klog_print("Received mutex req from \n");
        klog_print_hex(senderAddr);
        /* give to the pcb that requested, the mutex */

        /* send a msg to unblock the process */
        SYSCALL(SENDMESSAGE, (unsigned int)senderAddr, 0, 0);
        mutex_recvr = (pcb_PTR)senderAddr;
        klog_print("\n Mutex sent to: \n ");
        klog_print_hex((unsigned int)mutex_recvr);

        /* listens for a mutex release */
        retr = SYSCALL(RECEIVEMESSAGE, (unsigned int)senderAddr, 0, 0);
        klog_print("\n Mutex returned from: \n");
        klog_print_hex(retr);
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
        /* create the SST process, supStruct will be used for retrieve asid
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
    switch (devNo)
    {
    case IL_PRINTER:
        printerState[asid].pc_epc = (memaddr)terminals[asid];
        printerState[asid].reg_sp = (memaddr)ramtop;
        printerState[asid].status = ALLOFF | 0x4 | 0xFD00 | TEBITON;
        printerState[asid].entry_hi = (asid + 1) << ASIDSHIFT;
        printerPcbs[asid] = create_process(&printerState[asid], &supStruct[asid]);
        break;
    case IL_TERMINAL:
        terminalState[asid].pc_epc = (memaddr)printers[asid];
        terminalState[asid].reg_sp = (memaddr)ramtop;
        terminalState[asid].status = ALLOFF | 0x4 | 0xFD00 | TEBITON;
        terminalState[asid].entry_hi = (asid + 1) << ASIDSHIFT;
        terminalPcbs[asid] = create_process(&terminalState[asid], &supStruct[asid]);
        break;
    }
    ramtop -= PAGESIZE;
}

/*
 * Wrapper delle funzioni di stampa per poterle assegnare ai program counter dei
 * vari device.
 */
void print_term0() { printDevice(0, IL_TERMINAL); }
void print_term1() { printDevice(1, IL_TERMINAL); }
void print_term2() { printDevice(2, IL_TERMINAL); }
void print_term3() { printDevice(3, IL_TERMINAL); }
void print_term4() { printDevice(4, IL_TERMINAL); }
void print_term5() { printDevice(5, IL_TERMINAL); }
void print_term6() { printDevice(6, IL_TERMINAL); }
void print_term7() { printDevice(7, IL_TERMINAL); }

void printer0() { printDevice(0, IL_PRINTER); }
void printer1() { printDevice(1, IL_PRINTER); }
void printer2() { printDevice(2, IL_PRINTER); }
void printer3() { printDevice(3, IL_PRINTER); }
void printer4() { printDevice(4, IL_PRINTER); }
void printer5() { printDevice(5, IL_PRINTER); }
void printer6() { printDevice(6, IL_PRINTER); }
void printer7() { printDevice(7, IL_PRINTER); }

/*
 * Array di puntatori ai wrapper delle funzioni di stampa per una maggiore
 * comodità durante l'assegnamento al program counter.
 */
void (*terminals[8])() = {print_term0, print_term1, print_term2, print_term3,
                          print_term4, print_term5, print_term6, print_term7};
void (*printers[8])() = {printer0, printer1, printer2, printer3,
                         printer4, printer5, printer6, printer7};

/**
 * @brief The TLB-Refill event Handler. This type of event is triggered
 *        whenever a TLB exception occurs, that is, when we try to access
 *        a page that is not in the TLB. So, it's a cache-miss event.
 *        Hence, this handler is responsible for loading the
 *        missing page table entry and restart the instruction.
 *        note. Previous definition (for phase2) is found in lib.h, commented.
 *
 * @param void
 * @return int
 */
void uTLB_RefillHandler()
{ /* redefinition of phase2 handler */
    /* locate the pt entry */
    state_t *excState = EXCEPTION_STATE;
    int p = ENTRYHI_GET_VPN(excState->entry_hi);
    /* this is done due cause it happens that the macro
    return a out of range (0-31) value */

    p = MIN(p, MAXPAGES - 1);

    /* set the entryhi and entrylo, with supStruct of curr_proc */
    setENTRYHI(current_process->p_supportStruct->sup_privatePgTbl[p].pte_entryHI);
    setENTRYLO(current_process->p_supportStruct->sup_privatePgTbl[p].pte_entryLO);
    /* write the TLB */
    TLBWR();
    /* restart the instruction */
    LDST(excState);
}

void test()
{
    /* following the structure in p2test -> test func
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
    klog_print("\n Mutex process pid is \n");
    klog_print_dec(swap_mutex->p_pid);

    /* mutex request are now active */
    for (int i = 0; i < UPROCMAX; i++)
    {
        initDeviceProc(i, IL_PRINTER);
    }

    for (int i = 0; i < UPROCMAX; i++)
    {
        initDeviceProc(i, IL_TERMINAL);
    }

    initSST();

    for (int i = 0; i < UPROCMAX; i++)
    {
        SYSCALL(RECEIVEMESSAGE, (unsigned int)sstPcbs[i], 0, 0);
        klog_print("\n SST process ");
        klog_print_dec(i);
        klog_print(" terminated \n");
    }

    ssi_payload_t pyld = {
        .service_code = TERMPROCESS,
        .arg = NULL,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&pyld, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
    klog_print("Test process terminated");
}