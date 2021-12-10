
module basic_test(m, n, p, q, y, z);
  input m ,n ,p ,q ;
  output y, z;
  
  wire mn1, mn2, paq, poq;
  assign mn1 = m & n;
  assign mn2 = m & n;
  assign paq = p & q;
  assign poq = p | q;
  assign y = mn1 ^ mn2;
  assign z = paq ~^ poq;
  
endmodule

