/* Generated by Yosys livehd+0.9+ */

module top(out5, out);
  wire [3:0] \%out5_0 ;
  wire [3:0] \%out_0 ;
  wire [2:0] lg_0;
  wire [1:0] lg_1;
  wire [2:0] lg_2;
  wire [3:0] lg_3;
  wire [4:0] lg_4;
  wire [4:0] lg_5;
  output [3:0] out;
  output [3:0] out5;
  sum \___b_0:tmp_0:sum  (
    .\a.bar (lg_2),
    .\a.foo (lg_0),
    .b(lg_3),
    .\sout.o1.baz (lg_5),
    .sout2(lg_4)
  );
  assign \%out_0  = $signed(lg_1) + $signed(lg_5);
  assign \%out5_0  = $signed(lg_1) + $signed(lg_4);
  assign lg_0 = 3'h3;
  assign lg_1 = 2'h1;
  assign lg_2 = 3'h2;
  assign lg_3 = 4'h4;
  assign out5 = \%out5_0 ;
  assign out = \%out_0 ;
endmodule
/* Generated by Yosys livehd+0.9+ */

module sum(\a.foo , \a.bar , b, sout2, \sout.o1.baz );
  wire [3:0] \%sout2_0 ;
  wire [2:0] ___f_0;
  input [2:0] \a.bar ;
  input [1:0] \a.foo ;
  input [3:0] b;
  output [4:0] \sout.o1.baz ;
  output [4:0] sout2;
  assign ___f_0 = $signed(\a.foo ) + $signed(b);
  assign \%sout2_0  = $signed(\a.bar ) + $signed(b);
  assign sout2 = { 1'h0, \%sout2_0  };
  assign \sout.o1.baz  = { 2'h0, ___f_0 };
endmodule
