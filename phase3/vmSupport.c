#include "./headers/lib.h"

/**
 * @brief Initialize the swap pool table, by putting a default value
 *        in swap_t structure fields. Defined here, but used in initProc.c
 *        according to specs.
 *
 * @param void
 * @return void
 */
void initSwapStructs(int entryid)
{
  swapPoolTable[entryid].sw_asid = NOASID;   /* 6bit identifier
      to distinguish the process' kuseg from the others*/
  swapPoolTable[entryid].sw_pageNo = NOPAGE; /* sw_pageNo is the
  virtual page number in the swap pool.*/
}

/**
 * @brief Replacement algorithm for the pager. It searches for a free frame
 *        in the swap pool table, if not found, it uses a FIFO replacement
 *        algorithm.
 *
 * @param void
 * @return int - the free frame index found or the victim page index
 */
int replacement()
{
  for (int i = 0; i < POOLSIZE; i++)
  {
    if (swapPoolTable[i].sw_asid == NOASID)
      return i; /* free frame index found */
  } /* replacement algo. that search for a victim page */
  static int fifoIndex = -1; /* index for the replacement algo. */
  fifoIndex = (fifoIndex + 1) % POOLSIZE;
  return fifoIndex;
}

/**
 * @brief Ask for the mutex to the swap_mutex process. mutex_recvr will be
 *        the eventual mutex holder after this.
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
 * @brief Atomically, update the specified swap pool table, marking it as not valid and
 *        eventually update the TLB.
 *
 * @param void
 * @return void
 */
void handleOccupiedFrame(int frameIndex)
{
  /* disabling interrupts - 5.3 specs */
  setSTATUS(getSTATUS() & ~IECON);
  /* invaliding page */
  swapPoolTable[frameIndex].sw_pte->pte_entryLO &= ~VALIDON;
  /* updatin TLB */
  setENTRYHI(swapPoolTable[frameIndex].sw_pte->pte_entryHI);
  TLBP(); /* TLBProbe */
  if ((getINDEX() & PRESENTFLAG) == 0)
  { /* not cached */
    /* updating only entryLO cause it's the part of the TLB entry that
    contains the Physical Frame Number -> no need to modify VPN/EntryHI */
    setENTRYLO(swapPoolTable[frameIndex].sw_pte->pte_entryLO);
    TLBWI(); /* TLBWrite */
  } /* re-enabling interrupts, immediatly */
  setSTATUS(getSTATUS() | IECON);
}

/**
 * @brief Atomically, update a process page table entry, marking it as valid and
 *        eventually update the TLB.
 *
 * @param memaddr pageAddr - the address of the page
 * @param support_t *sup - the support struct get from current_process
 * @param int pageNo - the page number in the swap pool table
 * @return void
 */
void handleFreeFrame(memaddr pageAddr, support_t *sup, int pageNo)
{
  setSTATUS(getSTATUS() & ~IECON);                      /* disabling interrupts */
  sup->sup_privatePgTbl[pageNo].pte_entryLO |= VALIDON; /* marking as valid */
  sup->sup_privatePgTbl[pageNo].pte_entryLO |= DIRTYON; /* marking as not dirty */
  sup->sup_privatePgTbl[pageNo].pte_entryLO &= 0xFFF;    /* preserve first 12 bit of entryLO -> reset PFN */
  sup->sup_privatePgTbl[pageNo].pte_entryLO |= pageAddr; /* setting the frame */

  /* updatin TLB */
  setENTRYHI(sup->sup_privatePgTbl[pageNo].pte_entryHI);
  TLBP(); /* TLBProbe */
  if ((getINDEX() & PRESENTFLAG) == 0)
  { /* not cached */
    /* updating only entryLO cause it's the part of the TLB entry that
    contains the Physical Frame Number -> no need to modify VPN/EntryHI */
    setENTRYLO(sup->sup_privatePgTbl[pageNo].pte_entryLO);
    TLBWI(); /* TLBWrite */
  } /* re-enabling interrupts, immediatly */
  setSTATUS(getSTATUS() | IECON);
}

/**
 * @brief Perform a backing store operation, reading or writing a page from/in the backing store.
 *        Mind that a backing store is treated as a flash device, so operations
 *        are done in blocks using data1 and command field, as specified in
 *        uMPS3 - pops p.34.
 *
 * @param memaddr pageAddr - the address of the page to read/write and load in data1
 * @param int asid - the address space identifier
 * @param int pageNo - the page number in the swap pool table
 * @param int operation - the operation to perform (READ/WRITE)
 * @return int - the status of the backing store write operation
 */
