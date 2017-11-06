#ifndef SPCSM_H
#define SPCSM_H

#include <stddef.h>

/* =================== */
/* ===== Methods ===== */
/* =================== */

int state_machine_SET(unsigned char *, size_t);

int state_machine_UA_TX(unsigned char *, size_t);

int state_machine_UA_RX(unsigned char *, size_t);

int state_machine_D(unsigned char *, size_t, unsigned char);

int state_machine_D_F();

int state_machine_RR(unsigned char *, size_t, unsigned char);

int state_machine_REJ(unsigned char *, size_t, unsigned char);

int state_machine_DISC_TX(unsigned char *, size_t);

int state_machine_DISC_RX(unsigned char *, size_t);

void state_machine_clear();

#endif
