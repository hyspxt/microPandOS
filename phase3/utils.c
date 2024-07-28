#include "./headers/lib.h"

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
pcb_PTR create_process(state_t *s, support_t *sup){
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


