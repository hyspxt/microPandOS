#include "./headers/lib.h"

/**
 * @brief Initialize the swap pool table, by putting a default value
 *        in swap_t structure fields.
 *
 * @param void
 * @return void
 */
void initSwapStructs(int entryid)
{
  swapPoolTable[i].sw_asid = NOASID;   /* 6bit identifier
      to distinguish the process' kuseg from the others*/
  swapPoolTable[i].sw_pageNo = NOPAGE; /* sw_pageNo is the
  virtual page number in the swap pool.*/
  swapPoolTable[i].sw_pte = NULL;      /* pointer to the page table entry */
                                       /* TODO check if this causes problems */
}

/**
 * @brief Replacement algorithm for the pager. It searches for a free frame
 *        in the swap pool table, if not found, it uses a FIFO replacement
 *        algorithm.
 *
 * @param void
 * @return void
 */
int replacement()
{
  for (int i = 0; i < POOLSIZE; i++){
    if (swapPoolTable[i].sw.asid == NOASID)
      return i; /* free frame index found */
  } /* replacement algo. that search for a victim page */
  static int fifoIndex = -1; /* index for the replacement algo. */
  fifoIndex = (++fifoIndex) % POOLSIZE;
  return fifoIndex;
}

/**
 * @brief Ask for the mutex to the swap_mutex process. mutex_rcvr will be
 *        the eventual mutex holder after this.
 *
 * @param void
 * @return void
 */
void askMutex(){
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex, 0, 0);
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
  support_t *sstSup = getSupStruct();

  /* determine the case of the TLB exception occurred */
  state_t *excState = &(supStruct->sup_exceptState[PGFAULTEXCEPT]) int excCode = (excState->cause & GETEXECCODE) >> CAUSESHIFT;
  if (excCode == TLBINVLDM) /* if the exc is a TLB-modification */
    programTrapHandler(excState);
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
    memaddr victimPageAddr = (memaddr) SWAPPOOL + (frameIndex * PAGESIZE);

  }
}