int backingStoreOp(memaddr pageAddr, int asid, int pageNo, int operation)
{
  /* take the register as a non-terminal device register
  interruptLine is obviosly associated with flash devices, and the devNo
  depends on which backing store we're considering (so depends on UProc asid) */
  dtpreg_t *flashReg = (dtpreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
  flashReg->data0 = pageAddr; /* load the page address, 4k block to read/write */
  unsigned int s;

  /* pops p.35 - an operation on a flash device is started by loading the
  appropriate value into the COMMAND field. */
  ssi_do_io_t do_io = {
      /* doio service */
      .commandAddr = (unsigned int *)&(flashReg->command),
      .commandValue = (pageNo << BYTELENGTH) | operation,
  }; /* write on BLOCKNUMBER (24bit) shifting 1byte sx */
  ssi_payload_t payload = {
      .service_code = DOIO,
      .arg = &do_io,
  };
  /* send the service to the SSi */
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&s, 0);
  return s;
}

/**
 * @brief Updates swap pool table to reflect frame new content.
 *
 * @param int frameIndex - the index of the frame in the swap pool table
 * @param support_t *sup - the support struct get from current_process
 * @param int pageNo - the page number in the swap pool table
 * @return void
 */
void updateSwapTable(int frameIndex, support_t *sup, int pageNo)
{
  swapPoolTable[frameIndex].sw_asid = sup->sup_asid;
  swapPoolTable[frameIndex].sw_pageNo = pageNo;
  swapPoolTable[frameIndex].sw_pte = &(sup->sup_privatePgTbl[pageNo]);
}

void freeAndKill(int releaseMutex)
{
    if (releaseMutex)
        SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);

    for (int i = 0; i < POOLSIZE; i++)
    { /* clear the swap pool table entry */
        if (swapPoolTable[i].sw_asid == current_process->p_supportStruct->sup_asid)
            swapPoolTable[i].sw_asid = NOASID;
    }
    sendKillReq();
}

/**
 * @brief Pager component.
 *
 * @param void
 * @return void
 */
void pager()
{
  /* obtain the support struct */
  support_t *sup = getSupStruct();

  /* determine the case of the TLB exception occurred */
  state_t *excState = &(sup->sup_exceptState[PGFAULTEXCEPT]);
  int excCode = (excState->cause & GETEXECCODE) >> CAUSESHIFT;
  if (excCode == TLBINVLDM)            /* if the exc is a TLB-modification */
    freeAndKill(OFF);
  else
  { /* otherwise handle the remaining TLB invalid action (load/store) */
    /* gain mutual exclusion, sending and awaiting message to mutex giver */
    askMutex();

    /* determine the missing page number (same as TLBRefill) */
    int p = ENTRYHI_GET_VPN(excState->entry_hi);
    if (p >= MAXPAGES - 1) /* avoiding out of range (0-31) */
      p = MAXPAGES - 1;

    /* get a frame from the swap pool with a replacement algo,
    determining if it's free and can contain a page */
    int frameIndex = replacement();
    /* examine the page (assuming OS code is located as specs said)*/
    memaddr victimPageAddr = (memaddr)SWAPPOOL + (frameIndex * PAGESIZE);

    int status; /* status to read after operations of backing stores */
    if (swapPoolTable[frameIndex].sw_asid != NOASID)
    { /* frame currently occupied */
      /* mark page as not valid, atomically disabiliting interrupts */
      /* update also the TLB, if needed */
      handleOccupiedFrame(frameIndex);
      /* update backing store and check status */
      status = backingStoreOp(victimPageAddr, swapPoolTable[frameIndex].sw_asid, swapPoolTable[frameIndex].sw_pageNo, FLASHWRITE);
      if (status != ON){ /* doio should return a "Device Ready" = 1 status */
        SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
        freeAndKill(ON);
      } 
    }

    /* read the contents from backing store in logical page p */
    status = backingStoreOp(victimPageAddr, sup->sup_asid, p, FLASHREAD);
    if (status != ON){ /* doio should return a "Device Ready" = 1 status */
      SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
      freeAndKill(ON);
    }
    /* update the swap pool table */
    updateSwapTable(frameIndex, sup, p);
    /* update page table entry and TLB */
    handleFreeFrame(frameIndex, sup, p);

    /* release the mutex */
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
    /* return control to retry after the page fault */
    LDST(excState);
  }
}
