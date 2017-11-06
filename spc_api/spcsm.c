#include "spcsm.h"

/* ===================== */
/* ===== Constants ===== */
/* ===================== */

enum universal_frame_bytes_t
{
	F									= 0x7E,
	A_TX								= 0x03,
	A_RX								= 0x01
};

enum control_frame_bytes_t
{
	C_SET								= 0x03,
	C_DISC								= 0x0B,
	C_UA								= 0x07,
	C_RR								= 0x05,
	C_REJ								= 0x01
};

enum data_frame_bytes_t
{
	C_D									= 0x00
};

/* ============================= */
/* ===== State Machine SET ===== */
/* ============================= */

static int state_actual_SET = 5;

int state_machine_SET(unsigned char * buffer, size_t buffer_pos)
{
	switch (state_actual_SET)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_SET;
			else
				state_actual_SET = 5;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_SET;
			else if (buffer[buffer_pos] == F)
				state_actual_SET = 4;
			else
				state_actual_SET = 5;
			break;
		case 3: // Checking C_SET
			if (buffer[buffer_pos] == C_SET)
				--state_actual_SET;
			else if (buffer[buffer_pos] == F)
				state_actual_SET = 4;
			else
				state_actual_SET = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_SET;
			else if (buffer[buffer_pos] == F)
				state_actual_SET = 4;
			else
				state_actual_SET = 5;
			break;
	}
	return state_actual_SET;
}

/* ============================ */
/* ===== State Machine UA ===== */
/* ============================ */

static int state_actual_UA_TX = 5;
static int state_actual_UA_RX = 5;

int state_machine_UA_TX(unsigned char * buffer, size_t buffer_pos)
{
	switch (state_actual_UA_TX)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_UA_TX;
			else
				state_actual_UA_TX = 5;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_UA_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_TX = 4;
			else
				state_actual_UA_TX = 5;
			break;
		case 3: // Checking C_UA
			if (buffer[buffer_pos] == C_UA)
				--state_actual_UA_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_TX = 4;
			else
				state_actual_UA_TX = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_UA_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_TX = 4;
			else
				state_actual_UA_TX = 5;
			break;
	}
	return state_actual_UA_TX;
}

int state_machine_UA_RX(unsigned char * buffer, size_t buffer_pos)
{
	switch (state_actual_UA_RX)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_UA_RX;
			else
				state_actual_UA_RX = 5;
			break;
		case 4: // Checking A_RX
			if (buffer[buffer_pos] == A_RX)
				--state_actual_UA_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_RX = 4;
			else
				state_actual_UA_RX = 5;
			break;
		case 3: // Checking C_UA
			if (buffer[buffer_pos] == C_UA)
				--state_actual_UA_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_RX = 4;
			else
				state_actual_UA_RX = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_UA_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_UA_RX = 4;
			else
				state_actual_UA_RX = 5;
			break;
	}
	return state_actual_UA_RX;
}

/* =========================== */
/* ===== State Machine D ===== */
/* =========================== */

static int state_actual_D = 5;
static unsigned char BCC_D = 0x00;
static int potential_data = 2; // At least two bytes required to be data (one data byte and BCC_D)

int state_machine_D(unsigned char * buffer, size_t buffer_pos, unsigned char SN_s)
{
	switch (state_actual_D)
	{
		case 5: // Checking F (This case will probably never happen here)
			if (buffer[buffer_pos] == F)
				--state_actual_D;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_D;
			else if (buffer[buffer_pos] == F)
				state_actual_D = 4;
			else
				state_actual_D = 5;
			break;
		case 3: // Checking C_D ^ SN_s
			if (buffer[buffer_pos] == (C_D ^ SN_s))
				--state_actual_D;
			else if (buffer[buffer_pos] == F)
				state_actual_D = 4;
			else
				state_actual_D = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_D;
			else if (buffer[buffer_pos] == F)
				state_actual_D = 4;
			else
				state_actual_D = 5;
			break;
		case 1: // Checking BCC_D
			BCC_D ^= buffer[buffer_pos];
			if (potential_data)
				--potential_data;
			break;
	}
	return state_actual_D;
}

int state_machine_D_F()
{
	switch (state_actual_D)
	{
		case 5: // Checking F
			--state_actual_D;
			break;
		case 4: // Checking A_TX
		case 3: // Checking C_D ^ SN_s
		case 2: // Checking BCC
			state_actual_D = 4;
			break;
		case 1: // Checking BCC_D
			if (BCC_D == 0x00 && !potential_data)
				--state_actual_D;
			else
				state_actual_D = -1;
			break;
	}
	return state_actual_D;
}

