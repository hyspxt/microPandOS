#include "./headers/msg.h"
static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

/**
 * @brief Inizializza la lista dei messaggi liberi (msgFree) in modo da contenere tutti gli elementi dell'array statico di MAXMESSAGES messaggi.
 *        Questa funzione viene chiamata una volta sola in fase di inizializzazione della struttura dati.
 * 
 * @brief-eng Initializes the list of free messages (msgFree) so that it contains all the elements of the static array of MAXMESSAGES messages.
 *            This function is called only once during the initialization of the data structure.
 * 
 * @param void
 * @return void
 */
void initMsgs()
{
    for (int i = 0; i < MAXMESSAGES; i++)
    {
        list_add(&msgTable[i].m_list, &msgFree_h);
    }
}

/**
 * @brief Inserisce l'elemento puntato da p nella lista dei messaggi liberi (msgFree).
 * 
 * @brief-eng Inserts the element pointed to by p into the list of free messages (msgFree).
 * 
 * @param msg_t *m: Puntatore al messaggio da inserire nella lista dei messaggi liberi.
 * @return void
 */
void freeMsg(msg_t *m)
{
    list_del(&m->m_list);
    list_add_tail(&m->m_list,&msgFree_h);
}


/**
 * @brief Ritorna NULL se la lista dei messaggi liberi (msgFree) è vuota. Altrimenti rimuove un elemento dalla lista dei messaggi liberi,
 *        fornisce valori iniziali per TUTTI i campi del messaggio e restituisce un puntatore all'elemento rimosso. I messaggi vengono
 *        riutilizzati, dunque è importante che nessun valore precedente persista in un messaggio quando viene riallocato.
 * 
 * @brief-eng Returns NULL if the list of free messages (msgFree) is empty. Otherwise, remove an element from the list of free messages,
 *            provide initial values for ALL of the messages fields and then return a pointer to the removed element. Messages get
 *            reused, so it is important that no previous value persist in a message when it gets reallocated.
 * 
 * @param void
 * @return msg_t *: Puntatore al primo messaggio libero, oppure NULL nel caso di lista vuota.
 */
msg_t *allocMsg()
{
   if(list_empty(&msgFree_h)) return NULL;
    else{
        msg_t *nms = container_of(msgFree_h.next, msg_t, m_list);
        list_del(msgFree_h.next);
        mkEmptyMessageQ(&nms->m_list);
        // here we re-initialize the message
        nms->m_sender = NULL;
        nms->m_payload = 0;
        return nms;
    }
}

/**
 * @brief Si inizializza una lista vuota di messaggi attraverso una variabile puntatore head.
 * 
 * @brief-eng Initializes an empty list of messages through a head pointer variable.
 * 
 * @param void
 * @return void
 */
void mkEmptyMessageQ(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}


/**
 * @brief Ritorna TRUE se la coda la cui fine é puntata da head é vuota. FALSE altrimenti.
 * 
 * @brief-eng Returns TRUE if the queue whose tail is pointed to by head is empty. FALSE otherwise.
 * 
 * @param list_head head *: Il puntatore alla coda della lista da controllare.
 * @return int: TRUE se la lista é vuota, FALSE altrimenti.
 */
int emptyMessageQ(struct list_head *head)
{
    return list_empty(head);
}

/**
 * @brief Inserisce il messaggio puntato da m alla FINE della coda la cui testa é puntata da head.
 * 
 * @brief-eng Insert the message pointed to by m at the end (tail) of the queue whose head pointer is pointed to by head.
 * 
 * @param list_head head *: puntatore alla testa della lista da controllare.
 * @param msg_t *m: puntatore al messaggio da inserire nella coda.
 * 
 * @return void
 */
void insertMessage(struct list_head *head, msg_t *m)
{
    list_add_tail(&m->m_list,head);
}


/**
 * @brief Inserisce il messaggio puntato da m all'INIZIO della coda la cui testa é puntata da head.
 * 
 * @brief-eng Insert the message pointed to by m at the head of the queue whose head pointer is pointed to by head.
 * 
 * @param list_head head *: Il puntatore alla testa della lista da controllare.
 * @param msg_t *m: Il puntatore al messaggio da inserire nella coda.
 * 
 * @return void
 */
void pushMessage(struct list_head *head, msg_t *m)
{
    list_add(&m->m_list,head);
}


/**
 * @brief Rimuove il primo elemento (partendo dalla testa) dalla coda dei messaggi acceduta tramite head il cui mittente é p ptr.
 *        Se p ptr é NULL, restituisce il primo messaggio nella coda. Restituisce NULL se la coda dei messaggi era vuota o se non
 *        é stato trovato nessun messaggio da p ptr; altrimenti restituisce il puntatore al messaggio rimosso.
 * 
 * @brief-eng Remove the first element (starting by the head) from the message queue accessed via head whose sender is p ptr.
              If p ptr is NULL, return the first message in the queue. Return NULL if the message queue was empty or if no
              message from p ptr was found; otherwise return the pointer to the removed message.
 * 
 * @param list_head head *: Il puntatore alla testa della lista da controllare.
 * @param pcb_t *p_ptr: Il puntatore al processo mittente.
 * 
 * @return msg_t *: Il puntatore al messaggio rimosso, NULL se la coda dei messaggi era vuota o se non é stato trovato nessun messaggio da p ptr.
 */
msg_t *popMessage(struct list_head *head, pcb_PTR p_ptr)
{
    if(emptyMessageQ(head)) return NULL;
    if(p_ptr == NULL){
        msg_t *nms = container_of(head->next, msg_t, m_list);
        list_del(head->next);
        return nms;
    }
    msg_t *iter;
    list_for_each_entry(iter, head, m_list){
        if(iter->m_sender == p_ptr){
            list_del(&iter->m_list);
            return(iter);
        }
    }
    return NULL;
}


/**
 * @brief Restituisce un puntatore al primo messaggio dalla coda la cui testa é puntata da head. Non rimuove il messaggio dalla coda.
 *        Restituisce NULL se la coda é vuota.
 * 
 * @brief-eng Return a pointer to the first message from the queue whose head is pointed to by head. 
 *            Do not remove the message from the queue. Return NULL if the queue is empty.
 * 
 * @param list_head head *: Il puntatore alla testa della lista da controllare.
 * 
 * @return msg_t *: Il puntatore al primo messaggio nella coda, oppure NULL se la coda é vuota.
 */
msg_t *headMessage(struct list_head *head)
{
    if (emptyMessageQ(head)) return NULL;
    else return container_of(head->next, msg_t, m_list);
}
