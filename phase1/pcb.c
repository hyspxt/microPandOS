#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

/**
 * @brief     Initializes the list of free PCBs pcbFree_h so that it contains all the elements of the static array of PCBs,
 *            that is pcbTable of size MAXPROC. This function is called only once during the initialization of the data structure.
 *
 * @param void
 * @return void
 */
void initPcbs()
{

    for (int i = MAXPROC - 1; i >= 0; i--)
    {
        list_add(&pcbTable[i].p_list, &pcbFree_h);
    }
}

/**
 * @brief    Provides default values to the pcb when allocated.
 *
 * @param void
 * @return void
 */
static void initPcbValues(pcb_PTR p)
{

    // initialize the pcb structures
    INIT_LIST_HEAD(&p->p_list);
    p->p_parent = NULL;
    INIT_LIST_HEAD(&p->p_child);
    INIT_LIST_HEAD(&p->p_sib);
    INIT_LIST_HEAD(&p->msg_inbox);

    p->p_s.cause = 0;
    p->p_s.entry_hi = 0;
    p->p_supportStruct = NULL;

    /* Set up the general purpose register, not sure if is necessary, since p->p_s.t9 exists */
    for (int i = 0; i < STATE_GPR_LEN; i++)
        p->p_s.gpr[i] = 0;

    p->p_s.hi = 0;
    p->p_s.lo = 0;
    p->p_s.pc_epc = 0;
    p->p_s.status = 0;
    p->p_time = 0;

    p->p_pid = next_pid++;
}

/**
 * @brief     Adds a PCB to the list of free PCBs. Therefore, it inserts the element pointed to by p into the list of free PCBs (pcbFree_h).
 *
 * @param pcb_t *p:  Puntatore al PCB da inserire nella lista dei PCB liberi.
 * @return void
 */
void freePcb(pcb_t *p)
{
    list_add_tail(&p->p_list, &pcbFree_h);
}

/**
 * @brief      Returns NULL if the list of free PCBs is empty. Otherwise it allocates and returns the pointer to the first free PCB, removing it
 *             from the list of free PCBs. It is important that all fields are reinitialized, as the PCB may have been used previously.
 *
 * @param      void
 * @return     pcb_t *: Puntatore al primo PCB libero.
 */
pcb_t *allocPcb()
{
    if (list_empty(&pcbFree_h))
        return NULL;
    else
    {
        pcb_t *nPcb = container_of(pcbFree_h.next, pcb_t, p_list);
        list_del(pcbFree_h.next);
        initPcbValues(nPcb);
        return nPcb;
    }
}

/**
 * @brief      Initializes an empty list, i.e. a head pointer variable that points to a process queue.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da inizializzare.
 * @return     void
 */
void mkEmptyProcQ(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}

/**
 * @brief      Returns TRUE if the list pointed to by head is empty, FALSE otherwise.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da controllare.
 * @return     int: TRUE se la lista è vuota, FALSE altrimenti.
 */
int emptyProcQ(struct list_head *head)
{
    return list_empty(head);
}

/**
 * @brief      Inserts the PCB pointed to by p into the process queue pointed to by head.
 *
 * @param      list_head head *: Il puntatore alla testa della lista in cui inserire il processo.
 * @param      pcb_t p *: Il puntatore al processo da inserire nella lista.
 * @return     void
 */
void insertProcQ(struct list_head *head, pcb_t *p)
{
    list_add_tail(&p->p_list, head); // é una coda, quindi si dovrebbe inserire dalla tail.
}

/**
 * @brief      Returns a pointer to the first PCB in the process list whose head is pointed to by head. It is important that
 *             the PCB is not removed from the process list. NULL is returned if the process list is empty.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da cui ottenere il primo PCB.
 * @return     pcb_t: Il puntatore al primo PCB nella coda, oppure NULL se la coda é vuota.
 */
pcb_t *headProcQ(struct list_head *head)
{
    if (emptyProcQ(head))
        return NULL;
    else
        return container_of(head->next, pcb_t, p_list);
}

/**
 * @brief      Removes the first (i.e. head) element from the process queue pointed to by head. Returns NULL if the queue is initially empty,
 *             otherwise it returns a pointer to the PCB removed from the process queue.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da cui rimuovere il primo PCB.
 * @return     pcb_t *: Il puntatore al PCB che é stato rimosso dalla coda dei processi, NULL se la coda é vuota.
 */
pcb_t *removeProcQ(struct list_head *head)
{
    if (emptyProcQ(head))
        return NULL;
    else
    {
        pcb_t *first_pcb = container_of(list_next(head), pcb_t, p_list);       
        list_del(&(first_pcb->p_list)); 
        return first_pcb;               
    }
}

