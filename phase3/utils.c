#include "./headers/lib.h"
typedef unsigned int devregtr;

/*
    This module contains some useful functions for process
    creation, printing on devices... etc. These are taken maily
    from the given p2test for phase2 and slighly modified.
*/

/**
 * @brief Requests a create process service to the ssi process. The service
 *        returns a pcb pointer to the newly created process.
 *
 * @param state_t - state of the pcb to create
 * @param support_t - support struct (to inherit) of the pcb to create
 * @return the pcb pointer to the newly created process
 */
pcb_PTR create_process(state_t *s, support_t *sup)
{ /* Protocol: send a msg to ssi -> await for response -> return pcb */
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
 *        to the ssi process. The service returns a pointer to the support
 *        struct of current_process, that will be inherited by its child.
 *
 * @param void
 * @return
 */
support_t *getSupStruct()
{ /* Protocol: send a msg to ssi -> await for response -> return sup */
    support_t *sup;
    ssi_payload_t getsup_payload = {
        .service_code = GETSUPPORTPTR,
        .arg = NULL,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup), 0);
    return sup;
}

/**
 * @brief Send a termination request to the ssi process. The service
 *        make the ssi to kill the process and all its progeny.
 *
 * @param pcb_PTR - pcb to terminate (NULL for current process)
 * @return void
 */
void sendKillReq(pcb_PTR p)
{
    ssi_payload_t term_process_payload = {
        .service_code = TERMPROCESS,
        .arg = (void *)p,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&term_process_payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

/**
 * @brief  Print a string of characters after operations on backing stores on terminal/printer
 *         devices. The string and its length it's retrieved by the SST with
 *         message passing and then printed with SSI doio service.
 *         All values (base, command, data0) are calculated accorting to umps - pops
 *
 * @param asid - index of the backing store
 * @param deviceType - identifies type of device (terminal/printer)
 * @return void
 */
void printDevice(int asid, int deviceType)
{
    while (1)
    {
        char *msg;                                       /* char that starts the print */
        devregtr *base, *command, *data0, status, value; /* device register values */

        /* get the sst_print_t pointer in which we found the string to print,
        the message is sent by the writeX, where X is the device type */
        unsigned int sender = (unsigned int)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&msg), 0);
        switch (deviceType)
        {
        case IL_PRINTER:                     /* pops p.39 for printers */
            base = (devregtr *)(PRINT0ADDR); /* base device address for printer */
            base += asid * DEVREGLEN;        /* offset for the device */
            command = base + 1;              /* COMMAND field p28 pops */
            data0 = base + 2;                /* DATA0 field where to put char to print */
            break;
        case IL_TERMINAL:                   /* pops p.41 for terminals */
            base = (devregtr *)(TERM0ADDR); /* base device address for terminal 0 */
            base += asid * DEVREGLEN;       /* offset for the device */
            /* this because there are 4 (0-3) possible fields in the layout, so
            (0-3) is the field layout for devNo 0, and (4-7) for devNo 1 */
            data0 = base + 2; /* we want TRANSM_COMMAND that is in fact 3rd and last field */
            command = base + TRANCOMMAND;
            break;
        }
        while (*msg != EOS)
        { /* iterating until we reach the null-term of the string, char by char */
            switch (deviceType)
            {
            case IL_PRINTER: /* printer transmit the char in data0 over the line */
                *data0 = (unsigned int)*msg;
                value = PRINTCHR;
                break;
            case IL_TERMINAL: /* terminal don't use data0 at all */
                value = PRINTCHR | (((devregtr)*msg) << 8);
                break;
            } /* prepping the doio payload for requesting doio service to sssi */
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
            msg++;
        } /* unblock sst */
        SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    }
}
