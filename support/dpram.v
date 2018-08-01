module dpram(
	// Generic synchronous dual-port RAM interface
	rclk, rrst, rce, oe, raddr, do,
	wclk, wrst, wce, we, waddr, di
);

	//
	// Default address and data buses width
	//
	parameter aw = 5;  // number of bits in address-bus
	parameter dw = 16; // number of bits in data-bus

	//
	// Generic synchronous double-port RAM interface
	//
	// read port
	input           rclk;  // read clock, rising edge trigger
	input           rrst;  // read port reset, active high
	input           rce;   // read port chip enable, active high
	input           oe;	   // output enable, active high
	input  [aw-1:0] raddr; // read address
	output [dw-1:0] do;    // data output

	// write port
	input          wclk;  // write clock, rising edge trigger
	input          wrst;  // write port reset, active high
	input          wce;   // write port chip enable, active high
	input          we;    // write enable, active high
	input [aw-1:0] waddr; // write address
	input [dw-1:0] di;    // data input

	reg [dw-1:0] mem [(1<<aw) -1:0] /* synthesis syn_ramstyle="block_ram" */;
	reg [aw-1:0] ra;                // register read address

	// read operation
	always @(posedge rclk) begin
		if (rce) begin
			ra <= raddr;
		end
	end

	assign do = mem[ra];

	// write operation
	always @(posedge wclk) begin
		if (we && wce) begin
			mem[waddr] <= di;
		end
	end

endmodule

