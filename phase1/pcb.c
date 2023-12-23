#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);

static int next_pid = 1;

/**
 * @brief Inizializza la lista dei PCB liberi pcbFree_h in modo che contenga tutti gli elementi dell'array statico dei PCB, 
 *        ovvero pcbTable di dimensione MAXPROC. Questa funzione viene chiamata una volta sola in fase di inizializzazione della struttura dati.
 * 
 * @brief-eng Initializes the list of free PCBs pcbFree_h so that it contains all the elements of the static array of PCBs,
 *            that is pcbTable of size MAXPROC. This function is called only once during the initialization of the data structure.
 *
 * @param void
 * @return void
 *
 */
void initPcbs() {

    for (int i = MAXPROC - 1; i >= 0; i--)
    {
      list_add (&pcbTable[i].p_list, &pcbFree_h);
      // pcbTable[i].p_pid = i + 1;
    }
}

/**
 * @brief      Aggiunge un PCB alla lista dei PCB liberi. Dunque, inserisce l elemento puntato da p nella lista dei PCB liberi (pcbFree_h).
 * 
 * @brief-eng  Adds a PCB to the list of free PCBs. Therefore, it inserts the element pointed to by p into the list of free PCBs (pcbFree_h).
 *
 * @param pcb_t *p:  Puntatore al PCB da inserire nella lista dei PCB liberi.
 * @return void
 *
 */
void freePcb(pcb_t *p) {
    list_add(p, &pcbFree_h);
}

/**
 * @brief      Ritorna NULL se la lista dei PCB liberi è vuota. Altrimenti alloca e restituisce il puntatore al primo PCB libero, rimuovendolo
 *             dalla lista dei PCB liberi. É importante che tutti i campi vengano reinizializzati, in quanto il PCB potrebbe essere stato usato
 *             in precedenza.
 * 
 * @brief-eng  Returns NULL if the list of free PCBs is empty. Otherwise it allocates and returns the pointer to the first free PCB, removing it
 *             from the list of free PCBs. It is important that all fields are reinitialized, as the PCB may have been used previously.  
 *
 * @param      void
 * @return     pcb_t *: Puntatore al primo PCB libero.
 *
 */
pcb_t *allocPcb() {
    if(list_empty(&pcbFree_h)) return NULL;
    else{
        pcb_t *nPcb = container_of(pcbFree_h.next,pcb_t,p_list);
        nPcb->p_parent = NULL;
        list_del(&nPcb->p_list);
        mkEmptyProcQ(&nPcb->p_list);
        mkEmptyProcQ(&nPcb->p_child);
        mkEmptyProcQ(&nPcb->p_sib);
        nPcb->p_time = 0;
        nPcb->p_supportStruct = NULL;
        return nPcb;
    }
}

/**
 * @brief      Inizializza una lista vuota, ovvero una variabile puntatore head che punta a una lista di processi.
 * 
 * @brief-eng  Initializes an empty list, i.e. a head pointer variable that points to a process queue.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da inizializzare. 
 * @return     void
 *
 */
void mkEmptyProcQ(struct list_head *head) {
    INIT_LIST_HEAD(head);
}

/**
 * @brief      Verifica se una lista é vuota, restituendo TRUE, altrimenti restituisce FALSE.
 * 
 * @brief-eng  Returns TRUE if the list pointed to by head is empty, FALSE otherwise.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da controllare.
 * @return     int: TRUE se la lista è vuota, FALSE altrimenti.
 *
 */
int emptyProcQ(struct list_head *head) {
    return (list_empty(head));
}

/**
 * @brief      Inserisce il PCB puntato da p nella coda dei processi puntata da head.
 * 
 * @brief-eng  Inserts the PCB pointed to by p into the process queue pointed to by head.
 *
 * @param      list_head head *: Il puntatore alla testa della lista in cui inserire il processo.
 * @param      pcb_t p *: Il puntatore al processo da inserire nella lista.
 * @return     void
 *
 */
void insertProcQ(struct list_head *head, pcb_t *p) {
    list_add_tail(&p->p_list, head);   // é una coda, quindi si dovrebbe inserire dalla tail.
}

