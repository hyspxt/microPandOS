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
 * @brief Pick a frame from the SPT according to a replacement algorithm.
 *        The algorithm is a simple FIFO, that returns the first free frame found.
 *        If no free frame is found, the algorithm returns the first frame in the pool.
 *
 * @param void
 * @return int - the frame number of the free/victimized page
 */
int pick_frame()
{
  static int counter = -1; /* correction, -> this was an unsigned int */
  for (int i = 0; i < POOLSIZE; i++)
  {
    if (isFrameFree(i))
      return i;
  } /* increment mod POOLSIZE */
  return (counter++ % POOLSIZE);
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
  TLBP(); /* TLB probing */
  if ((getINDEX() & PRESENTFLAG) == 0)
  { /* not cached */
    /* correction -> setENTRYHI not necessary */
    setENTRYLO(pte.pte_entryLO);
    TLBWI(); /* write entry */
  }
}

/**
 * @brief Perform a read/write operation on uproc's flash device. This is 
 *        done requesting a DOIO service to the SSI, since backing store
 *        are treated as flash devices.
 *
 * @param int asid - the address space identifier
 * @param int block - the block number on which the operation is performed (that is page number)
 * @param memaddr pageAddr - the page address
 * @param int operation - the operation to perform (FLASHREAD/FLASHWRITE)
 * @return int - the status of the operation (1 if successful, 0 otherwise)
 */
int flashOp(int asid, int block, memaddr pageAddr, int operation)
{ /* take the register as a non-terminal device register
  interruptLine 4 is associated with flash devices, and the devNo
  depends on which backing store we're considering (so it depends on UProc asid) */
  unsigned int s;
  devreg_t *flashReg = (devreg_t *)DEV_REG_ADDR(FLASHINT, asid - 1);
  flashReg->dtp.data0 = pageAddr; /* load the page address, 4k block to read/write */

  /* pops p.35 - an operation on a flash device is started by loading the
  appropriate value into the COMMAND field. */
  ssi_do_io_t do_io = { /* doio service */
      .commandAddr = &(flashReg->dtp.command),
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
 * @brief Pager component. This is the handler for Page Fault exceptions.
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
  int victimizedPgNo = pick_frame();
  swap_t *spte = &swapPoolTable[victimizedPgNo];
  unsigned int victimizedPgAddr = SWAPPOOL + (victimizedPgNo * PAGESIZE);

  /* check if the frame is currently occupied */
  if (!isFrameFree(victimizedPgNo))
  { /* mark page as not valid, atomically disabiliting interrupts - 5.3 specs */
    /* update also the TLB, if needed */
    interrupts_off();
    pteEntry_t *victimizedPte = spte->sw_pte;
    victimizedPte->pte_entryLO &= ~VALIDON; /* mark the page as not valid */
    updateTLB(*victimizedPte);
    interrupts_on();

    /* write on backing store */
    if (flashOp(spte->sw_asid, spte->sw_pageNo, victimizedPgAddr, FLASHWRITE) != READY)
      programTrapHandler(); /* treat any write/read error on devices as a progtrap */
  } 

  /* read from backing store */
  if (flashOp(sup->sup_asid, p, victimizedPgAddr, FLASHREAD) != READY)
    programTrapHandler();

  /* update the swap pool table */
  spte->sw_asid = sup->sup_asid;
  spte->sw_pageNo = p;
  spte->sw_pte = &sup->sup_privatePgTbl[p];

  /* update the current process page table entry */
  interrupts_off();
  sup->sup_privatePgTbl[p].pte_entryLO |= VALIDON | DIRTYON;
  sup->sup_privatePgTbl[p].pte_entryLO &= 0xFFF;
  /* correction -> clear the PNF but preserve the last 12 bits */
  sup->sup_privatePgTbl[p].pte_entryLO |=  victimizedPgAddr; /* mark the page as valid and dirty */
  updateTLB(sup->sup_privatePgTbl[p]);
  interrupts_on();

  /* release the mutex */
  SYSCALL(SENDMESSAGE, (unsigned int)mutexSender, 0, 0);
  LDST(supState);
}
