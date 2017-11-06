#ifndef SPC_H
#define SPC_H

#include <stddef.h>
#include <termios.h>

/* ===================== */
/* ===== Constants ===== */
/* ===================== */

enum llconstant_t
{
	DATA_MAX							= 509, // Depends on internal buffer

	TRANSMITTER							= 0x0,
	RECEIVER							= 0x1
};

/* ================= */
/* ===== Types ===== */
/* ================= */

typedef struct lldefs_t
{
	int fd;
	struct termios st_default_; // Internal use
	unsigned timeout_seconds;
	unsigned retry_limit;
	unsigned n_actual_; // Internal use
}
lldefs_t;

/* =================== */
/* ===== Methods ===== */
/* =================== */

int llcreate(lldefs_t *);

int lldestroy(int);

int llopen(int, int);

int llwrite(int, unsigned char *, size_t);

int llread(int, unsigned char *);

int llclose(int, int);

#endif
