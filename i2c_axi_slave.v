module i2c_axi_slave #
(
	parameter integer C_S_AXI_DATA_WIDTH = 32,
	parameter integer C_S_AXI_ADDR_WIDTH = 13
)
(
	input wire  S_AXI_ACLK,
	input wire  S_AXI_ARESETN,
	input wire [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_AWADDR,
	input wire [2 : 0] S_AXI_AWPROT,
	input wire  S_AXI_AWVALID,
	output wire  S_AXI_AWREADY,
	input wire [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_WDATA,
	input wire [(C_S_AXI_DATA_WIDTH/8)-1 : 0] S_AXI_WSTRB,
	input wire  S_AXI_WVALID,
	output wire  S_AXI_WREADY,
	output wire [1 : 0] S_AXI_BRESP,
	output wire  S_AXI_BVALID,
	input wire  S_AXI_BREADY,
	input wire [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_ARADDR,
	input wire [2 : 0] S_AXI_ARPROT,
	input wire  S_AXI_ARVALID,
	output wire  S_AXI_ARREADY,
	output wire [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_RDATA,
	output wire [1 : 0] S_AXI_RRESP,
	output wire  S_AXI_RVALID,
	input wire  S_AXI_RREADY,

	output wire i2c_cmd_pulse_o,
	output wire i2c_irq_ack_pulse_o,
	output wire[10:0] i2c_ctrl_reg_o,
	input wire[9:0] i2c_status_reg_i
);

	reg [C_S_AXI_ADDR_WIDTH-1 : 0] axi_awaddr;
	reg axi_awready;
	reg axi_wready;
	reg [1:0] axi_bresp;
	reg axi_bvalid;
	reg [C_S_AXI_ADDR_WIDTH-1 : 0] axi_araddr;
	reg axi_arready;
	reg [C_S_AXI_DATA_WIDTH-1 : 0] axi_rdata;
	reg [1 : 0] axi_rresp;
	reg axi_rvalid;

	reg [C_S_AXI_DATA_WIDTH-1 : 0] slv_reg_a;
	reg [C_S_AXI_DATA_WIDTH-1 : 0] slv_reg_b;
	reg [C_S_AXI_DATA_WIDTH-1 : 0] slv_reg_c;
	reg [10:0] slv_reg_i2c_ctrl;

	wire slv_reg_rden;
	wire slv_reg_wren;
	reg [C_S_AXI_DATA_WIDTH-1:0] reg_data_out;
	reg i2c_cmd_pulse;
	reg i2c_irq_ack_pulse;

	assign i2c_ctrl_reg_o = slv_reg_i2c_ctrl;

	assign S_AXI_AWREADY = axi_awready;
	assign S_AXI_WREADY = axi_wready;
	assign S_AXI_BRESP = axi_bresp;
	assign S_AXI_BVALID = axi_bvalid;
	assign S_AXI_ARREADY = axi_arready;
	assign S_AXI_RDATA = axi_rdata;
	assign S_AXI_RRESP = axi_rresp;
	assign S_AXI_RVALID = axi_rvalid;

	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_awready <= 1'b0;
		end
		else begin
			if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID) begin
				axi_awready <= 1'b1;
			end
			else begin
				axi_awready <= 1'b0;
			end
		end
	end

	// Implement axi_awaddr latching
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_awaddr <= 0;
		end
		else begin
			if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID) begin
				axi_awaddr <= S_AXI_AWADDR;
			end
		end
	end

	// Implement axi_wready generation
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_wready <= 1'b0;
		end
		else begin
			if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID) begin
				axi_wready <= 1'b1;
			end
			else begin
				axi_wready <= 1'b0;
			end
		end
	end

	assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

	always @( posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			slv_reg_a <= 0;
			slv_reg_b <= 0;
			slv_reg_c <= 0;
		end
		else begin
			if (slv_reg_wren && axi_awaddr[12:0] == 13'h1000) begin
				slv_reg_a <= S_AXI_WDATA;
			end
			if (slv_reg_wren && axi_awaddr[12:0] == 13'h1004) begin
				slv_reg_b <= S_AXI_WDATA;
			end
			if (slv_reg_wren && axi_awaddr[12:0] == 13'h1008) begin
				slv_reg_c <= S_AXI_WDATA;
			end
			if (slv_reg_wren && axi_awaddr[12:0] == 13'h100c) begin
				slv_reg_i2c_ctrl <= S_AXI_WDATA[10:0];
			end
		end
	end

	assign i2c_cmd_pulse_o = i2c_cmd_pulse;

	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			i2c_cmd_pulse <= 0;
		end
		else begin
			i2c_cmd_pulse <= slv_reg_wren && axi_awaddr[12:0] == 13'h100c;
		end
	end

	assign i2c_irq_ack_pulse_o = i2c_irq_ack_pulse;

	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			i2c_irq_ack_pulse <= 0;
		end
		else begin
			i2c_irq_ack_pulse <= slv_reg_wren && axi_awaddr[12:0] == 13'h1020;
		end
	end

	// Implement write response logic generation
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_bvalid  <= 0;
			axi_bresp   <= 2'b0;
		end
		else begin
			if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID) begin
				// indicates a valid write response is available
				axi_bvalid <= 1'b1;
				axi_bresp  <= 2'b0; // 'OKAY' response
			end
			else begin
				if (S_AXI_BREADY && axi_bvalid) begin
					axi_bvalid <= 1'b0;
				end
			end
		end
	end

	// Implement axi_arready generation
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_arready <= 1'b0;
			axi_araddr  <= 32'b0;
		end
		else begin
			if (~axi_arready && S_AXI_ARVALID) begin
				axi_arready <= 1'b1;
				axi_araddr  <= S_AXI_ARADDR;
			end
			else begin
				axi_arready <= 1'b0;
			end
		end
	end

	// Implement axi_rvalid generation
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_rvalid <= 0;
			axi_rresp  <= 0;
		end
		else begin
			if (axi_arready && S_AXI_ARVALID && ~axi_rvalid) begin
				// Valid read data is available at the read data bus
				axi_rvalid <= 1'b1;
				axi_rresp  <= 2'b0; // 'OKAY' response
			end
			else if (axi_rvalid && S_AXI_RREADY) begin
				// Read data is accepted by the master
				axi_rvalid <= 1'b0;
			end
		end
	end

	// Implement memory mapped register select and read logic generation
	assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;
	always @* begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			reg_data_out <= 0;
		end
		else begin
			// Address decoding for reading registers
			case ( axi_araddr[12:0] )
				13'h1000: reg_data_out <= slv_reg_a;
				13'h1004: reg_data_out <= slv_reg_b;
				13'h1008: reg_data_out <= slv_reg_c;
				13'h100c: reg_data_out <= slv_reg_i2c_ctrl;
				13'h1010: reg_data_out <= i2c_status_reg_i;
				default : reg_data_out <= 0;
			endcase
		end
	end

	// Output register or memory read data
	always @( posedge S_AXI_ACLK ) begin
		if ( S_AXI_ARESETN == 1'b0 ) begin
			axi_rdata <= 0;
		end
		else begin
			if (slv_reg_rden) begin
				axi_rdata <= reg_data_out;
			end
		end
	end

endmodule
