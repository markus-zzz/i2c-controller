#pragma once

#include <stdint.h>

#define SOCK_PATH "axi_master_socket"

struct axi_master_msg {
	enum {MSG_CODE_WRITE_CMD, MSG_CODE_WRITE_ACK, MSG_CODE_READ_CMD, MSG_CODE_READ_ACK} code;
	uint32_t address;
	uint32_t data;
};

