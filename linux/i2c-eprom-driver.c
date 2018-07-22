#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "zzz-i2c-eprom"
#define CLASS_NAME "zzz"

#define MEM_SIZE 16
#define I2C_ADDR 0x10

#define assert(x)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Markus Lavin (https://www.zzzconsulting.se)");
MODULE_DESCRIPTION("Device driver for the I2C EPROM tutorial posts from https://www.zzzconsulting.se/");
MODULE_VERSION("0.1");

static int majorNumber;

static struct class *zzzClass  = NULL;
static struct device *zzzDevice = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static void __iomem *io_base = NULL;

static void axi_master_write(uint32_t address, uint32_t data)
{
	writel(data, io_base + address);
}

static uint32_t axi_master_read(uint32_t address)
{
	return readl(io_base + address);
}

static const uint32_t i2c_ctrl_addr = 0x00c;
static const uint32_t i2c_status_addr = 0x010;

static const uint32_t i2c_ctrl_we_bit = 1 << 10;
static const uint32_t i2c_ctrl_start_bit = 1 << 9;
static const uint32_t i2c_ctrl_stop_bit = 1 << 8;

static const uint32_t i2c_status_busy_bit = 1 << 9;
static const uint32_t i2c_status_ack_bit = 1 << 8;

static void i2c_mem_write(uint8_t i2c_addr, uint8_t mem_addr, uint8_t mem_data)
{
	uint32_t status;
	/* Make sure interface is not busy */
	while (axi_master_read(i2c_status_addr) & i2c_status_busy_bit) schedule();

	/* Address for write mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 0 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "I2C address ACK");

	/* Memory address */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "MEM address ACK");

	/* Memory data */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_stop_bit | mem_data);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "MEM write ACK");
}

static uint8_t i2c_mem_read(uint8_t i2c_addr, uint8_t mem_addr)
{
	uint32_t status;
	/* Make sure interface is not busy */
	while (axi_master_read(i2c_status_addr) & i2c_status_busy_bit) schedule();

	/* Address for write mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 0 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "I2C (write) address ACK");

	/* Memory address */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "MEM address ACK");

	/* Address for read mode */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | i2c_addr << 1 | 1 << 0);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "I2C (read) address ACK");

	/* Memory data */
	axi_master_write(i2c_ctrl_addr, i2c_ctrl_stop_bit);

	/* Wait until complete */
	while ((status = axi_master_read(i2c_status_addr)) & i2c_status_busy_bit) schedule();
	assert(status & i2c_status_ack_bit && "MEM read ACK");

	return status & 0xff;
}


static int __init zzz_init(void)
{
	io_base = ioremap(0x1e00b000, SZ_4K);

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0){
		return majorNumber;
	}

	zzzClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(zzzClass)){
		unregister_chrdev(majorNumber, DEVICE_NAME);
		return PTR_ERR(zzzClass);
	}

	zzzDevice = device_create(zzzClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(zzzDevice)){
		class_destroy(zzzClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		return PTR_ERR(zzzDevice);
	}

	return 0;
}

static void __exit zzz_exit(void)
{
	device_destroy(zzzClass, MKDEV(majorNumber, 0));
	class_unregister(zzzClass);
	class_destroy(zzzClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	char message[MEM_SIZE];
	int i;

	len = min(len, (size_t)(MEM_SIZE - *offset));

	for (i = 0; i < len; ++i)
		message[i] = i2c_mem_read(I2C_ADDR, *offset + i);

	len = len - copy_to_user(buffer, message, len);

	*offset += len;

	return len;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	char message[MEM_SIZE];
	int i;

	len = min(len, (size_t)(MEM_SIZE - *offset));
	len = len - copy_from_user(message, buffer, len);

	for (i = 0; i < len; ++i)
		i2c_mem_write(I2C_ADDR, *offset + i, message[i]);

	*offset += len;

	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	return 0;
}

module_init(zzz_init);
module_exit(zzz_exit);
