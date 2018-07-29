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
/*
				13'h800: reg_data_out <= {tx_rp_i, 2'b00};
				13'h804: reg_data_out <= {tx_wp, 2'b00};
				13'h808: reg_data_out <= {rx_rp, 2'b00};
				13'h80c: reg_data_out <= {rx_wp_i, 2'b00};
 */

static const uint32_t tx_base_addr = 0x000;
static const uint32_t rx_base_addr = 0x400;

static const uint32_t tx_rp_addr = 0x800;
static const uint32_t tx_wp_addr = 0x804;
static const uint32_t rx_rp_addr = 0x808;
static const uint32_t rx_wp_addr = 0x80c;

void put_msg(const char *msg)
{
	uint32_t buf[1024];
	uint32_t msg_byte_len = strlen(msg);
	uint32_t msg_word_len = (msg_byte_len >> 2) + (msg_byte_len & 0x3 ? 1 : 0);
	strcpy((char*)buf, msg);

	uint32_t tx_wp = axi_master_read(tx_wp_addr);

	axi_master_write(tx_base_addr + tx_wp, msg_byte_len);
	for (int i = 0; i < msg_word_len; i++) {
		axi_master_write(tx_base_addr + tx_wp + (1 + i) * 4, buf[i]);
	}

	/* Advance TX_WP */
	axi_master_write(tx_wp_addr, tx_wp + (msg_word_len + 1) * 4);
}

void get_msg()
{
	uint32_t buf[1024];

	uint32_t rx_rp = axi_master_read(rx_rp_addr);

	uint32_t msg_byte_len = axi_master_read(rx_base_addr + rx_rp);
	uint32_t msg_word_len = (msg_byte_len >> 2) + (msg_byte_len & 0x3 ? 1 : 0);

	for (int i = 0; i < msg_word_len; i++) {
		buf[i] = axi_master_read(rx_base_addr + rx_rp + (1 + i) * 4);
	}

	/* Advance RX_RP */
	axi_master_write(rx_rp_addr, rx_rp + 1 + (msg_word_len) * 4);

	char *msg = (char*)&buf[0];
	msg[msg_byte_len] = '\0';
	printf("\nmsg: len=%d '%s'\n", msg_byte_len, msg);
}

void dump_rx()
{
	printf("\n\n\n");
	for (int i = 0; i < 32; i++) {
		uint32_t off = i * 4;
		printf("%08x: %08x\n", rx_base_addr + off,  axi_master_read(rx_base_addr + off));
	}
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
	put_msg("Hello World1");
	put_msg("Dummy you.");
#if 0
	put_msg("FooBar");
#endif

	dump_rx();
	dump_rx();
#if 0
	return 0;

	/* Busy wait until tx_rp == tx_wp */
	while (1) {
		uint32_t tx_rp = axi_master_read(0x800);
		uint32_t tx_wp = axi_master_read(0x804);
		if (tx_rp == tx_wp) {
			break;
		}
		printf(".");
	}
#endif

	get_msg();
#if 0
	get_msg();
	get_msg();
#endif

	/* end - test */

    close(axi_master_socket_sync);
    close(axi_master_socket_async);

    return 0;
}
