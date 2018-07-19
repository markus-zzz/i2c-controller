#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"

#define TYPE_AXI_MASTER_CLIENT_DEVICE "axi_master_client_device"
#define AXI_MASTER_CLIENT_DEVICE(obj) OBJECT_CHECK(AxiMasterClientDeviceState, (obj), TYPE_AXI_MASTER_CLIENT_DEVICE)

#define SOCK_PATH "/tmp/axi_master_socket"

struct axi_master_msg {
	enum {MSG_CODE_WRITE_CMD = 1, MSG_CODE_WRITE_ACK = 2, MSG_CODE_READ_CMD = 3, MSG_CODE_READ_ACK = 4} code;
	uint32_t address;
	uint32_t data;
};

typedef struct AxiMasterClientDeviceState {
	SysBusDevice parent_obj;
	MemoryRegion iomem;
	qemu_irq irq;
	int sock_sync_fd;
	int sock_async_fd;
	uint32_t base_address;
} AxiMasterClientDeviceState;

static uint64_t
axi_master_client_device_read(void *opaque, hwaddr offset, unsigned size)
{
	AxiMasterClientDeviceState *s = (AxiMasterClientDeviceState *)opaque;

	printf("axi_master_client_device_read offset=%llx size=%llx\n",
	       (unsigned long long)offset, (unsigned long long)size);

	struct axi_master_msg msg;
	msg.code = MSG_CODE_READ_CMD;
	msg.address = s->base_address + offset;
	msg.data = 0;

	if (send(s->sock_sync_fd, &msg, sizeof(msg), 0) == -1) {
		perror("send");
		exit(1);
	}

	if (recv(s->sock_sync_fd, &msg, sizeof(msg), 0) <= 0) {
		perror("recv");
		exit(1);
	}

	assert(msg.code == MSG_CODE_READ_ACK);
	return msg.data;
}

static void
axi_master_client_device_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
	AxiMasterClientDeviceState *s = (AxiMasterClientDeviceState *)opaque;

	printf("axi_master_client_device_write offset=%llx value=%llx size=%llx\n",
	       (unsigned long long)offset, (unsigned long long)value, (unsigned long long)size);

	struct axi_master_msg msg;
	msg.code = MSG_CODE_WRITE_CMD;
	msg.address = s->base_address + offset;
	msg.data = value;

	if (send(s->sock_sync_fd, &msg, sizeof(msg), 0) == -1) {
		perror("send");
		exit(1);
	}

	if (recv(s->sock_sync_fd, &msg, sizeof(msg), 0) <= 0) {
		perror("recv");
		exit(1);
	}

	assert(msg.code == MSG_CODE_WRITE_ACK);
}

static const MemoryRegionOps axi_master_client_device_ops = {
	.read = axi_master_client_device_read,
	.write = axi_master_client_device_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

static void *
async_thread(void *opaque)
{
	AxiMasterClientDeviceState *s = (AxiMasterClientDeviceState *)opaque;

	while (1) {
		uint32_t irq_level;
		if (recv(s->sock_async_fd, &irq_level, sizeof(irq_level), 0) <= 0) {
			perror("recv");
			exit(1);
		}

		printf("Got IRQ level : %d\n", irq_level);
		/* Need to acquire the 'Big QEMU Lock' before reporting IRQ to main thread */
		qemu_mutex_lock_iothread();
		qemu_set_irq(s->irq, irq_level);
		qemu_mutex_unlock_iothread();
	}

	return NULL;
}

static void
axi_master_client_device_init(Object *obj)
{
	AxiMasterClientDeviceState *s = AXI_MASTER_CLIENT_DEVICE(obj);
	SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

	memory_region_init_io(&s->iomem, obj, &axi_master_client_device_ops, s, "axi_master_client_device", 0x1000);
	sysbus_init_mmio(sbd, &s->iomem);
	sysbus_init_irq(sbd, &s->irq);

	struct sockaddr_un remote;

	if ((s->sock_sync_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	if ((s->sock_async_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	printf("Trying to connect...\n");

	remote.sun_family = AF_UNIX;
	snprintf(remote.sun_path, 104, "%s.%s", SOCK_PATH, "sync");
	if (connect(s->sock_sync_fd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		perror("connect sync");
		exit(1);
	}
	snprintf(remote.sun_path, 104, "%s.%s", SOCK_PATH, "async");
	if (connect(s->sock_async_fd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		perror("connect async");
		exit(1);
	}

	printf("Connected.\n");

    QemuThread thread;
	qemu_thread_create(&thread, "async_thread", async_thread, s, QEMU_THREAD_DETACHED);

	s->base_address = 0x1000;
}

static const TypeInfo axi_master_client_device_info = {
	.name          = TYPE_AXI_MASTER_CLIENT_DEVICE,
	.parent        = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(AxiMasterClientDeviceState),
	.instance_init = axi_master_client_device_init,
};

static void axi_master_client_device_register_types(void)
{
	type_register_static(&axi_master_client_device_info);
}

type_init(axi_master_client_device_register_types)
