#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];
LIST_HEAD(pcbFree_h);
static int next_pid = 1;

void initPcbs() {
    for(int n = 0; n < length(pcbTable); n++)
        list_add_tail(&pcbTable[n], &pcbFree_h);
}

void freePcb(pcb_t *p) {
    list_add(p, &pcbFree_h);
}

pcb_t *allocPcb() {
    if(list_empty(&pcbFree_h) == 1) return NULL;
    else{
        pcb_t tmp;
        tmp.p_list = pcbFree_h;
        list_del(&pcbFree_h);
        INIT_LIST_HEAD(&pcbFree_h);
        return &tmp;
    }
}

void mkEmptyProcQ(struct list_head *head) {
}

int emptyProcQ(struct list_head *head) {
}

void insertProcQ(struct list_head *head, pcb_t *p) {
}

pcb_t *headProcQ(struct list_head *head) {
}

pcb_t *removeProcQ(struct list_head *head) {
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
}

int emptyChild(pcb_t *p) {
    return(list_empty(&pcbTable->p_child));
}

void insertChild(pcb_t *prnt, pcb_t *p) {
}

pcb_t *removeChild(pcb_t *p) {
}

pcb_t *outChild(pcb_t *p) {
}
