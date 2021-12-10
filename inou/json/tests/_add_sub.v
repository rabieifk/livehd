
module _add_sub (input [7:0] a, input [7:0] b,
  output [7:0] apb, output [7:0] amb, output [7:0] mapb, output [7:0] mamb);

  assign apb =          a + b;
  assign amb =          a - b;
  assign mapb =       - a + b;
  assign mamb =       - a - b;
endmodule
