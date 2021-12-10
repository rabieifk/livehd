

module basic_pin(a, b, c, d, oa);
  input signed a, b, c, d ;
  output signed oa;
  wire signed oo, ox;
  assign oo = a | b;
  assign ox = c ^ d;
  assign oa = oo & ox;
endmodule

