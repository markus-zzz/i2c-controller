
module i2c_controller #
(
	parameter integer C_CLK_DIVIDER_LOG2 = 1
)
(
	input wire clk,
	input wire rst,

	output wire I2C_SCL,
	output wire I2C_SDA_O,
	output wire I2C_SDA_OE,
	input wire  I2C_SDA_I,

	input wire i2c_cmd_pulse_i,
	input wire[10:0] i2c_ctrl_reg_i,
	output wire[9:0] i2c_status_reg_o,
	input wire i2c_irq_ack_pulse_i,
	output wire i2c_irq_o
);

	wire ctrl_we;
	wire ctrl_start;
	wire ctrl_stop;
	wire[7:0] ctrl_data;

	wire status_busy;
	wire status_ack;
	wire[7:0] status_data;

	reg[1:0] scl_phase;
	reg scl;
	reg sda;
	reg sda_oe;

	reg[7:0] data_out;
	reg[7:0] data_in;
	reg ack_in;

	reg i2c_irq;

	assign ctrl_we    = i2c_ctrl_reg_i[10];
	assign ctrl_start = i2c_ctrl_reg_i[9];
	assign ctrl_stop  = i2c_ctrl_reg_i[8];
	assign ctrl_data  = i2c_ctrl_reg_i[7:0];

	assign i2c_status_reg_o = {status_busy, status_ack, status_data};

	// Derive a clock enable for a 4x SCL clock
	wire scl_4x_clk_en;
	reg[C_CLK_DIVIDER_LOG2-1:0] clk_divider;
	assign scl_4x_clk_en = (clk_divider == 0) ? 1 : 0;

	always @( posedge clk ) begin
		if (rst) begin
			clk_divider <= 0;
		end
		else begin
			clk_divider <= clk_divider + 1;
		end
	end

	always @( posedge clk ) begin
		if (rst) begin
			scl_phase <= 2'b00;
		end
		else if (scl_4x_clk_en) begin
			scl_phase <= scl_phase + 2'b01;
		end
	end


	// FSM for idle->start->data->ack->stop etc

	parameter S_IDLE = 6'b00_0001, S_SYNC = 6'b00_0010, S_START = 6'b00_0100, S_DATA = 6'b00_1000, S_ACK = 6'b01_0000, S_STOP = 6'b10_0000;

	reg [5:0] curr_state, next_state;

	always @(posedge clk) begin
		if (rst) begin
			curr_state <= S_IDLE;
		end
		else begin
			curr_state <= next_state;
		end
	end


	always @(*) begin

		next_state = curr_state;

		case (curr_state)
			S_IDLE: begin
				if (i2c_cmd_pulse_i) begin
					next_state = S_SYNC;
				end
			end
			S_SYNC: begin
				if (scl_4x_clk_en && scl_phase == 2'b11) begin
					next_state = ctrl_start ? S_START : S_DATA;
				end
			end
			S_START: begin
				if (scl_4x_clk_en && scl_phase == 2'b11) begin
					next_state = S_DATA;
				end
			end
			S_DATA: begin
				if (scl_4x_clk_en && scl_phase == 2'b11 && data_cntr == 3'h7) begin
					next_state = S_ACK;
				end
			end
			S_ACK: begin
				if (scl_4x_clk_en && scl_phase == 2'b11) begin
					next_state = ctrl_stop ? S_STOP : S_IDLE;
				end
			end
			S_STOP: begin
				if (scl_4x_clk_en && scl_phase == 2'b11) begin
					next_state = S_IDLE;
				end
			end
		endcase

	end

	reg [2:0] data_cntr;
	always @(posedge clk) begin
		if (rst) begin
			data_cntr <= 0;
		end
		else if (curr_state == S_DATA && scl_4x_clk_en && scl_phase == 2'b11) begin
			data_cntr <= data_cntr + 1;
		end
	end

	// IRQ generation
	assign i2c_irq_o = i2c_irq;
	always @(posedge clk) begin
		if (rst) begin
			i2c_irq <= 0;
		end
		else begin
			if (scl_4x_clk_en && curr_state != S_IDLE && next_state == S_IDLE) begin
				i2c_irq <= 1;
			end
			// Clearing has lower priority
			else if (i2c_irq_ack_pulse_i) begin
				i2c_irq <= 0;
			end
		end
	end

	assign status_busy = (curr_state != S_IDLE);
	assign status_ack = ~ack_in;
	assign status_data = data_in;

	// I2C clocking scheme
	//
	//        -+                             +-----------------------------+
	// SCL     |                             |                             |
	//         +-----------------------------+                             +-
	//
	//        -+            +-+            +-+            +-+            +-+
	// 4x_en   |            | |            | |            | |            | |
	//         +------------+ +------------+ +------------+ +------------+ +-
	//
	// phase      2'b00          2'b01          2'b10          2'b11

	// SCL generation
	always @(posedge clk) begin
		if (rst) begin
			scl <= 1'b1;
		end
		else if (scl_4x_clk_en && scl_phase == 2'b11) begin
			scl <= 1'b0;
		end
		else if (scl_4x_clk_en && scl_phase == 2'b01) begin
			scl <= 1'b1;
		end
	end

	// SDA generation
	always @(posedge clk) begin
		if (rst) begin
			sda <= 1'b1;
		end
		else if (scl_4x_clk_en) begin
			if (curr_state == S_START) begin
				// SDA 1 -> 0 transition when SCL is high
				if (scl_phase == 2'b00) begin
					sda <= 1'b1;
				end
				if (scl_phase == 2'b10) begin
					sda <= 1'b0;
				end
			end
			if (curr_state == S_STOP) begin
				// SDA 0 -> 1 transition when SCL is high
				if (scl_phase == 2'b00) begin
					sda <= 1'b0;
				end
				if (scl_phase == 2'b10) begin
					sda <= 1'b1;
				end
			end
			if (curr_state == S_ACK) begin
				if (scl_phase == 2'b00) begin
					sda <= 1'b0;
				end
			end
			if (curr_state == S_DATA) begin
				// SDA shifted data byte
				if (scl_phase == 2'b00) begin
					sda <= data_out[7];
				end
			end
		end
	end

	// Outgoing data shift register
	always @(posedge clk) begin
		if (rst) begin
			data_out <= 8'h0;
		end
		else if (scl_4x_clk_en) begin
			if (curr_state == S_SYNC) begin
				data_out <= ctrl_data;
			end
			if (curr_state == S_DATA) begin
				if (scl_phase == 2'b00) begin
					data_out <= {data_out[6:0], 1'b0};
				end
			end
		end
	end

	// Incomming data shift register
	always @(posedge clk) begin
		if (rst) begin
			data_in <= 8'h0;
		end
		else if (scl_4x_clk_en) begin
			if (curr_state == S_DATA) begin
				if (scl_phase == 2'b10) begin
					data_in <= {data_in[6:0], I2C_SDA_I};
				end
			end
		end
	end

	// Incomming ack
	always @(posedge clk) begin
		if (rst) begin
			ack_in <= 1'b1;
		end
		else if (scl_4x_clk_en) begin
			if (curr_state == S_ACK) begin
				if (scl_phase == 2'b10) begin
					ack_in <= I2C_SDA_I;
				end
			end
		end
	end

	assign I2C_SCL = curr_state == S_START ||
	                 curr_state == S_STOP ||
	                 curr_state == S_DATA ||
	                 curr_state == S_ACK ? scl : 1'b1;

	assign I2C_SDA_O = sda;
	assign I2C_SDA_OE = curr_state == S_START ||
	                    curr_state == S_STOP ||
	                    (curr_state == S_DATA && ctrl_we) ||
	                    (curr_state == S_ACK && ~ctrl_we);

endmodule
