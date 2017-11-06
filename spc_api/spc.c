#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "spc.h"
#include "spcsm.h"

#define BAUDRATE B115200 // subject to change

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
	C_REJ								= 0x01,

	RN_0								= 0x00,
	RN_1								= 0x80
};

enum data_frame_bytes_t
{
	C_D									= 0x00,

	SN_0								= 0x00,
	SN_1								= 0x40
};

enum stuffed_frame_bytes_t
{
	ESCAPED_F							= 0x7D,
	F_STUFFING							= 0x5E,
	ESCAPED_F_STUFFING					= 0x5D
};

enum frame_state_t
{
	INVALID_FRAME						= -1,
	VALID_FRAME							= 0,
	VALIDATING_FRAME					= 5 // For initialization only
};

enum
{
	IDLE_TIMEOUT						= 60,
	SNOOZE								= 0,
	
	BUFFER_SIZE							= 1024,

	NO_CREATE							= 0,
	CREATE								= 1
};

/* ========================== */
/* ===== Signal Handler ===== */
/* ========================== */

static void timeout_handler(int signal)
{
	fprintf(stderr, "Timeout!\n");
	(void) signal;
}

static int install_handler()
{
	struct sigaction sigact;
	memset(&sigact, 0, sizeof sigact);
	sigact.sa_handler = &timeout_handler;
	return sigaction(SIGALRM, &sigact, NULL);
}

/* ============================= */
/* ===== Buffer Definition ===== */
/* ============================= */

static unsigned char buffer[BUFFER_SIZE];
static size_t buffer_pos = 0;

/* ================================ */
/* ===== Instances Definition ===== */
/* ================================ */

static lldefs_t ** llinstances = NULL;
static size_t llcount = 0;

void llterminate()
{
	if (llinstances == NULL)
	{
		size_t index;
		for (index = 0; index < llcount; ++index)
		{
			free(llinstances[index]);
		}
		free(llinstances);
	}
}

static int llinitialize()
{
	if (install_handler() == -1)
	{
		fprintf(stderr, "Error on handler installation!\n");
		return -12;
	}
	if (atexit(llterminate) != 0)
	{
		fprintf(stderr, "Error on function registration!\n");
		return -11;
	}
	return 0;
}

/* =========================== */
/* ===== Private Methods ===== */
/* =========================== */

static lldefs_t * llget(int fd, int create)
{
	size_t index;
	for (index = 0; index < llcount; ++index)
	{
		if (llinstances[index]->fd == fd)
		{
			return llinstances[index];
		}
	}

	if (create)
	{
		lldefs_t instance = { fd, { 0 }, 3, 3, 0 };
		if (llcreate(&instance) != 0)
		{
			fprintf(stderr, "Error creating struct!\n");
			return NULL;
		}
		return llinstances[index]; // Pretty clever
	}

	fprintf(stderr, "Error finding struct!\n");
	return NULL;
}

static void llwrite_ctrl(int fd, unsigned char address_field, unsigned char control_field)
{
	memset(buffer, 0, BUFFER_SIZE);
	tcflush(fd, TCOFLUSH);

	buffer[0] = F;
	buffer[1] = address_field;
	buffer[2] = control_field;
	buffer[3] = buffer[1] ^ buffer[2];
	buffer[4] = F;

	write(fd, buffer, 5);
}

static int llread_ctrl(int fd, unsigned timeout_seconds, int(* state_machine)(unsigned char *, size_t))
{
	errno = buffer_pos = 0;
	memset(buffer, 0, BUFFER_SIZE);

	int state = VALIDATING_FRAME;
	state_machine_clear();

	alarm(timeout_seconds);
	do
	{
		read(fd, &buffer[buffer_pos], 1);

		if (errno == EINTR)
			break;

		state = state_machine(buffer, buffer_pos++);
	}
	while (state != VALID_FRAME);
	alarm(SNOOZE);

	return state;
}
// to do error messages from here
static int llopen_tx(int fd)
{
	lldefs_t * instance;
	if ((instance = llget(fd, CREATE)) == NULL)
	{
		return -3; // Error getting / creating struct
	}

	unsigned retry_count = 0;
	do
	{
		// Write SET

		llwrite_ctrl(fd, A_TX, C_SET);

		// Read UA

		if (llread_ctrl(fd, instance->timeout_seconds, &state_machine_UA_TX))
			++retry_count;
		else
			break;
	}
	while (retry_count < instance->retry_limit);

	if (retry_count == instance->retry_limit)
	{
		return -5; // Reached limit of retries
	}

	return 0; // Error code must be < -1
}

