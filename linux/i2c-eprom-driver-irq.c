#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
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

static char read_message[MEM_SIZE];
static int read_idx;
static int read_len;

static char write_message[MEM_SIZE];
static int write_idx;
static int write_len;


wait_queue_head_t wq;

static int irq_num;

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

static enum {S_ILLEGAL, /* S_READ_0, */ S_READ_1, S_READ_2, S_READ_3, S_READ_4,
             S_WRITE_0, S_WRITE_1, S_WRITE_2, S_WRITE_3} state;

static irq_handler_t zzz_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	uint32_t status;
	uint8_t mem_addr;
	uint8_t mem_data = write_message[write_idx];

	/* Acknowledge interrupt */
	writel(0xffff, io_base + 0x20);

	status = axi_master_read(i2c_status_addr);

	if (status & i2c_status_busy_bit) {
		printk(KERN_ALERT "zzz-i2c-eprom: IRQ while busy");
		state = S_ILLEGAL;
		goto done_with_irq;
	}
	if (~status & i2c_status_ack_bit) {
		printk(KERN_ALERT "zzz-i2c-eprom: No ACK");
		state = S_ILLEGAL;
		goto done_with_irq;
	}

	switch (state) {
	case S_ILLEGAL:
		/* Interrupts are not expected while in this state */
		printk(KERN_ALERT "zzz-i2c-eprom: Unexpected interrupt");
		goto done_with_irq;
		break;

	case S_READ_1:
		/* Memory address */
		mem_addr = read_idx;
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);
		state = S_READ_2;
		break;

	case S_READ_2:
		/* Address for read mode */
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | I2C_ADDR << 1 | 1 << 0);
		state = S_READ_3;
		break;

	case S_READ_3:
		/* Memory data */
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_stop_bit);
		state = S_READ_4;
		break;

	case S_READ_4:
		read_message[read_idx] = status & 0xff;
		if (read_idx + 1 < read_len) {
			read_idx++;
			/* Address device for write mode */
			axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | I2C_ADDR << 1 | 0 << 0);
			state = S_READ_1;
		}
		else {
			/* Wake up sleeping user blocked on read */
			state = S_ILLEGAL;
			wake_up_interruptible(&wq);
		}
		break;

	case S_WRITE_0:
		/* I2C address device for write mode */
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | I2C_ADDR << 1 | 0 << 0);
		state = S_WRITE_1;
		break;

	case S_WRITE_1:
		/* Memory address */
		mem_addr = write_idx;
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | mem_addr);
		state = S_WRITE_2;
		break;

	case S_WRITE_2:
		/* Memory data */
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_stop_bit | mem_data);
		state = S_WRITE_3;
		if (write_idx + 1 < write_len) {
			/* prepare new data and address */
			write_idx++;
			state = S_WRITE_0; /* Actually quite possible the device has
								  internal auto increment on address in which
								  case we can go directly to S_WRITE_2 */
		}
		else {
			state = S_WRITE_3;
		}
		break;

	case S_WRITE_3:
		/* Wake up sleeping user blocked on write */
		state = S_ILLEGAL;
		wake_up_interruptible(&wq);
		break;
	}

done_with_irq:

	return (irq_handler_t)IRQ_HANDLED;
}

static int __zzz_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource res;
	int ret;

	init_waitqueue_head(&wq);
	state = S_ILLEGAL;

	irq_num = irq_of_parse_and_map(np, 0);

	dev_info(dev, "probe: irq_num=%d\n", irq_num);

	if ((ret = request_irq(irq_num, (irq_handler_t) zzz_irq_handler, IRQF_TRIGGER_RISING, DEVICE_NAME, dev))) {
		dev_err(dev, "probe: request_irq: %d\n", ret);
		return ret;
	}

	if ((ret = of_address_to_resource(np, 0, &res))) {
		dev_err(dev, "probe: of_address_to_resource: %d\n", ret);
		return ret;
	}

	io_base = ioremap(res.start, resource_size(&res));

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

static int __zzz_driver_remove(struct platform_device *pdev)
{
	device_destroy(zzzClass, MKDEV(majorNumber, 0));
	class_destroy(zzzClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);

	free_irq(irq_num, &pdev->dev);
	irq_dispose_mapping(irq_num);

	return 0;
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	DEFINE_WAIT(wait);

	len = min(len, (size_t)(MEM_SIZE - *offset));
	read_len = len;
	read_idx = 0;

	/* Pay special attention to the order in which these steps are performed!!! */
	{
		prepare_to_wait(&wq, &wait, TASK_INTERRUPTIBLE);
		/* Address device for write mode */
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | I2C_ADDR << 1 | 0 << 0);
		state = S_READ_1;
		schedule();
		finish_wait(&wq, &wait);
	}

	len = len - copy_to_user(buffer, read_message, len);

	*offset += len;

	return len;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	DEFINE_WAIT(wait);

	len = min(len, (size_t)(MEM_SIZE - *offset));
	len = len - copy_from_user(write_message, buffer, len);
	write_len = len;
	write_idx = 0;

	/* Pay special attention to the order in which these steps are performed!!! */
	{
		prepare_to_wait(&wq, &wait, TASK_INTERRUPTIBLE);
		/* I2C address device for write mode */
		state = S_WRITE_1;
		axi_master_write(i2c_ctrl_addr, i2c_ctrl_we_bit | i2c_ctrl_start_bit | I2C_ADDR << 1 | 0 << 0);
		schedule();
		finish_wait(&wq, &wait);
	}

	*offset += len;

	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	return 0;
}

static const struct of_device_id __zzz_driver_id[] = {
	{.compatible = "zzz-i2c-eprom"},
	{}
};

static struct platform_driver __zzz_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(__zzz_driver_id),
	},
	.probe = __zzz_driver_probe,
	.remove = __zzz_driver_remove
};

module_platform_driver(__zzz_driver);
