module modem_axi_top #(
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

  output wire busy_bit_o,

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

	wire [7:0] rx_rp;
	wire [7:0] rx_wp;
	wire [7:0] tx_rp;
	wire [7:0] tx_wp;


	wire [31:0] rx_rdata;
	wire [7:0] rx_raddr;
	wire [31:0] rx_wdata;
	wire [7:0] rx_waddr;

	wire [31:0] tx_rdata;
	wire [7:0] tx_raddr;
	wire [31:0] tx_wdata;
	wire [7:0] tx_waddr;
	wire tx_wen;
	wire rx_wen;

	wire [7:0] tx_byte;
	reg [7:0] rx_byte;

	wire msg_begin;
	wire msg_end;
	wire byte_valid;

	modem_axi_slave # (
	  .C_S_AXI_DATA_WIDTH(C_S00_AXI_DATA_WIDTH),
	  .C_S_AXI_ADDR_WIDTH(C_S00_AXI_ADDR_WIDTH))
	u_modem_axi_slave (
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

	  .rx_wp_i(rx_wp),
	  .tx_rp_i(tx_rp),
	  .rx_rp_o(rx_rp),
	  .tx_wp_o(tx_wp),

	  .tx_wdata_o(tx_wdata),
	  .tx_waddr_o(tx_waddr),
	  .tx_wen_o(tx_wen),

	  .rx_rdata_i(rx_rdata),
	  .rx_raddr_o(rx_raddr),

	  .i2c_cmd_pulse_o(i2c_cmd_pulse),
	  .i2c_ctrl_reg_o(i2c_ctrl_reg),
	  .i2c_status_reg_i(i2c_status_reg)
	);

	// TX Ring Buffer 1 KiB
	dpram # (
	  .aw(8),
	  .dw(32))
	u_tx_dpram (
	  .rclk(clk),
	  .rrst(rst),
	  .rce(1'b1),
	  .oe(1'b1),
	  .raddr(tx_raddr),
	  .do(tx_rdata),
	  .wclk(clk),
	  .wrst(rst),
	  .wce(tx_wen),
	  .we(1'b1),
	  .waddr(tx_waddr),
	  .di(tx_wdata)
	);

	tx_ctrl u_tx_ctrl(
	  .clk(clk),
	  .rst(rst),
	  .tx_wp_i(tx_wp),
	  .tx_rp_o(tx_rp),
	  .tx_rdata_i(tx_rdata),
	  .tx_raddr_o(tx_raddr),
	  .tx_byte_o(tx_byte),
	  .tx_byte_valid_o(byte_valid),
	  .tx_begin_o(msg_begin),
	  .tx_end_o(msg_end)
	);

	// RX Ring Buffer 1 KiB
	dpram # (
	  .aw(8),
	  .dw(32))
	u_rx_dpram (
	  .rclk(clk),
	  .rrst(rst),
	  .rce(1'b1),
	  .oe(1'b1),
	  .raddr(rx_raddr),
	  .do(rx_rdata),
	  .wclk(clk),
	  .wrst(rst),
	  .wce(rx_wen),
	  .we(1'b1),
	  .waddr(rx_waddr),
	  .di(rx_wdata)
	);

	rx_ctrl u_rx_ctrl(
	  .clk(clk),
	  .rst(rst),
	  .rx_rp_i(rx_rp),
	  .rx_wp_o(rx_wp),
	  .rx_wdata_o(rx_wdata),
	  .rx_waddr_o(rx_waddr),
	  .rx_wen_o(rx_wen),
	  .rx_byte_i(rx_byte),
	  .rx_byte_valid_i(byte_valid),
	  .rx_begin_i(msg_begin),
	  .rx_end_i(msg_end)
	);

	//
	// Default address and data buses width
	//
	parameter aw = 5;  // number of bits in address-bus
	parameter dw = 16; // number of bits in data-bus

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

	assign busy_bit_o = i2c_status_reg[9];

	//
	// Do the silly action of changing upper case to lower case and vice versa
	//

	task switch_case;
		input [7:0] in;
		output [7:0] out;
		begin
			out = in;
			if ("A" <= in && in <= "Z") begin
				//Is upper case
				out = in + ("a" - "A");
			end
			else if ("a" <= in && in <= "z") begin
				//Is lower case
				out = in - ("a" - "A");
			end
		end
	endtask

	always @(tx_byte) begin
		switch_case(tx_byte, rx_byte);
	end

	always @(posedge clk) begin
		if (byte_valid) $display("tx_byte: %x '%c'", tx_byte, tx_byte);
	end

endmodule

module tx_ctrl(
  input wire clk,
  input wire rst,
  input wire [7:0] tx_wp_i,
  output wire [7:0] tx_rp_o,

  input wire [31:0] tx_rdata_i,
  output wire [7:0] tx_raddr_o,
  output wire [7:0] tx_byte_o,
  output wire tx_byte_valid_o,
  output wire tx_begin_o,
  output wire tx_end_o
);

	reg [7:0] tx_rp;

	assign tx_rp_o = tx_rp;

	//
	// FSM for TX ring buffer management
	//

	parameter S_TX_IDLE = 6'b00_0001, S_TX_MSG_HEADER = 6'b00_0010, S_TX_MSG_PAYLOAD = 6'b00_0100, S_TX_MSG_FINISH = 6'b00_1000;

	reg [5:0] tx_curr_state, tx_next_state;

	always @(posedge clk) begin
		if (rst) begin
			tx_curr_state <= S_TX_IDLE;
		end
		else begin
			tx_curr_state <= tx_next_state;
		end
	end

	reg [7:0] tx_rp_tmp;
	reg [9:0] tx_msg_byte_len;
	reg [9:0] tx_msg_byte_idx;

	always @(posedge clk) begin
		if (rst) begin
			tx_msg_byte_len <= 0;
		end
		else if (tx_curr_state == S_TX_MSG_HEADER) begin
			tx_msg_byte_len <= tx_rdata_i[9:0];
		end
	end

	always @(posedge clk) begin
		if (rst || tx_curr_state == S_TX_IDLE) begin
			tx_rp_tmp <= tx_rp;
		end
		else if (tx_curr_state == S_TX_MSG_HEADER || (tx_curr_state == S_TX_MSG_PAYLOAD && tx_msg_byte_idx[1:0] == 2'b11)) begin
			tx_rp_tmp <= tx_rp_tmp + 1;
		end
	end

	always @(posedge clk) begin
		if (rst || tx_curr_state == S_TX_IDLE) begin
			tx_msg_byte_idx <= 2'b00;
		end
		else if (tx_curr_state == S_TX_MSG_PAYLOAD) begin
			tx_msg_byte_idx <= tx_msg_byte_idx + 1;
		end
	end

	assign tx_raddr_o = tx_rp_tmp;

	always @(*) begin

		tx_next_state = tx_curr_state;

		case (tx_curr_state)
			S_TX_IDLE: begin
				if (tx_wp_i != tx_rp) begin
					tx_next_state = S_TX_MSG_HEADER;
				end
			end
			S_TX_MSG_HEADER: begin
				tx_next_state = S_TX_MSG_PAYLOAD;
			end
			S_TX_MSG_PAYLOAD: begin
				if (tx_msg_byte_idx == tx_msg_byte_len - 1) begin
					tx_next_state = S_TX_MSG_FINISH;
				end
			end
			S_TX_MSG_FINISH: begin
				tx_next_state = S_TX_IDLE;
			end
		endcase

	end

	always @(posedge clk) begin
		if (rst) begin
			tx_rp <= 0;
		end
		else if (tx_curr_state == S_TX_MSG_FINISH) begin
			tx_rp <= tx_rp_tmp + (tx_msg_byte_len[1:0] != 2'b00 ? 1 : 0);
		end
	end

	reg [7:0] tx_byte;
	reg tx_byte_valid;

	assign tx_byte_o = tx_byte;
	assign tx_byte_valid_o = tx_byte_valid;
	assign tx_begin_o = (tx_curr_state == S_TX_MSG_HEADER);
	assign tx_end_o = (tx_curr_state == S_TX_MSG_FINISH);

	always @(posedge clk) begin
		if (rst) begin
			tx_byte_valid <= 0;
		end
		else begin
			tx_byte_valid <= (tx_curr_state == S_TX_MSG_PAYLOAD);
		end
	end

	wire [1:0] byte_sel;
	assign byte_sel = tx_msg_byte_idx[1:0] - 1;

	always @(*) begin
		case (byte_sel)
			2'b00: tx_byte = tx_rdata_i[7:0];
			2'b01: tx_byte = tx_rdata_i[15:8];
			2'b10: tx_byte = tx_rdata_i[23:16];
			2'b11: tx_byte = tx_rdata_i[31:24];
		endcase
	end


endmodule

module rx_ctrl(
  input wire clk,
  input wire rst,
  input wire [7:0] rx_rp_i,
  output wire [7:0] rx_wp_o,
  input wire [31:0] rx_wdata_o,
  output wire [7:0] rx_waddr_o,
  output wire rx_wen_o,
  input wire [7:0] rx_byte_i,
  output wire rx_byte_valid_i,
  output wire rx_begin_i,
  output wire rx_end_i
);

	reg [7:0] rx_wp;
	assign rx_wp_o = rx_wp;

	reg [7:0] rx_wp_tmp;

	//
	// FSM for RX ring buffer management
	//

	parameter S_RX_IDLE = 6'b00_0001, S_RX_MSG_HEADER = 6'b00_0010, S_RX_MSG_PAYLOAD = 6'b00_0100, S_RX_MSG_OVERFLOW = 6'b00_1000, S_RX_MSG_FLUSH = 6'b01_0000 ;

	reg [5:0] rx_curr_state, rx_next_state;

	always @(posedge clk) begin
		if (rst) begin
			rx_curr_state <= S_RX_IDLE;
		end
		else begin
			rx_curr_state <= rx_next_state;
		end
	end

	always @(*) begin

		rx_next_state = rx_curr_state;

		case (rx_curr_state)
			S_RX_IDLE: begin
				if (rx_byte_valid_i) begin
					rx_next_state = S_RX_MSG_PAYLOAD;
				end
			end
			S_RX_MSG_PAYLOAD: begin
				if (rx_waddr_o + 1 == rx_rp_i) begin
					rx_next_state = S_RX_MSG_OVERFLOW;
				end
				else if (rx_end_i) begin
					rx_next_state = S_RX_MSG_FLUSH;
				end
			end
			S_RX_MSG_FLUSH: begin
				rx_next_state = S_RX_MSG_HEADER;
			end
			S_RX_MSG_HEADER: begin
				rx_next_state = S_RX_IDLE;
			end
			S_RX_MSG_OVERFLOW: begin
				rx_next_state = S_RX_IDLE;
			end
		endcase

	end

	always @(posedge clk) begin
		if (rst) begin
			rx_wp_tmp <= 0;
		end
		else if (rx_curr_state == S_RX_IDLE && rx_byte_valid_i) begin
			rx_wp_tmp <= rx_wp + 1;
		end
		else if (rx_curr_state == S_RX_MSG_PAYLOAD && rx_byte_valid_i && rx_msg_byte_idx[1:0] == 2'b00) begin
			rx_wp_tmp <= rx_wp_tmp + 1;
		end
	end

	always @(posedge clk) begin
		if (rst) begin
			rx_wp <= 0;
		end
		else if (rx_curr_state == S_RX_MSG_HEADER) begin
			rx_wp <= rx_wp_tmp + 1;
		end
	end


	reg [31:0] rx_wdata_staging;
	reg [9:0] rx_msg_byte_idx;

	assign rx_wdata_o = (rx_curr_state == S_RX_MSG_HEADER) ? rx_msg_byte_idx : rx_wdata_staging;
	assign rx_waddr_o = (rx_curr_state == S_RX_MSG_HEADER) ? rx_wp : rx_wp_tmp;

	assign rx_wen_o = (rx_curr_state == S_RX_MSG_PAYLOAD && rx_msg_byte_idx[1:0] == 2'b00 && rx_byte_valid_i) ||
	                   rx_curr_state == S_RX_MSG_HEADER ||
	                   rx_curr_state == S_RX_MSG_FLUSH;

	always @(posedge clk) begin
		if (rst) begin
			rx_wdata_staging <= 0;
		end
		else if (rx_byte_valid_i) begin
			case (rx_msg_byte_idx[1:0])
				2'b00: rx_wdata_staging[7:0]   <= rx_byte_i;
				2'b01: rx_wdata_staging[15:8]  <= rx_byte_i;
				2'b10: rx_wdata_staging[23:16] <= rx_byte_i;
				2'b11: rx_wdata_staging[31:24] <= rx_byte_i;
			endcase
		end
	end

	always @(posedge clk) begin
		if (rst || rx_curr_state == S_RX_MSG_HEADER) begin
			rx_msg_byte_idx <= 0;
		end
		else if (rx_byte_valid_i) begin
			rx_msg_byte_idx <= rx_msg_byte_idx + 1;
		end
	end
endmodule
