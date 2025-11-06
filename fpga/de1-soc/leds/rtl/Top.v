module Top(
  output [9:0] LEDR
);

  // Simple static LED pattern for hardware test (Altera DE1-SoC)
  assign LEDR = 10'b0101010101;
  
endmodule
