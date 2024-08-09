#include "./headers/lib.h"
typedef unsigned int devregtr;

/*
    This module contains some useful functions for process
    creation, printing on devices... etc. These are taken directly
    from the given p2test for phase2 and slighly modified.
*/

/**
 * @brief Create and return a process, represented by a PCB.
 *
 * @param state_t - state of the pcb to create
 * @param support_t -
 * @return void
 */
pcb_PTR create_process(state_t *s, support_t *sup)
{
    pcb_PTR p;
    ssi_create_process_t ssi_create_process = {
        .state = s,
        .support = sup,
    };
    ssi_payload_t payload = {
        .service_code = CREATEPROCESS,
        .arg = &ssi_create_process,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);
    return p;
}

/**
 * @brief Get the support struct requesting the associated service
 *        to the ssi process. The support struct is taken from sender
 *        data (and eventually inherited by its children).
 *
 * @param void
 * @return a support struct associated with the process
 */
support_t *getSupStruct()
{
    support_t *sup;
    ssi_payload_t getsup_payload = {
        .service_code = GETSUPPORTPTR,
        .arg = NULL,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup), 0);
    return sup;
}

void sendKillReq()
{
    ssi_payload_t sst_payload = {
        .service_code = TERMPROCESS,
        .arg = NULL,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&sst_payload, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

/**
 * @brief  Print a string of characters using flash devices on terminal/printer
 *         devices. The string and its length it's retrieved by the SST with
 *         message passing and then printed with SSI doio service.
 *
 * @param asid - index of the backing store
 * @param device - identifies type of device (terminal/printer)
 * @return a support struct associated with the process
 */
void printDevice(int asid, int device, sst_print_PTR print)
{
    char *msg = print->string; /* char that starts the print */
    int len = print->length;   /* length of the string */
    devregtr *base, *command, *data0, status;
    switch (device)
    {
    case IL_PRINTER:                     /* pops p.39 for printers */
        base = (devregtr *)(PRINT0ADDR); /* base device address for printer */
        base += asid * WORD_SIZE;        /* offset for the device */
        command = base + 1;              /* COMMAND field p28 pops */
        data0 = base + 2;                /* DATA0 field where to put char to print */
        break;
    case IL_TERMINAL:                   /* pops p.41 for terminals */
        base = (devregtr *)(TERM0ADDR); /* base device address for terminal 0 */
        base += asid * DEVREGLEN;       /* offset for the device */
        /* this because there are 4 (0-3) possible fields in the layout, so
        (0-3) is the field layout for devNo 0, and (4-7) for devNo 1 */
        command = base + TRANCOMMAND; /* we want TRANSM_COMMAND that is in fact 3rd and last field */
        break;
    }
    while (*msg != EOS)
    { /* iterating until we reach the null-term of the string, char by char */
        devregtr value;
        switch (device)
        {
        case IL_PRINTER: /* printer transmit the char in data0 over the line */
            *data0 = (unsigned int)*msg;
            value = PRINTCHR;
            break;
        case IL_TERMINAL: /* terminal don't use data0 at all */
            value = PRINTCHR | (((devregtr)*msg) << 8);
            break;
        }
        /* prepping the doio structs for requesting doio service to sssi */
        ssi_do_io_t do_io = {
            .commandAddr = command,
            .commandValue = value,
        };
        ssi_payload_t payload = {
            .service_code = DOIO,
            .arg = &do_io,
        };
        SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
        SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

        if ((status & TERMSTATMASK) != RECVD)
            PANIC();
        msg++;
    } /* unblock sst */
}