/**
 * @brief      Removes the PCB pointed to by p from the process queue pointed to by head. Returns NULL if the PCB is not present in the queue.
 *             Otherwise it returns p. It should be noted that p can point to any element of the queue.
 *
 * @param      list_head head *: puntatore alla testa della coda dei processi da cui bisogna rimuovere il PCB.
 * @param      pcb_t p *: puntatore al PCB da rimuovere dalla coda dei processi.
 * @return     Puntatore al PCB rimosso dalla coda dei processi, NULL se il PCB non é presente nella coda.
 */
pcb_t *outProcQ(struct list_head *head, pcb_t *p)
{
    if (p == NULL)
        return NULL;
    struct list_head *iter;
    list_for_each(iter, head)
    {
        if (iter == &p->p_list)
        {
            list_del(iter);
            return p;
        }
    }
    return NULL;
}

/**
 * @brief      Checks if a certain PCB pointed to has children, returning TRUE if it has no one, FALSE otherwise.
 *
 * @param      pcb_t p *: puntatore al PCB di cui bisogna verificare se ha dei figli.
 * @return     int: TRUE se il PCB non ha figli, FALSE altrimenti.
 */
int emptyChild(pcb_t *p)
{
    return list_empty(&p->p_child);
}

/**
 * @brief      Makes the PCB pointed to by p a child of the PCB pointed to by prnt.
 *
 * @param      pcb_t prnt *: puntatore al PCB padre.
 * @param      pcb_t p *: puntatore al PCB che deve diventare figlio del padre.
 * @return     void
 */
void insertChild(pcb_t *prnt, pcb_t *p)
{
    p->p_parent = prnt;
    list_add_tail(&p->p_sib, &prnt->p_child);
}

/**
 * @brief      Makes the first child of the PCB pointed to by p no longer have p as its parent. Returns NULL if there are no children of p initially.
 *             Otherwise, it returns a pointer to the removed child PCB.
 *
 * @param      pcb_t p *: puntatore al PCB di cui rimuovere il primo figlio.
 * @return     pcb_t *: puntatore al PCB del figlio rimosso, NULL se non ci sono figli.
 */
pcb_t *removeChild(pcb_t *p)
{
    if (emptyChild(p))
        return NULL;
    else
    {
        pcb_t *first_child = container_of(p->p_child.next, pcb_t, p_sib);
        list_del(p->p_child.next);
        return first_child;
    }
}

/**
 * @brief      Makes the PCB pointed to by p no longer a child of its parent. If the PCB pointed to by p has no parent, it returns NULL.
 *             Otherwise, it returns p, i.e. the pointer of the removed child. It should be noted that the element pointed to by p can be found in an
 *             arbitrary position in the list of the parent's children (i.e. p may not be the parent's first child).
 *
 * @param      pcb_t p *: puntatore al PCB figlio che deve essere rimosso (in quanto figlio) dal genitore.
 * @return     pcb_t *: puntatore al PCB del figlio rimosso, NULL se p non ha genitore.
 */
pcb_t *outChild(pcb_t *p)
{
    if (p->p_parent == NULL)
        return NULL;
    else
    {
        p->p_parent = NULL;
        list_del(&p->p_sib);
        return p;
    }
}

/**
 * @brief     Search a pcb pointer in a list (of pcbs).
 *            Returns 1 if the pcb is found, 0 otherwise.
 *
 * @param      pcb_PTR p: pointer to be found in the list.
 * @param      struct list_head *list: pointer to the list where to search the pcb.
 * @return     int: 1 if the pcb is found, 0 otherwise.
 */
unsigned int searchProcQ(pcb_PTR p, struct list_head *list)
{
    pcb_PTR nPcb;
    pcb_PTR pcb_toCheck = p;
    list_for_each_entry(nPcb, list, p_list)
    {
        if (nPcb == pcb_toCheck)
            return 1;
        else if (p == NULL)
            return 0;
    }
    return 0;
}

/**
 * @brief     Unblocks a pcb that is blocked on a device.
 *            Returns the pcb if it is found, NULL otherwise.
 *
 * @param      unsigned_int devNo: device number.
 * @param      struct list_head *list: pointer to the list where to search the pcb.
 * @return     pcb_PTR: pointer to the pcb that was unblocked, NULL if not found.
 */
pcb_PTR unlockPcbDevNo(unsigned int devNo, struct list_head *list)
{
    pcb_PTR nPcb;
    list_for_each_entry(nPcb, list, p_list)
    {
        if (nPcb->blockedOnDevice == devNo)
            return outProcQ(list, nPcb);
    }
    return NULL;
}