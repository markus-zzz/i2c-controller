module tb #(
  parameter integer C_AXI_DATA_WIDTH = 32,
  parameter integer C_AXI_ADDR_WIDTH = 13
);

	reg clk, rst;
	reg lrclk, bclk;

	wire axi_aclk;
	wire axi_aresetn;
	reg [C_AXI_ADDR_WIDTH-1 : 0] axi_awaddr;
	reg [2 : 0] axi_awprot;
	reg axi_awvalid;
	wire axi_awready;
	reg [C_AXI_DATA_WIDTH-1 : 0] axi_wdata;
	reg [(C_AXI_DATA_WIDTH/8)-1 : 0] axi_wstrb;
	reg axi_wvalid;
	wire axi_wready;
	wire [1 : 0] axi_bresp;
	wire axi_bvalid;
	reg axi_bready;
	reg [C_AXI_ADDR_WIDTH-1 : 0] axi_araddr;
	reg [2 : 0] axi_arprot;
	reg axi_arvalid;
	wire axi_arready;
	wire [C_AXI_DATA_WIDTH-1 : 0] axi_rdata;
	wire [1 : 0] axi_rresp;
	wire axi_rvalid;
	reg axi_rready;

	wire busy_bit;

	wire i2c_scl;
	wire i2c_sda_io;

	assign axi_aclk = clk;
	assign axi_aresetn = ~rst;

	modem_axi_top dut(
	  .S00_AXI_aclk(axi_aclk),
	  .S00_AXI_aresetn(axi_aresetn),
	  .S00_AXI_awaddr(axi_awaddr),
	  .S00_AXI_awprot(axi_awprot),
	  .S00_AXI_awvalid(axi_awvalid),
	  .S00_AXI_awready(axi_awready),
	  .S00_AXI_wdata(axi_wdata),
	  .S00_AXI_wstrb(axi_wstrb),
	  .S00_AXI_wvalid(axi_wvalid),
	  .S00_AXI_wready(axi_wready),
	  .S00_AXI_bresp(axi_bresp),
	  .S00_AXI_bvalid(axi_bvalid),
	  .S00_AXI_bready(axi_bready),
	  .S00_AXI_araddr(axi_araddr),
	  .S00_AXI_arprot(axi_arprot),
	  .S00_AXI_arvalid(axi_arvalid),
	  .S00_AXI_arready(axi_arready),
	  .S00_AXI_rdata(axi_rdata),
	  .S00_AXI_rresp(axi_rresp),
	  .S00_AXI_rvalid(axi_rvalid),
	  .S00_AXI_rready(axi_rready),

	  .busy_bit_o(busy_bit),

	  .I2C_SCL_O(i2c_scl),
	  .I2C_SDA_IO(i2c_sda_io)
	);

	i2c_slave_model i2c_slave(
	  .scl(i2c_scl),
	  .sda(i2c_sda_io)
	);

	initial begin
		$dumpvars;
		clk = 0;
		rst = 1;
		lrclk = 0;
		bclk = 0;

		#5
		rst = 0;
	end

	always clk = #1 ~clk;
	always lrclk = #100 ~lrclk;
	always bclk = #3 ~bclk;

	// I2C bus needs pullups on both SCL and SDA for correct operation
	assign (weak0, weak1) i2c_scl = 1'b1;
	assign (weak0, weak1) i2c_sda_io = 1'b1;

endmodule

