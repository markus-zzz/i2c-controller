#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include "axi_master.h"

static int axi_master_socket_sync;
static int axi_master_socket_async;

void axi_master_write(uint32_t address, uint32_t data)
{
	struct axi_master_msg msg;
	msg.code = MSG_CODE_WRITE_CMD;
	msg.address = address;
	msg.data = data;

	if (send(axi_master_socket_sync, &msg, sizeof(msg), 0) == -1) {
		perror("send");
		exit(1);
	}

	if (recv(axi_master_socket_sync, &msg, sizeof(msg), 0) <= 0) {
		perror("recv");
		exit(1);
	}

	assert(msg.code == MSG_CODE_WRITE_ACK);
}

uint32_t axi_master_read(uint32_t address)
{
	struct axi_master_msg msg;
	msg.code = MSG_CODE_READ_CMD;
	msg.address = address;
	msg.data = 0;

	if (send(axi_master_socket_sync, &msg, sizeof(msg), 0) == -1) {
		perror("send");
		exit(1);
	}

	if (recv(axi_master_socket_sync, &msg, sizeof(msg), 0) <= 0) {
		perror("recv");
		exit(1);
	}

	assert(msg.code == MSG_CODE_READ_ACK);
	return msg.data;
}

const uint32_t i2c_ctrl_addr = 0x0000100c;
const uint32_t i2c_status_addr = 0x00001010;

const uint32_t i2c_ctrl_we_bit = 1 << 10;
const uint32_t i2c_ctrl_start_bit = 1 << 9;
const uint32_t i2c_ctrl_stop_bit = 1 << 8;

const uint32_t i2c_status_busy_bit = 1 << 9;
const uint32_t i2c_status_ack_bit = 1 << 8;

void i2c_mem_write(uint8_t i2c_addr, uint8_t mem_addr, uint8_t mem_data)
{
	uint32_t status;
	/* Make sure interface is not busy */
	while (axi_master_read(i2c_status_addr) & i2c_status_busy_bit);

	/* Address for write mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 0 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "I2C address ACK");

	/* Memory address */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "MEM address ACK");

	/* Memory data */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_stop_bit | mem_data);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "MEM write ACK");
}

uint8_t i2c_mem_read(uint8_t i2c_addr, uint8_t mem_addr)
{
	uint32_t status;
	/* Make sure interface is not busy */
	while (axi_master_read(i2c_status_addr) & i2c_status_busy_bit);

	/* Address for write mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 0 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "I2C (write) address ACK");

	/* Memory address */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "MEM address ACK");

	/* Address for read mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 1 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "I2C (read) address ACK");

	/* Memory data */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_stop_bit);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit);
	assert(status & i2c_status_ack_bit && "MEM read ACK");

	return status & 0xff;
}

int main(void)
{
    struct sockaddr_un remote;

    if ((axi_master_socket_sync = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    if ((axi_master_socket_async = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    printf("Trying to connect...\n");

	remote.sun_family = AF_UNIX;
	snprintf(remote.sun_path, 104, "%s.%s", SOCK_PATH, "sync");
	if (connect(axi_master_socket_sync, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		perror("connect sync");
		exit(1);
	}
	snprintf(remote.sun_path, 104, "%s.%s", SOCK_PATH, "async");
	if (connect(axi_master_socket_async, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		perror("connect async");
		exit(1);
	}

    printf("Connected.\n");

	/* begin - test */

	/* Test a few AXI registers */
	axi_master_write(0x00001000, 0xcafebabe);
	axi_master_write(0x00001004, 0xdeadbeef);
	axi_master_write(0x00001008, 0xf00baaaa);

	printf("data: %x\n", axi_master_read(0x00001004));
	printf("data: %x\n", axi_master_read(0x00001000));
	printf("data: %x\n", axi_master_read(0x00001008));

	/* I2C slave model has address 7'b001_0000 */
#define I2C_ADDR 0x10
#define DATA_SIZE 16
	uint8_t data[DATA_SIZE];

	/* Generate reference data */
	for (int i = 0; i < DATA_SIZE; i++) {
		data[i] = rand();
	}

	/* Write data to memory in forward order */
	for (int i = 0; i < DATA_SIZE; i++) {
		i2c_mem_write(I2C_ADDR, i, data[i]);
	}

	/* Read data from memory (and verify) in forward order */
	for (int i = 0; i < DATA_SIZE; i++) {
		assert(i2c_mem_read(I2C_ADDR, i) == data[i]);
	}

	/* Read data from memory (and verify) in reverse order */
	for (int i = 0; i < DATA_SIZE; i++) {
		assert(i2c_mem_read(I2C_ADDR, DATA_SIZE - 1 - i) == data[DATA_SIZE - 1 - i]);
	}

	/* end - test */

    close(axi_master_socket_sync);
    close(axi_master_socket_async);

    return 0;
}
