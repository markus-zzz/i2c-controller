#pragma once

#include <stdint.h>

#define SOCK_PATH "/tmp/axi_master_socket"

struct axi_master_msg {
	enum {MSG_CODE_WRITE_CMD = 1, MSG_CODE_WRITE_ACK = 2, MSG_CODE_READ_CMD = 3, MSG_CODE_READ_ACK = 4} code;
	uint32_t address;
	uint32_t data;
};

