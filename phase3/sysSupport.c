#include "./headers/lib.h"

/**
 * @brief The central exception handler for the Support Level that redirects the control 
 *        to the appropriate handler, based on the cause/type of the singular exception occurred.
 *
 * @param void
 * @return void
 */
void supExceptionHandler(){
    support_t *supStruct = getSupStruct();
    /* can't use EXCEPTION_STATE macro, it's not in BIOSDATAPAGE 
    but in sup_exceptState */
    state_t *state = &(supStruct->sup_exceptState[GENERALEXCEPT]);

}