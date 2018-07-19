#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>

#define DEVNAME "zzz-i2c-eprom"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Markus Lavin (www.zzzconsulting.se)");
MODULE_DESCRIPTION("Device driver for the I2C EPROM tutorial posts from https://www.zzzconsulting.se/");
MODULE_VERSION("0.1");

#if 0

static irq_handler_t zzz_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	return (irq_handler_t)IRQ_HANDLED;
}

static int __zzz_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int irq_num;

	dev_info(dev, "probe\n");

#if 0
	int irq_num = platform_get_irq(pdev, 0);
#endif
	irq_num = irq_of_parse_and_map(dev->of_node, 0);

	printk(KERN_INFO "ZZZ: irq_num %d\n", irq_num);
	return request_irq(irq_num, (irq_handler_t) zzz_irq_handler, IRQF_TRIGGER_HIGH /* RISING */, DEVNAME, NULL);
}

static int __zzz_driver_remove(struct platform_device *pdev)
{
	int irq_num = platform_get_irq(pdev, 0);
	free_irq(irq_num, NULL);
	return 0;
}

static const struct of_device_id __zzz_driver_id[] = {
	{.compatible = "zzz-i2c-eprom"},
	{}
};

static struct platform_driver __zzz_driver = {
	.driver = {
		.name = DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(__zzz_driver_id),
	},
	.probe = __zzz_driver_probe,
	.remove = __zzz_driver_remove
};

module_platform_driver(__zzz_driver);

#else

#define assert(x)
static void __iomem *io_base = NULL;

static void axi_master_write(uint32_t address, uint32_t data)
{
	writel(data, io_base + address);
}

static uint32_t axi_master_read(uint32_t address)
{
	return readl(io_base + address);
}

const uint32_t i2c_ctrl_addr = 0x00c;
const uint32_t i2c_status_addr = 0x010;

const uint32_t i2c_ctrl_we_bit = 1 << 10;
const uint32_t i2c_ctrl_start_bit = 1 << 9;
const uint32_t i2c_ctrl_stop_bit = 1 << 8;

const uint32_t i2c_status_busy_bit = 1 << 9;
const uint32_t i2c_status_ack_bit = 1 << 8;

void i2c_mem_write(uint8_t i2c_addr, uint8_t mem_addr, uint8_t mem_data)
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

uint8_t i2c_mem_read(uint8_t i2c_addr, uint8_t mem_addr)
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

#define I2C_ADDR 0x10

static int __init zzz_init(void)
{
	int i;
	printk(KERN_INFO "ZZZ: Loaded module\n");

	io_base = ioremap(0x1e00b000, SZ_4K);

	const char str_out[] = "Hello EPROM!";

	for (i = 0; str_out[i]; ++i)
		i2c_mem_write(I2C_ADDR, i, str_out[i]);

	char str_in[16];
	memset(str_in, 0, sizeof(str_in));
	for (i = 0; i < sizeof(str_in)/sizeof(str_in[0]) - 1; ++i)
		str_in[i] = i2c_mem_read(I2C_ADDR, i);

	printk(KERN_INFO "ZZZ: str_in[] : '%s'\n", str_in);

	return 0;
}

static void __exit zzz_exit(void)
{
	printk(KERN_INFO "ZZZ: Unloaded module\n");
}

module_init(zzz_init);
module_exit(zzz_exit);
#endif
