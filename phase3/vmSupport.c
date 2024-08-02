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
  int cause = sup->sup_exceptContext[PGFAULTEXCEPT].cause;
  int excCode = (cause & GETEXECCODE) >> CAUSESHIFT;
  if (excCode == TLBINVLDM) /* if the exc is a TLB-modificiation */
}
