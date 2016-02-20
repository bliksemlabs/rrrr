#ifndef CHULA_COMMON_H
#define CHULA_COMMON_H

#include "macros.h"

typedef enum {
    ret_no_sys          = -4,
    ret_nomem           = -3,
    ret_deny            = -2,
    ret_error           = -1,
    ret_ok              =  0,
    ret_eof             =  1,
    ret_eof_have_data   =  2,
    ret_not_found       =  3,
    ret_file_not_found  =  4,
    ret_eagain          =  5,
    ret_ok_and_sent     =  6
} ret_t;

#endif /* CHULA_COMMON_H */