/**
 * @brief      Restituisce un puntatore al primo PCB della lista dei processi la cui testa é puntata da head. É importante che
 *            il PCB non venga rimosso dalla lista dei processi. Si ritorna NULL se la lista dei processi é vuota.
 * 
 * @brief-eng  Returns a pointer to the first PCB in the process list whose head is pointed to by head. It is important that
 *             the PCB is not removed from the process list. NULL is returned if the process list is empty.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da cui ottenere il primo PCB.
 * @return     pcb_t: Il puntatore al primo PCB nella coda, oppure NULL se la coda é vuota.
 *
 */
pcb_t *headProcQ(struct list_head *head) {
    if(emptyProcQ(head)) return NULL;
    else return container_of(head->next, pcb_t, p_list);
}

/**
 * @brief      Rimuove il primo (i.e. head) elemento dalla coda dei processi puntata da head. Ritorna NULL se la coda é inizialmente vuota, 
 *             altrimenti ritorna un puntatore al PCB rimosso dalla coda dei processi.
 * 
 * @brief-eng  Removes the first (i.e. head) element from the process queue pointed to by head. Returns NULL if the queue is initially empty,
 *             otherwise it returns a pointer to the PCB removed from the process queue.
 *
 * @param      list_head head *: Il puntatore alla testa della lista da cui rimuovere il primo PCB.
 * @return     pcb_t *: Il puntatore al PCB che é stato rimosso dalla coda dei processi, NULL se la coda é vuota.
 *
 */
pcb_t *removeProcQ(struct list_head *head) {
    if(emptyProcQ(head)) return NULL;
    else{
        pcb_t *removed_p = container_of(head->next, pcb_t, p_list);
        list_del(head->next);
        return removed_p;
    }
}


/**
 * @brief      Rimuove il PCB puntato da p dalla coda dei processi puntata da head. Ritorna NULL se il PCB non é presente nella coda.
 *             Altrimenti ritorna p. Si fa notare come p possa puntare a un qualsiasi elemento della coda.
 * 
 * @brief-eng  Removes the PCB pointed to by p from the process queue pointed to by head. Returns NULL if the PCB is not present in the queue.
 *             Otherwise it returns p. It should be noted that p can point to any element of the queue.
 *
 * @param      list_head head *: puntatore alla testa della coda dei processi da cui bisogna rimuovere il PCB.
 * @param      pcb_t p *: puntatore al PCB da rimuovere dalla coda dei processi.
 * @return     Puntatore al PCB rimosso dalla coda dei processi, NULL se il PCB non é presente nella coda.
 */
pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
    pcb_t *track;
    list_for_each_entry(track, head, p_list){
        if(track == p){
            list_del(&track->p_list);
            return p;
        }
    }
    return NULL;
}

/**
 * @brief      Controlla se un certo PCB puntato ha dei figli. Ritorna TRUE se non ne ha nessuno, FALSE altrimenti.
 * 
 * @brief-eng  Checks if a certain PCB pointed to has children, returning TRUE if it has no one, FALSE otherwise.
 *
 * @param      pcb_t p *: puntatore al PCB di cui bisogna verificare se ha dei figli.
 * @return     int: TRUE se il PCB non ha figli, FALSE altrimenti.
 */
int emptyChild(pcb_t *p) {
    return(list_empty(&p->p_child));
}

/**
 * @brief      Fa si che il PCB puntato da p diventi un figlio del PCB puntato da prnt.
 * 
 * @brief-eng  Makes the PCB pointed to by p a child of the PCB pointed to by prnt.
 *
 * @param      pcb_t prnt *: puntatore al PCB padre.
 * @param      pcb_t p *: puntatore al PCB che deve diventare figlio del padre.
 * @return     void
 */
void insertChild(pcb_t *prnt, pcb_t *p) {
    list_add_tail(&p->p_sib, &prnt->p_child);
    p->p_parent = prnt;
}


pcb_t *removeChild(pcb_t *p) {

}

pcb_t *outChild(pcb_t *p) {

}
