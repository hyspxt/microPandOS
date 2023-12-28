#include "./headers/msg.h"
static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

/*initialize the msgFree list to contain all the elements of the static array of MAXMESSAGES messages.
This method will be called only once during data structure initialization.*/
void initMsgs()
{
    int i = 0;
    for (; i < MAXMESSAGES; i++)
    {
        list_add(&msgTable[i].m_list, &msgFree_h);
    }
}

void freeMsg(msg_t *m)
{
    list_add(m, &msgFree_h);
}

/*
return NULL if the msgFree list is empty. Otherwise, remove an element from the msgFree list,
provide initial values for ALL of the messages fields and then
return a pointer to the removed element. Messages get
reused, so it is important that no previous value persist in a
message when it gets reallocated.*/
msg_t *allocMsg()
{

    if (list_empty(&msgFree_h) == 1)
        return NULL;
    else if (list_empty(&msgFree_h) == 0)
    {
        msg_t *nms = container_of(msgFree_h.next, msg_t, m_list);
        list_del(&nms->m_list);
        return nms;
    }
    else
        return NULL;
}

void mkEmptyMessageQ(struct list_head *head)
{
    INIT_LIST_HEAD(head);
}

/*
Returns TRUE if the queue whose tail is pointed to by head is empty, FALSE otherwise.*/
int emptyMessageQ(struct list_head *head)
{
    if (list_empty(head) == 1)
        return 1;
    else
        return 0;
}

void insertMessage(struct list_head *head, msg_t *m)
{

    list_add_tail(m, head);
}

/*
Insert the message pointed to by m at the head of the queue whose head pointer is pointed to by
head.*/
void pushMessage(struct list_head *head, msg_t *m)
{
    list_add(m, head);
}

/*remove the first element (starting by the head) from
the message queue accessed via head whose sender is p ptr.
If p ptr is NULL, return the first message in the queue.
Return NULL if the message queue was empty or if no
message from p ptr was found; otherwise return the pointer
to the removed message.*/
msg_t *popMessage(struct list_head *head, pcb_t *p_ptr)
{
    msg_t *nms = container_of(head->next, msg_t, m_list);
    pcb_t *mandante = container_of(head->next, msg_t, m_sender);

    if (emptyProcQ(head) == 1)
        return NULL;
    else if (p_ptr == NULL)
    {

        nms = container_of(head->next, msg_t, m_list);
        list_del(&nms->m_list);
        return nms;
    }

    else
    {

        struct list_head *iter;
        for (iter = (head)->next; iter != (head); iter = iter->next)
        {
            nms = container_of(iter, msg_t, m_list);
            if (nms->m_sender == p_ptr)
            {

                list_del(&nms->m_list);
                return iter;
            }
        }
        return NULL;
    }
}

/*
Return a pointer to the first message from the queue whose head is pointed to by head. Do not
remove the message from the queue. Return NULL if the queue is empty.a*/
msg_t *headMessage(struct list_head *head)
{
    if (emptyMessageQ(head) == 1)
        return NULL;
    else
    {
        return container_of(head->next, msg_t, m_list);
    }
}
