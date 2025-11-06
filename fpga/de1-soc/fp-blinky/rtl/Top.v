`timescale 1ns / 1ps


module Top(
  input CLOCK_50,           // 50 MHz clock from DE1-SoC (PIN_AF14)
  output [9:0] LEDR,        // 10 red LEDs (LEDR[9:0])
  output UART_TX,           // UART transmit
  input UART_RX,             // UART receive
  input [9:0] SW            // Onboard swiches
);


  // Directly use CLOCK_50 as system clock
  wire sysclk = CLOCK_50;

  // Map FpgaTop GPIO outputs to LEDR[7:0], leave LEDR[9:8] unconnected or assign as needed
  wire [7:0] leds_internal;

  FpgaTop flexpret(
    .clock(sysclk),
    .io_gpio_out_0(leds_internal[1:0]),
    .io_gpio_out_1(leds_internal[3:2]),
    .io_gpio_out_2(leds_internal[5:4]),
    .io_gpio_out_3(leds_internal[7:6]),
    .io_uart_rx(UART_RX),
    .io_uart_tx(UART_TX)
  );

  assign LEDR[7:0] = leds_internal;
  assign LEDR[9:8] = 2'b00; // Unused, set to 0


endmodule
