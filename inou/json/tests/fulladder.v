module fullAdder ( In1, In2, Cin, Sum, Cout );
    input  In1, In2, Cin;
    output  Sum, Cout;

    assign Sum  = (In1 ^ In2) ^ Cin;
    assign Cout = (In1 & In2) | (In2 & Cin) | (Cin & In1);
endmodule

module fa_2output_cell (a, b, c, d, e, occ, ocn, ods, oes);
    input a , b, c, d, e;
    output occ, ocn, ods, oes;

    wire wcout, wsum;
    fullAdder fa1(.In1(a), .In2(b), .Cin(c), .Cout(wcout), .Sum(wsum) );
    assign occ = c | wcout;
    assign ocn = ~wcout;
    assign ods = wsum & d;
    assign oes = wsum ^ e;

endmodule
