`timescale 1ns / 1ps

module Top(
  input CLOCK_50,           // 50 MHz clock from DE1-SoC (PIN_AF14)
  output [9:0] LEDR,        // 10 red LEDs (LEDR[9:0])
  inout [35:0] GPIO_0,      // GPIO_0 header pins
  input [9:0] SW,           // Onboard switches
  input [3:0] KEY           // Onboard push buttons (KEY3 can be used as reset)
);

  // Directly use CLOCK_50 as system clock
  wire sysclk = CLOCK_50;

	// Use KEY3 as reset (active low on DE1-SoC)
  wire reset = ~KEY[3];

  // Map FpgaTop GPIO outputs to LEDR[7:0], leave LEDR[9:8] unconnected or assign as needed
  // wire [7:0] leds_internal, rfu_out, rfu_in;
  wire [7:0] leds_internal;

  // UART signals using specific GPIO pins // Existing UART0
  wire uart_tx, uart_rx;

	// New UART1
	wire uart1_tx, uart1_rx;
  
  // Assign UART to specific GPIO pins // Existing UART0
  assign GPIO_0[0] = uart_tx;  // UART TX on GPIO_0[0] // TX0
  assign uart_rx = GPIO_0[1];  // UART RX on GPIO_0[1] // RX0
  assign GPIO_0[2] = 1'b0;  // gnd on GPIO_0[2]

	// Assign UART1 to GPIO_0[3:4]
	assign GPIO_0[3] = uart1_tx;  // TX1
	assign uart1_rx  = GPIO_0[4]; // RX1
  assign GPIO_0[5] = 1'b0;  // gnd 

  // assign GPIO_0[4:11] = rfu_in;
  // assign rfu_out = GPIO_0[12:19];

  // Set unused GPIO pins to high-Z
  genvar i;
  generate
    for (i = 3; i < 36; i = i + 1) begin : gpio_unused
      assign GPIO_0[i] = 1'bz;
    end
  endgenerate

  FpgaTop flexpret(
    .clock(sysclk),
    .reset(reset),
    .io_gpio_out_0(leds_internal[1:0]),
    .io_gpio_out_1(leds_internal[3:2]),
    .io_gpio_out_2(leds_internal[5:4]),
    .io_gpio_out_3(leds_internal[7:6]),
		// existing ports for UART0
    .io_uart_rx(uart_rx),
    .io_uart_tx(uart_tx),
		// new UART1 ports for UART1
		.io_uart1_tx(uart1_tx),
		.io_uart1_rx(uart1_rx),
    .io_gpio_in_0(SW[1:0]),    // Use SW[1:0] for GPIO inputs
    .io_int_exts_1(KEY[0]),    // Use KEY[0] for external interrupt 1
    // .io_int_exts_2(KEY[1]),    // Use KEY[1] for external interrupt 2
    // .io_rfu_signals_in_0(rfu_in),
    // .io_rfu_signals_out_0(rfu_out)
  );

  assign LEDR[7:0] = leds_internal;
  assign LEDR[9:8] = 2'b00; // Unused, set to 0

endmodule
