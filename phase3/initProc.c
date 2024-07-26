#include "./headers/lib.h"

/* information that should be added to the Support 
structure that is pointed by a U-Proc*/
state_t uProcState[UPROCMAX];
support_t pageTable[UPROCMAX]; /* 32bit half EntryHi, half EntryLo */
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
void initSwapTable()
{
    for (int i = 0; i < POOLSIZE; i++){
        swapPoolTable[i].sw_asid = NOPROC; /* 6bit identifier
          to distinguish the process' kuseg from the others*/
        swapPoolTable[i].sw_pageNo = NOPROC; /* sw_pageNo is the
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


}


void test()
{
    /* Swap table initialization */
    initSwapTable();
    klog_print("Swap table initialized");

    /* following the structure in p2test -> test func 
    no need to alloc this, this is already done in ph2*/
    test_pcb = current_process;
    RAMTOP(current_memaddr);
    current_memaddr -= 3 * PAGESIZE; /* ssi -> PAGESIZE, test -> 2 * PAGESIZE */

    /* user process (UPROC) initialization */
    for (int i = 0; i < UPROCMAX; i++)
        initUProc(i);



}