/* ============================ */
/* ===== State Machine RR ===== */
/* ============================ */

static int state_actual_RR = 5;

int state_machine_RR(unsigned char * buffer, size_t buffer_pos, unsigned char RN_r)
{
	switch (state_actual_RR)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_RR;
			else
				state_actual_RR = 5;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_RR;
			else if (buffer[buffer_pos] == F)
				state_actual_RR = 4;
			else
				state_actual_RR = 5;
			break;
		case 3: // Checking C_RR ^ RN_r
			if (buffer[buffer_pos] == (C_RR ^ RN_r))
				--state_actual_RR;
			else if (buffer[buffer_pos] == F)
				state_actual_RR = 4;
			else
				state_actual_RR = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_RR;
			else if (buffer[buffer_pos] == F)
				state_actual_RR = 4;
			else
				state_actual_RR = 5;
			break;
	}
	return state_actual_RR;
}

/* ============================= */
/* ===== State Machine REJ ===== */
/* ============================= */

static int state_actual_REJ = 5;

int state_machine_REJ(unsigned char * buffer, size_t buffer_pos, unsigned char RN_r)
{
	switch (state_actual_REJ)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_REJ;
			else
				state_actual_REJ = 5;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_REJ;
			else if (buffer[buffer_pos] == F)
				state_actual_REJ = 4;
			else
				state_actual_REJ = 5;
			break;
		case 3: // Checking C_REJ ^ RN_r
			if (buffer[buffer_pos] == (C_REJ ^ RN_r))
				--state_actual_REJ;
			else if (buffer[buffer_pos] == F)
				state_actual_REJ = 4;
			else
				state_actual_REJ = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_REJ;
			else if (buffer[buffer_pos] == F)
				state_actual_REJ = 4;
			else
				state_actual_REJ = 5;
			break;
	}
	return state_actual_REJ;
}

/* ============================== */
/* ===== State Machine DISC ===== */
/* ============================== */

static int state_actual_DISC_TX = 5;
static int state_actual_DISC_RX = 5;

int state_machine_DISC_TX(unsigned char * buffer, size_t buffer_pos)
{
	switch (state_actual_DISC_TX)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_DISC_TX;
			else
				state_actual_DISC_TX = 5;
			break;
		case 4: // Checking A_TX
			if (buffer[buffer_pos] == A_TX)
				--state_actual_DISC_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_TX = 4;
			else
				state_actual_DISC_TX = 5;
			break;
		case 3: // Checking C_DISC
			if (buffer[buffer_pos] == C_DISC)
				--state_actual_DISC_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_TX = 4;
			else
				state_actual_DISC_TX = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_DISC_TX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_TX = 4;
			else
				state_actual_DISC_TX = 5;
			break;
	}
	return state_actual_DISC_TX;
}

int state_machine_DISC_RX(unsigned char * buffer, size_t buffer_pos)
{
	switch (state_actual_DISC_RX)
	{
		case 5:
		case 1: // Checking F
			if (buffer[buffer_pos] == F)
				--state_actual_DISC_RX;
			else
				state_actual_DISC_RX = 5;
			break;
		case 4: // Checking A_RX
			if (buffer[buffer_pos] == A_RX)
				--state_actual_DISC_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_RX = 4;
			else
				state_actual_DISC_RX = 5;
			break;
		case 3: // Checking C_DISC
			if (buffer[buffer_pos] == C_DISC)
				--state_actual_DISC_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_RX = 4;
			else
				state_actual_DISC_RX = 5;
			break;
		case 2: // Checking BCC
			if (buffer[buffer_pos] == (buffer[buffer_pos - 2] ^ buffer[buffer_pos - 1]))
				--state_actual_DISC_RX;
			else if (buffer[buffer_pos] == F)
				state_actual_DISC_RX = 4;
			else
				state_actual_DISC_RX = 5;
			break;
	}
	return state_actual_DISC_RX;
}

/* =============================== */
/* ===== State Machine Clear ===== */
/* =============================== */

void state_machine_clear()
{
	state_actual_SET = 5;
	state_actual_UA_TX = 5;
	state_actual_UA_RX = 5;
	state_actual_D = 5;
	state_actual_RR = 5;
	state_actual_REJ = 5;
	state_actual_DISC_TX = 5;
	state_actual_DISC_RX = 5;
	BCC_D = 0x00;
	potential_data = 2;
}
