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
  // wire [7:0] leds_internal;

  // Separate synchronization from LED control:
  // ports 0–2 drive LED0–LED5; port 3 drives sync instead of LED6–LED7.
  wire [5:0] leds_internal;
  wire [1:0] sync_out;

  // UART signals using specific GPIO pins // Existing UART0
  wire uart_tx, uart_rx;
	// New UART1
	wire uart1_tx, uart1_rx;
	// New UART2
	wire uart2_tx, uart2_rx;
  // New UART3
	wire uart3_tx, uart3_rx;
  
  // Assign UART to specific GPIO pins // Existing UART0
  assign GPIO_0[0] = uart_tx;  // UART TX on GPIO_0[0] // TX0
  assign uart_rx = GPIO_0[1];  // UART RX on GPIO_0[1] // RX0

	// Assign UART1
	assign GPIO_0[2] = uart1_tx;  // TX1
	assign uart1_rx  = GPIO_0[3]; // RX1

		// Assign UART2
	assign GPIO_0[4] = uart2_tx;  // TX2
	assign uart2_rx  = GPIO_0[5]; // RX2\

  // Assign UART3
  assign GPIO_0[6] = uart3_tx;  // TX3
  assign uart3_rx  = GPIO_0[7]; // RX3

  // Route FlexPRET port 3 bit 0 to GPIO_0[8] for synchronization pulses to the Mega.
  assign GPIO_0[8] = sync_out[0];

  // assign GPIO_0[4:11] = rfu_in;
  // assign rfu_out = GPIO_0[12:19];

  // Set unused GPIO pins to high-Z
  genvar i;
  generate
    for (i = 9; i < 36; i = i + 1) begin : gpio_unused
      assign GPIO_0[i] = 1'bz;
    end
  endgenerate

  FpgaTop flexpret(
    .clock(sysclk),
    .reset(reset),
    .io_gpio_out_0(leds_internal[1:0]),
    .io_gpio_out_1(leds_internal[3:2]),
    .io_gpio_out_2(leds_internal[5:4]),
    // .io_gpio_out_3(leds_internal[7:6]),
    // Route port 3 to synchronization instead of LEDs.
    .io_gpio_out_3(sync_out),
		// existing ports for UART0
    .io_uart_rx(uart_rx),
    .io_uart_tx(uart_tx),
		// new UART1 ports for UART1
		.io_uart1_tx(uart1_tx),
		.io_uart1_rx(uart1_rx),
		// new UART1 ports for UART2
		.io_uart2_tx(uart2_tx),
		.io_uart2_rx(uart2_rx),
    // new UART1 ports for UART3
    .io_uart3_tx(uart3_tx),
    .io_uart3_rx(uart3_rx),

		// 
    .io_gpio_in_0(SW[0]),    // Use SW[0] for GPIO inputs
		.io_gpio_in_1(SW[1]), 	// Use SW[1] for GPIO inputs
		
		.io_gpio_in_2(KEY[2]), 
		.io_gpio_in_3(KEY[1]), 

    // .io_int_exts_1(KEY[0]),    // Use KEY[0] for external interrupt 1
    // .io_int_exts_2(KEY[1]),    // Use KEY[1] for external interrupt 2
    // .io_rfu_signals_in_0(rfu_in),
    // .io_rfu_signals_out_0(rfu_out)

		//
  );

  // assign LEDR[7:0] = leds_internal;
  // assign LEDR[9:8] = 2'b00; // Unused, set to 0
  assign LEDR[5:0] = leds_internal; // Ports 0–2 drive LED0–LED5; port 3 is reserved for synchronization.
  assign LEDR[9:6] = 4'b0000; // Keep LED6–LED9 off, including the two LEDs previously driven by port 3.

endmodule
