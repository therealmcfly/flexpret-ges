`timescale 1ns / 1ps


module Top(
  input CLOCK_50,
  output [9:0] LEDR,
  output [3:0] GPIO_0,
  input [9:0] SW,
  input [3:0] KEY
);


  wire sysclk = CLOCK_50;

  // Map FpgaTop GPIO outputs to LEDR[7:0], leave LEDR[9:8] unconnected or assign as needed
  wire [7:0] leds_internal;

  FpgaTop flexpret(
    .clock(sysclk),
    .reset(!KEY[3]),
    .io_gpio_out_0(leds_internal[1:0]),
    .io_gpio_out_1(leds_internal[3:2]),
    .io_gpio_out_2(leds_internal[5:4]),
    .io_gpio_out_3(leds_internal[7:6]),
    .io_uart_rx(), // Not connected
    .io_uart_tx(GPIO_0[0])
  );

  // FpgaTop flexpret2(
  //   .clock(sysclk),
  //   .reset(!KEY[2]),
  //   // .io_gpio_out_0(leds_internal[1:0]),
  //   // .io_gpio_out_1(leds_internal[3:2]),
  //   //.io_gpio_out_2(leds_internal[5:4]),
  //   //.io_gpio_out_3(leds_internal[7:6]),
  //   .io_uart_rx(), // Not connected
  //   .io_uart_tx(GPIO_0[2])
  // );


  assign LEDR[7:0] = leds_internal;
  assign LEDR[9:8] = SW[9:8]; // Unused, set to 0
  assign GPIO_0[1] = 1'b0; // Drive GND on GPIO_0[1]
  assign GPIO_0[3] = 1'b0; // Drive GND on GPIO_0[3]



endmodule
