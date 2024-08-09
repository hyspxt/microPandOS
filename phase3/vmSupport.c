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

void askMutex()
{
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex, 0, 0);
}

int pick_frame(){
  static unsigned int counter = 0;
  for (int i = 0; i < POOLSIZE; i++){
    if (isFrameFree(i))
      return i;
  }
  return (counter++ % POOLSIZE);
}

int isFrameFree(int i){
  return swapPoolTable[i].sw_asid == NOASID;
}

void interrupts_off(){
  setSTATUS(getSTATUS() & (~IECON));
}

void interrupts_on(){
  setSTATUS(getSTATUS() | IECON);
}

void updateTLB(pteEntry_t pte){
  setENTRYHI(pte.pte_entryHI);
  TLBP(); /* TLBProbe */
  if ((getINDEX() & PRESENTFLAG) == 0){/* not cached */
    // TODO: check if this is correct
    setENTRYLO(pte.pte_entryLO);
    TLBWI(); /* TLBWrite */
  }
}

int flashOp(int asid, int block, memaddr pageAddr, int operation){
  /* take the register as a non-terminal device register
  interruptLine is obviosly associated with flash devices, and the devNo
  depends on which backing store we're considering (so depends on UProc asid) */
  dtpreg_t *flashReg = (dtpreg_t *)DEV_REG_ADDR(FLASHINT, asid - 1);
  flashReg->data0 = pageAddr; /* load the page address, 4k block to read/write */
  unsigned int s;

  /* pops p.35 - an operation on a flash device is started by loading the
  appropriate value into the COMMAND field. */
  ssi_do_io_t do_io = {
      /* doio service */
      .commandAddr = (unsigned int *)&(flashReg->command),
      .commandValue = (block << BYTELENGTH) | operation,
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
 * @brief Pager component. This is the handler for TLB exceptions.
 *        Permits the system to manage the virtual memory and address translations.
 *
 * @param void
 * @return void
 */
void pager()
{
  /* obtain the sup struct from currproc */
  support_t *sup = getSupStruct();
  /* detrmine the cause of the TLB exception occurred */
  state_t *supState = &sup->sup_exceptState[PGFAULTEXCEPT];
  if(CAUSE_GET_EXCCODE(supState->cause) == TLBINVLDM) /* TLB-modification */
    programTrapHandler(); /* treat it as progtrap */

  /* gain mutual exclusion over the spt */
  askMutex();
  /* determine the missing page number (same as TLBRefill) */
  int p = ENTRYHI_GET_VPN(supState->entry_hi);
  p = MIN(p, MAXPAGES - 1); /* bound check */
  /* get a frame from the swap pool with a replacement algo,
    determining if it's free and can contain a page */
  int victim_page = pick_frame();
  swap_t *spte = &swapPoolTable[victim_page];
  memaddr victim_page_addr = (memaddr)0x20020000 + (victim_page * PAGESIZE);

  /* check if the frame is currently occupied */
  if(!isFrameFree(victim_page)){
    /* mark page as not valid, atomically disabiliting interrupts - 5.3 specs */
    /* update also the TLB, if needed */
    interrupts_off();
    pteEntry_t *victim_pte = spte->sw_pte;
    victim_pte->pte_entryLO &= (~VALIDON); /* mark the page as not valid */
    updateTLB(*victim_pte);
    interrupts_on();

    /* write on backing store */
    if (flashOp(spte->sw_asid, spte->sw_pageNo, victim_page_addr, FLASHWRITE) != READY){
      SYSCALL(SENDMESSAGE, (unsigned int) swap_mutex, 0, 0);
      programTrapHandler();
    }
  }

  /* read from backing store */
  if (flashOp(sup->sup_asid, p, victim_page_addr, FLASHREAD) != READY){
    SYSCALL(SENDMESSAGE, (unsigned int) swap_mutex, 0, 0);
    programTrapHandler();
  }
  
  /* update the swap pool table */
  spte->sw_asid = sup->sup_asid;
  spte->sw_pageNo = p;
  spte->sw_pte = &sup->sup_privatePgTbl[p];

  interrupts_off();
  sup->sup_privatePgTbl[p].pte_entryLO |= VALIDON; /* mark the page as valid */
  sup->sup_privatePgTbl[p].pte_entryLO |= DIRTYON; /* mark the page as dirty */
  sup->sup_privatePgTbl[p].pte_entryLO &= 0xFFF;
  sup->sup_privatePgTbl[p].pte_entryLO |= (victim_page_addr);
  updateTLB(sup->sup_privatePgTbl[p]);
  interrupts_on();

  /* release the mutex */
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex, 0, 0);
  LDST(supState);
}