static int llopen_rx(int fd)
{
	lldefs_t * instance;
	if ((instance = llget(fd, CREATE)) == NULL)
	{
		return -3; // Error getting / creating struct
	}

	unsigned retry_count = 0;
	do
	{
		// Read SET

		if (llread_ctrl(fd, instance->timeout_seconds, &state_machine_SET))
			++retry_count;
		else
			break;
	}
	while (retry_count < instance->retry_limit);

	if (retry_count == instance->retry_limit)
	{
		return -5; // Reached limit of retries
	}

	// Write UA

	llwrite_ctrl(fd, A_TX, C_UA);

	return 0; // Error code must be < -2
}

static void llprocess_tx(unsigned char * buf, size_t len, unsigned char * buf_pro, size_t * len_pro, int sn_field)
{
	size_t index = 0;
	size_t offset = 4;

	buf_pro[0] = F;

	buf_pro[1] = A_TX;
	buf_pro[2] = C_D ^ sn_field;
	buf_pro[3] = buf_pro[2] ^ buf_pro[1];

	unsigned char BCC_D = 0x00;
	for (; index < len; ++index)
	{
		BCC_D ^= buf[index];
		switch (buf[index])
		{
			case F:
				buf_pro[index + offset] = ESCAPED_F;
				++offset;
				buf_pro[index + offset] = F_STUFFING;
				break;
			case ESCAPED_F:
				buf_pro[index + offset] = ESCAPED_F;
				++offset;
				buf_pro[index + offset] = ESCAPED_F_STUFFING;
				break;
			default:
				buf_pro[index + offset] = buf[index];
				break;
		}
	}
	switch (BCC_D)
	{
		case F:
			buf_pro[index + offset] = ESCAPED_F;
			++offset;
			buf_pro[index + offset] = F_STUFFING;
			break;
		case ESCAPED_F:
			buf_pro[index + offset] = ESCAPED_F;
			++offset;
			buf_pro[index + offset] = ESCAPED_F_STUFFING;
			break;
		default:
			buf_pro[index + offset] = BCC_D;
			break;
	}

	*len_pro = index + offset + 1;
	buf_pro[(*len_pro)++] = F;
}

static int llprocess_rx(unsigned char * buf_pro, size_t len_pro, int escaped_byte)
{
	switch (buffer[buffer_pos])
	{
		case F:
			buf_pro[len_pro] = buffer[buffer_pos];
			return -1;
		case ESCAPED_F:
			return 1;
		case F_STUFFING:
			if (escaped_byte)
			{
				buf_pro[len_pro] = F;
				return 0;
			}
			goto case_default;
		case ESCAPED_F_STUFFING:
			if (escaped_byte)
			{
				buf_pro[len_pro] = ESCAPED_F;
				return 0;
			}
			goto case_default;
		default:
		case_default:
			buf_pro[len_pro] = buffer[buffer_pos];
			break;
	}
	return 0;
}

static int llclose_tx(int fd)
{
	lldefs_t * instance;
	if ((instance = llget(fd, NO_CREATE)) == NULL)
	{
		return -3; // Error getting struct
	}

	unsigned retry_count = 0;
	do
	{
		// Read DISC

		if (llread_ctrl(fd, instance->timeout_seconds, &state_machine_DISC_RX))
			++retry_count;
		else
			break;
	}
	while (retry_count < instance->retry_limit);

	if (retry_count == instance->retry_limit)
	{
		return -5; // Reached limit of retries
	}

	// Write UA

	llwrite_ctrl(fd, A_RX, C_UA);

	sleep(1);

	return lldestroy(fd); // Error destroying struct
}

