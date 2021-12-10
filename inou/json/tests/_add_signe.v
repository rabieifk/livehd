
module _add_sign (input signed [7:0] sa, input [7:0] ua, input signed [7:0] sb, input [7:0] ub,
  output [7:0] uapub, output [7:0] uapsb, output [7:0] sapsb, output [7:0] sapub);

  assign uapub =          ua + ub;
  assign uapsb =          ua + sb;
  assign sapub =          sa + ub;
  assign sapsb =          sa + sb;
  
endmodule
