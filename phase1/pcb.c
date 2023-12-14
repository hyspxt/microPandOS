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
    

   /* for(int n = 0; n < length(pcbTable); n++)
        list_add_tail(&pcbTable[n], &pcbFree_h); */

    
}

/**
 * @brief      Aggiunge un PCB alla lista dei PCB liberi. Dunque, inserisce l elemento puntato da p nella lista dei PCB liberi (pcbFree_h).
 * 
 * @brief-eng  Adds a PCB to the list of free PCBs. Therefore, it inserts the element pointed to by p into the list of free PCBs (pcbFree_h).
 *
 * @param p:  Puntatore al PCB da inserire nella lista dei PCB liberi.
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
        pcb_t tmp;
        tmp.p_list = pcbFree_h;
        list_del(&pcbFree_h);
        INIT_LIST_HEAD(&pcbFree_h);
        return &tmp;
    }
}

void mkEmptyProcQ(struct list_head *head) {
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head *head) {
    return (list_empty(head));
}

void insertProcQ(struct list_head *head, pcb_t *p) {
    list_add(p, head);
}

pcb_t *headProcQ(struct list_head *head) {
    if(emptyProcQ(head)) return NULL;
    else return container_of(head, pcb_t, p_list);
}

pcb_t *removeProcQ(struct list_head *head) {
    if(emptyProcQ(head)) return NULL;
    else{
        pcb_t tmp = headProcQ(head);
        list_del(&tmp->p_list);
        return tmp;
    }
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
    
}

int emptyChild(pcb_t *p) {
    return(list_empty(p->p_child));
}

void insertChild(pcb_t *prnt, pcb_t *p) {
}

pcb_t *removeChild(pcb_t *p) {
}

pcb_t *outChild(pcb_t *p) {
}
