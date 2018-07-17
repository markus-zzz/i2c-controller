module i2c_axi_top #(
  parameter integer C_S00_AXI_DATA_WIDTH = 32,
  parameter integer C_S00_AXI_ADDR_WIDTH = 13
)
(
  /* AXI interface */
  input wire  S00_AXI_aclk,
  input wire  S00_AXI_aresetn,
  input wire [C_S00_AXI_ADDR_WIDTH-1 : 0] S00_AXI_awaddr,
  input wire [2 : 0] S00_AXI_awprot,
  input wire  S00_AXI_awvalid,
  output wire  S00_AXI_awready,
  input wire [C_S00_AXI_DATA_WIDTH-1 : 0] S00_AXI_wdata,
  input wire [(C_S00_AXI_DATA_WIDTH/8)-1 : 0] S00_AXI_wstrb,
  input wire  S00_AXI_wvalid,
  output wire  S00_AXI_wready,
  output wire [1 : 0] S00_AXI_bresp,
  output wire  S00_AXI_bvalid,
  input wire  S00_AXI_bready,
  input wire [C_S00_AXI_ADDR_WIDTH-1 : 0] S00_AXI_araddr,
  input wire [2 : 0] S00_AXI_arprot,
  input wire  S00_AXI_arvalid,
  output wire  S00_AXI_arready,
  output wire [C_S00_AXI_DATA_WIDTH-1 : 0] S00_AXI_rdata,
  output wire [1 : 0] S00_AXI_rresp,
  output wire  S00_AXI_rvalid,
  input wire  S00_AXI_rready,

  /* I2C interface */
  output wire I2C_SCL_O,
  inout wire I2C_SDA_IO
);
	wire clk;
	wire rst;

	assign clk = S00_AXI_aclk;
	assign rst = ~S00_AXI_aresetn;

	wire i2c_cmd_pulse;
	wire[10:0] i2c_ctrl_reg;
	wire[9:0] i2c_status_reg;

	wire i2c_sda_o;
	wire i2c_sda_oe;

	assign I2C_SDA_IO = i2c_sda_oe ? i2c_sda_o : 1'bz;

	i2c_axi_slave # (
	  .C_S_AXI_DATA_WIDTH(C_S00_AXI_DATA_WIDTH),
	  .C_S_AXI_ADDR_WIDTH(C_S00_AXI_ADDR_WIDTH))
	u_i2c_axi_slave (
	  .S_AXI_ACLK(S00_AXI_aclk),
	  .S_AXI_ARESETN(S00_AXI_aresetn),
	  .S_AXI_AWADDR(S00_AXI_awaddr),
	  .S_AXI_AWPROT(S00_AXI_awprot),
	  .S_AXI_AWVALID(S00_AXI_awvalid),
	  .S_AXI_AWREADY(S00_AXI_awready),
	  .S_AXI_WDATA(S00_AXI_wdata),
	  .S_AXI_WSTRB(S00_AXI_wstrb),
	  .S_AXI_WVALID(S00_AXI_wvalid),
	  .S_AXI_WREADY(S00_AXI_wready),
	  .S_AXI_BRESP(S00_AXI_bresp),
	  .S_AXI_BVALID(S00_AXI_bvalid),
	  .S_AXI_BREADY(S00_AXI_bready),
	  .S_AXI_ARADDR(S00_AXI_araddr),
	  .S_AXI_ARPROT(S00_AXI_arprot),
	  .S_AXI_ARVALID(S00_AXI_arvalid),
	  .S_AXI_ARREADY(S00_AXI_arready),
	  .S_AXI_RDATA(S00_AXI_rdata),
	  .S_AXI_RRESP(S00_AXI_rresp),
	  .S_AXI_RVALID(S00_AXI_rvalid),
	  .S_AXI_RREADY(S00_AXI_rready),

	  .i2c_cmd_pulse_o(i2c_cmd_pulse),
	  .i2c_ctrl_reg_o(i2c_ctrl_reg),
	  .i2c_status_reg_i(i2c_status_reg)
	);

	i2c_controller # (
	  .C_CLK_DIVIDER_LOG2(2))
	u_i2c_controller (
	  .clk(clk),
	  .rst(rst),

	  .i2c_cmd_pulse_i(i2c_cmd_pulse),
	  .i2c_ctrl_reg_i(i2c_ctrl_reg),
	  .i2c_status_reg_o(i2c_status_reg),

	  .I2C_SCL(I2C_SCL_O),
	  .I2C_SDA_O(i2c_sda_o),
	  .I2C_SDA_OE(i2c_sda_oe),
	  .I2C_SDA_I(I2C_SDA_IO)
	);

endmodule
