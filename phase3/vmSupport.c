#include "./headers/lib.h"

/*
    This module contains functions that support the virtual memory management
    and address translation in general. This is mainly done by the Pager component.
*/

/**
 * @brief Initialize the swap pool table, by putting a default value
 *        in swap_t structure fields. Defined here, but used in initProc.c
 *        according to specs.
 *
 * @param int entryid - the entry id in the swap pool table
 * @return void
 */
void initSwapStructs(int entryid)
{ /* 6bit identifier to distinguish the pcb's kuseg from the others*/
  swapPoolTable[entryid].sw_asid = NOASID;
  swapPoolTable[entryid].sw_pageNo = NOPAGE;
  swapPoolTable[entryid].sw_pte = NULL;
}

/**
 * @brief Ask for the swap mutex, in order to gain mutual exclusion over the swap pool table.
 *        If another process is using the swap pool table, the current process is blocked (recv)
 *        until the other process releases the mutex.
 *
 * @param void
 * @return void
 */
void askMutex()
{
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex, 0, 0);
}

/**
 * @brief Pick a frame from the SPT according to a replacement algorithm.
 *        The algorithm is a simple FIFO, that returns the first free frame found.
 *        If no free frame is found, the algorithm returns the first frame in the pool.
 *
 * @param void
 * @return unsigned int - the frame number
 */
int pick_frame()
{
  static unsigned int counter = -1;
  for (int i = 0; i < POOLSIZE; i++)
  {
    if (isFrameFree(i))
      return i;
  } /* increment mod POOLSIZE */
  return (counter++ % POOLSIZE);
}

/**
 * @brief Check if a frame is free in the swap pool table.
 *
 * @param int i - the frame number
 * @return int - 1 if the frame is free, 0 otherwise
 */
int isFrameFree(int i)
{
  return swapPoolTable[i].sw_asid == NOASID;
}

/**
 * @brief Disable interrupts putting the curresponding bit 
 *        in the status register to 0.
 *
 * @param void
 * @return void
 */
void interrupts_off()
{
  setSTATUS(getSTATUS() & (~IECON));
}

/**
 * @brief Enable interrupts putting the curresponding bit 
 *        in the status register to 1.
 *
 * @param void
 * @return void
 */
void interrupts_on()
{
  setSTATUS(getSTATUS() | IECON);
}

/**
 * @brief Update the TLB with the new page table entry.
 *        If the page is not cached, the entry is written in the TLB.
 *
 * @param pteEntry_t - the page table entry
 * @return void
 */
void updateTLB(pteEntry_t pte)
{
  setENTRYHI(pte.pte_entryHI);
  TLBP(); /* probe the TLB to find a match */
  if ((getINDEX() & PRESENTFLAG) == 0)
  { /* not cached */
    setENTRYHI(pte.pte_entryHI);
    setENTRYLO(pte.pte_entryLO);
    TLBWI(); /* write current entry in entryHI index */
  }
}

/**
 * @brief Perform a read/write operation on uproc's backing store. This is 
 *        done requesting a DOIO service to the SSI, since backing store
 *        are treated as flash devices.
 *
 * @param pteEntry_t - the page table entry
 * @return void
 */
int flashOp(int asid, int block, memaddr pageAddr, int operation)
{ /* take the register as a non-terminal device register
  interruptLine 4 is associated with flash devices, and the devNo
  depends on which backing store we're considering (so it depends on UProc asid) */
  unsigned int s;
  dtpreg_t *flashReg = (dtpreg_t *)DEV_REG_ADDR(FLASHINT, asid - 1);
  flashReg->data0 = pageAddr; /* load the page address, 4k block to read/write */

  /* pops p.35 - an operation on a flash device is started by loading the
  appropriate value into the COMMAND field. */
  ssi_do_io_t do_io = { /* doio service */
      .commandAddr = (unsigned int *)&(flashReg->command),
      .commandValue = (block << BYTELENGTH) | operation,
  }; /* write on BLOCKNUMBER (24bit) shifting 1byte sx */
  ssi_payload_t payload = {
      .service_code = DOIO,
      .arg = &do_io,
  }; /* send the service request to the SSi */
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&s, 0);
  return s;
}

/**
 * @brief Pager component. This is the handler for TLB exceptions.
 *        Permits the system to manage the virtual memory and address translations.
 *
 * @param void
 * @return void
 */
void pager()
{ /* obtain the sup struct from currproc */
  support_t *sup = getSupStruct();
  /* detrmine the cause of the TLB exception occurred */
  state_t *supState = &sup->sup_exceptState[PGFAULTEXCEPT];
  if (CAUSE_GET_EXCCODE(supState->cause) == TLBINVLDM) /* TLB-modification */
    programTrapHandler();                              /* treat it as progtrap */

  /* gain mutual exclusion over the spt */
  askMutex();
  /* determine the missing page number (same as TLBRefill) */
  int p = ENTRYHI_GET_VPN(supState->entry_hi);
  if (p > MAXPAGES - 1)  /* bound check */
    p = MAXPAGES - 1;

  /* get a frame from the swap pool with a replacement algo,
    determining if it's free and can contain a page */
  int victim_page = pick_frame();
  swap_t *spte = &swapPoolTable[victim_page];
  memaddr victim_page_addr = (memaddr)SWAPPOOL + (victim_page * PAGESIZE);

  /* check if the frame is currently occupied */
  if (!isFrameFree(victim_page))
  { /* mark page as not valid, atomically disabiliting interrupts - 5.3 specs */
    /* update also the TLB, if needed */
    interrupts_off();
    pteEntry_t *victim_pte = spte->sw_pte;
    victim_pte->pte_entryLO &= (~VALIDON); /* mark the page as not valid */
    updateTLB(*victim_pte);
    interrupts_on();

    /* write on backing store */
    if (flashOp(spte->sw_asid, spte->sw_pageNo, victim_page_addr, FLASHWRITE) != READY)
      programTrapHandler(); /* treat any write/read error on devices as a progtrap */
  }

  /* read from backing store */
  if (flashOp(sup->sup_asid, p, victim_page_addr, FLASHREAD) != READY)
    programTrapHandler();

  /* update the swap pool table */
  spte->sw_asid = sup->sup_asid;
  spte->sw_pageNo = p;
  spte->sw_pte = &sup->sup_privatePgTbl[p];

  /* update the current process page table entry */
  interrupts_off();
  sup->sup_privatePgTbl[p].pte_entryLO |= VALIDON | DIRTYON; /* mark the page as valid and dirty */
  sup->sup_privatePgTbl[p].pte_entryLO |= victim_page_addr;
  updateTLB(sup->sup_privatePgTbl[p]);
  interrupts_on();

  /* release the mutex */
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
  LDST(supState);
}