static int llclose_rx(int fd)
{
	lldefs_t * instance;
	if ((instance = llget(fd, NO_CREATE)) == NULL)
	{
		return -3; // Error getting struct
	}

	unsigned retry_count = 0;
	do
	{
		// Write DISC

		llwrite_ctrl(fd, A_RX, C_DISC);

		// Read UA

		if (llread_ctrl(fd, instance->timeout_seconds, &state_machine_UA_RX))
			++retry_count;
		else
			break;
	}
	while (retry_count < instance->retry_limit);

	if (retry_count == instance->retry_limit)
	{
		return -5; // Reached limit of retries
	}

	return lldestroy(fd); // Error destroying struct
}

/* ========================== */
/* ===== Public Methods ===== */
/* ========================== */

int llcreate(lldefs_t * lldef)
{
	int error;
	if (llinstances == NULL && (error = llinitialize()))
	{
		return error; // Error on initialization
	}

	if (lldef->fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	size_t index;
	for (index = 0; index < llcount; ++index)
	{
		if (llinstances[index]->fd == lldef->fd)
		{
			return -1; // Conflicting file descriptor
		}
	}

	lldefs_t ** pptmp_instances;
	if ((pptmp_instances = realloc(llinstances, (llcount + 1) * sizeof *pptmp_instances)) == NULL)
	{
		return -22; // Error on allocation
	}
	llinstances = pptmp_instances;

	lldefs_t * ptmp_def;
	if ((ptmp_def = malloc(sizeof *ptmp_def)) == NULL)
	{
		return -22; // Error on allocation
	}
	llinstances[llcount++] = ptmp_def;

	struct termios tmp_st;
	if (tcgetattr(lldef->fd, &lldef->st_default_) == -1)
	{
		return -3; // Error on getting termios
	}
	lldef->n_actual_ = 0;

	memcpy(&tmp_st, &lldef->st_default_, sizeof tmp_st);
	// Check these values
	tmp_st.c_iflag = IGNPAR;
	tmp_st.c_oflag = 0;
	tmp_st.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	tmp_st.c_lflag = 0;
	tmp_st.c_cc[VTIME] = 0;
	tmp_st.c_cc[VMIN] = 1;

	if (tcsetattr(lldef->fd, TCSANOW, &tmp_st) == -1)
	{
		return -4; // Error on setting termios
	}

	memcpy(ptmp_def, lldef, sizeof *ptmp_def);

	return 0;
}

int lldestroy(int fd)
{
	if (fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	size_t index;
	for (index = 0; index < llcount; ++index)
	{
		if (llinstances[index]->fd == fd)
		{
			if (tcsetattr(fd, TCSANOW, &llinstances[index]->st_default_) == -1)
			{
				return -4; // Error on resetting termios
			}
			llinstances[index]->fd = -1;

			break;
		}
	}

	return 0;
}

int llopen(int fd, int flag)
{
	if (fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	switch (flag)
	{
		case TRANSMITTER:
			return llopen_tx(fd); // return < -2 on error;
		case RECEIVER:
			return llopen_rx(fd); // return < -2 on error;
	}

	return -2; // Error on flag
}

int llwrite(int fd, unsigned char * buf, size_t len)
{
	if (fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	lldefs_t * instance;
	if ((instance = llget(fd, NO_CREATE)) == NULL)
	{
		return -3; // Error getting struct
	}

	// Check if len > 0

	if (len == 0)
	{
		llwrite_ctrl(fd, A_TX, C_DISC);

		return 0;
	}

	// Process buf

	unsigned char buf_pro[BUFFER_SIZE];
	size_t len_pro = 0;

	llprocess_tx(buf, len, buf_pro, &len_pro, (instance->n_actual_ ? SN_1 : SN_0));

	unsigned retry_count = 0;
	do
	{
		// Write D

		tcflush(fd, TCOFLUSH);

		write(fd, buf_pro, len_pro);

        // Read RR or REJ

        errno = buffer_pos = 0;
		memset(buffer, 0, BUFFER_SIZE);

		int stateRR = VALIDATING_FRAME;
		int stateREJ = VALIDATING_FRAME;
        state_machine_clear();

		alarm(instance->timeout_seconds);
		do
		{
			read(fd, &buffer[buffer_pos], 1);

			if (errno == EINTR)
			{
				sleep(1);

				tcflush(fd, TCIFLUSH);

				break;
			}

			if (instance->n_actual_)
			{
				stateRR = state_machine_RR(buffer, buffer_pos, RN_0);
				stateREJ = state_machine_REJ(buffer, buffer_pos++, RN_0);
			}
			else
			{
				stateRR = state_machine_RR(buffer, buffer_pos, RN_1);
				stateREJ = state_machine_REJ(buffer, buffer_pos++, RN_1);
			}
		}
		while (stateRR != VALID_FRAME && stateREJ != VALID_FRAME);
		alarm(SNOOZE);

		if (stateRR == VALID_FRAME)
			break;
	}
	while (++retry_count < instance->retry_limit);

	if (retry_count == instance->retry_limit)
	{
		return -5; // Reached limit of retries
	}

	instance->n_actual_ = !instance->n_actual_;

	return len;
}

int llread(int fd, unsigned char * buf)
{
	if (fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	lldefs_t * instance;
	if ((instance = llget(fd, NO_CREATE)) == NULL)
	{
		return -3; // Error getting struct
	}

	unsigned char buf_pro[BUFFER_SIZE];
	size_t len_pro = 0;

	loop:
	{
		// Read D

		errno = buffer_pos = len_pro = 0;
		memset(buffer, 0, BUFFER_SIZE);
		memset(buf_pro, 0, BUFFER_SIZE);

		int escaped_byte = 0;
		int stateD = VALIDATING_FRAME;
		int stateDISC = VALIDATING_FRAME;
		state_machine_clear();

		do
		{
            alarm(IDLE_TIMEOUT);

			read(fd, &buffer[buffer_pos], 1);

			if (errno == EINTR)
            {
                return -6; // Idle timeout
            }

            alarm(SNOOZE);

			switch ((escaped_byte = llprocess_rx(buf_pro, len_pro, escaped_byte)))
			{
				case -1:
					escaped_byte = 0;
					stateD = state_machine_D_F();
					++len_pro;
					break;
				case 0:
					if (instance->n_actual_)
						stateD = state_machine_D(buf_pro, len_pro++, SN_1);
					else
						stateD = state_machine_D(buf_pro, len_pro++, SN_0);
				case 1:
					break;
			}
			stateDISC = state_machine_DISC_TX(buffer, buffer_pos++); // Not an error, prefer to send the stuffed data so bytes aren't misinterpreted
		}
		while (stateD != VALID_FRAME && stateD != INVALID_FRAME && stateDISC != VALID_FRAME);

		if (stateDISC == VALID_FRAME)
		{
			return 0;
		}

		if (stateD == VALID_FRAME)
		{
			// Write RR

			llwrite_ctrl(fd, A_TX, C_RR ^ (instance->n_actual_ ? RN_0 : RN_1));

			goto end_loop;
		}
		else
		{
			// Write REJ

			sleep(1);

			tcflush(fd, TCIFLUSH);

			llwrite_ctrl(fd, A_TX, C_REJ ^ (instance->n_actual_ ? RN_0 : RN_1));

			goto loop;
		}
	}
	end_loop:

	memcpy(buf, (buf_pro + 4), (len_pro - 6));

	instance->n_actual_ = !instance->n_actual_;

	return (len_pro - 6);
}

int llclose(int fd, int flag)
{
	if (fd < 0)
	{
		return -1; // Invalid file descriptor
	}

	switch (flag)
	{
		case TRANSMITTER:
			return llclose_tx(fd); // return < -2 on error;
		case RECEIVER:
			return llclose_rx(fd); // return < -2 on error;
	}

	return -2; // Error on flag
}

// ============ NOTE ============
// The sleep() function calls are necessary to solve a bug
// with the Linux Kernel where tcflush() won't work correctly
// ========== END NOTE ==========
