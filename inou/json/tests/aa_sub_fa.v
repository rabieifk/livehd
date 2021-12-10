module fullAdder ( In1, In2, Cin, Sum, Cout);
    input  In1, In2, Cin;
    output  Sum, Cout;

    assign Sum  = (In1 ^ In2) ^ Cin;
    assign Cout = (In1 & In2) | (In2 & Cin) | (Cin & In1);
endmodule

module fa_3mod (a, b, cin, cout, sout, sfa);
    input a , b, cin;
    output cout, sout, sfa;

    wire cfa1, sfa1, cfa2;
    fullAdder fa1(a, b, cin, cfa1, sfa1 );
    fullAdder fa2(a, b, cin, cfa2, sfa );
    fullAdder fa3(~cfa1, sfa1, ~cfa2, cout, sout );


endmodule

