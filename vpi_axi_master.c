#include <assert.h>
#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <vpi_user.h>
#include "axi_master.h"

struct axi_values {
#define DEF_SIGNAL(x,y)	vpiHandle x##_h;
#include "signals.def"
#undef DEF_SIGNAL

#define DEF_SIGNAL(x,y)	s_vpi_value x;
#include "signals.def"
#undef DEF_SIGNAL
} axi_signals;

void signals_init()
{
#define DEF_SIGNAL(x,y) \
do { \
	axi_signals.x##_h = vpi_handle_by_name("tb." #x, NULL); \
} while (0);
#include "signals.def"
#undef DEF_SIGNAL
}

void signals_read()
{
#define DEF_SIGNAL(x,y) \
do { \
	axi_signals.x.format = vpiIntVal; \
	vpi_get_value(axi_signals.x##_h, &axi_signals.x); \
} while (0);
#include "signals.def"
#undef DEF_SIGNAL
}

void signals_write()
{
#define DEF_SIGNAL(x,y) \
do { \
	if (y) { \
		/* vpiInertialDelay - All scheduled events on the object shall be removed before this event is scheduled. */ \
		s_vpi_time when; \
		when.type = vpiSimTime; \
		when.high = 0; \
		when.low = 0; \
		when.real = 0; \
		vpi_put_value(axi_signals.x##_h, &axi_signals.x, &when, vpiInertialDelay); \
	} \
} while (0);
#include "signals.def"
#undef DEF_SIGNAL
}

int clock_request()
{
	return axi_signals.busy_bit.value.integer;
}

static int axi_master_sync_socket;
static int axi_master_async_socket;

static enum {s_idle, s_w_0, s_w_1, s_w_2, s_r_0, s_r_1, s_r_2} state = s_idle;
static struct axi_master_msg msg;

static uint32_t irq_level, irq_level_prev = 0;

int clk_cb(p_cb_data cb)
{
	signals_read();

	/* @posedge(axi_aclk) and inactive axi_aresetn */
	if (axi_signals.axi_aresetn.value.integer && axi_signals.axi_aclk.value.integer) {

		int res;

		int recv_flags = 0;
		if (clock_request()) {
			recv_flags = MSG_DONTWAIT;
		}

		irq_level = axi_signals.busy_bit.value.integer ? 0 : 1;
		if (irq_level != irq_level_prev) {
			/* Dont block and dont care if it fails (e.g. nobody is recving) */
			(void)send(axi_master_async_socket, &irq_level, sizeof(irq_level), MSG_DONTWAIT);
			irq_level_prev = irq_level;
		}

		switch (state) {
			case s_idle:
#if 0
				printf("about to recv() with recv_flags: %x\n", recv_flags);
#endif
				if ((res = recv(axi_master_sync_socket, &msg, sizeof(msg), recv_flags)) != sizeof(msg)) {
					if (res == -1 && (EAGAIN == errno || EWOULDBLOCK == errno)) {
						return 0;
					}
					else if (0 == res) {
						vpi_printf("socket closed.\n");
						exit(0);
					}
					else {
						perror("recv");
						exit(1);
					}
				}
				if (msg.code == MSG_CODE_WRITE_CMD) {
					state = s_w_0;
				}
				else {
					assert(msg.code == MSG_CODE_READ_CMD);
					state = s_r_0;
				}
				break;

			case s_w_0:
				axi_signals.axi_awvalid.value.integer = 1;
				axi_signals.axi_awaddr.value.integer = msg.address;

				axi_signals.axi_wvalid.value.integer = 1;
				axi_signals.axi_wstrb.value.integer = 0xf;
				axi_signals.axi_wdata.value.integer = msg.data;

				state = s_w_1;
				break;

			case s_w_1:
				if (axi_signals.axi_awready.value.integer && axi_signals.axi_wready.value.integer) {
					axi_signals.axi_awvalid.value.integer = 0;
					axi_signals.axi_wvalid.value.integer = 0;
					axi_signals.axi_bready.value.integer = 1;
					state = s_w_2;
				}
				break;

			case s_w_2:
				if (axi_signals.axi_bvalid.value.integer) {
					axi_signals.axi_bready.value.integer = 0;

					msg.code = MSG_CODE_WRITE_ACK;
					if (send(axi_master_sync_socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
						perror("send");
						exit(1);
					}
					state = s_idle;
				}
				break;

			case s_r_0:
				axi_signals.axi_arvalid.value.integer = 1;
				axi_signals.axi_araddr.value.integer = msg.address;

				state = s_r_1;
				break;

			case s_r_1:
				if (axi_signals.axi_arready.value.integer) {
					axi_signals.axi_arvalid.value.integer = 0;
					axi_signals.axi_rready.value.integer = 1;

					state = s_r_2;
				}
				break;

			case s_r_2:
				if (axi_signals.axi_rvalid.value.integer) {
					axi_signals.axi_rready.value.integer = 0;

					msg.data = axi_signals.axi_rdata.value.integer;
					msg.code = MSG_CODE_READ_ACK;
					if (send(axi_master_sync_socket, &msg, sizeof(msg), 0) != sizeof(msg)) {
						perror("send");
						exit(1);
					}
					state = s_idle;
				}
				break;
		}
	}

	signals_write();

	return 0;
}

void wait_for_axi_master_client(void)
{
	int sync_listen_socket;
	int async_listen_socket;
	unsigned t;
	struct sockaddr_un local, remote;

	if ((sync_listen_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket sync");
		exit(1);
	}
	if ((async_listen_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket async");
		exit(1);
	}

	local.sun_family = AF_UNIX;
	snprintf(local.sun_path, 104, "%s.%s", SOCK_PATH, "sync");
	unlink(local.sun_path);
	if (bind(sync_listen_socket, (struct sockaddr *)&local, sizeof(local)) == -1) {
		perror("bind");
		exit(1);
	}
	snprintf(local.sun_path, 104, "%s.%s", SOCK_PATH, "async");
	unlink(local.sun_path);
	if (bind(async_listen_socket, (struct sockaddr *)&local, sizeof(local)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sync_listen_socket, 5) == -1) {
		perror("listen");
		exit(1);
	}
	if (listen(async_listen_socket, 5) == -1) {
		perror("listen");
		exit(1);
	}

	printf("Waiting for a connection...\n");
	t = sizeof(remote);
	if ((axi_master_sync_socket = accept(sync_listen_socket, (struct sockaddr *)&remote, &t)) == -1) {
		perror("accept");
		exit(1);
	}
	if ((axi_master_async_socket = accept(async_listen_socket, (struct sockaddr *)&remote, &t)) == -1) {
		perror("accept");
		exit(1);
	}

	printf("Connected.\n");
}

int start_of_sim_cb(p_cb_data unused)
{
	vpiHandle clk;
	p_cb_data cb = malloc(sizeof(s_cb_data));

	clk = vpi_handle_by_name("tb.axi_aclk", NULL);

	cb->reason = cbValueChange;
	cb->cb_rtn = clk_cb;
	cb->obj = clk;
	cb->time = (p_vpi_time)malloc(sizeof(s_vpi_time));
	cb->time->type = vpiSuppressTime;
	cb->value = (p_vpi_value)malloc(sizeof(s_vpi_value));
	cb->value->format = vpiScalarVal;
	cb->user_data = NULL;

	vpi_register_cb(cb);

	signals_init();

	wait_for_axi_master_client();

	return 0;
}

void op_tester_register()
{
	p_cb_data cb = malloc(sizeof(s_cb_data));

	cb->reason = cbStartOfSimulation;
	cb->cb_rtn = start_of_sim_cb;
	cb->obj = NULL;
	cb->time = NULL;
	cb->value = NULL;
	cb->user_data = NULL;

	vpi_register_cb(cb);
}

void (*vlog_startup_routines[])() = {
	op_tester_register,
	NULL
};
