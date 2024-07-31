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
  swapPoolTable[i].sw_asid = OFF;   /* 6bit identifier
        to distinguish the process' kuseg from the others*/
  swapPoolTable[i].sw_pageNo = OFF; /* sw_pageNo is the
    virtual page number in the swap pool.*/
  swapPoolTable[i].sw_pte = NULL;   /* pointer to the page table entry */
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
  /* TODO */
}
