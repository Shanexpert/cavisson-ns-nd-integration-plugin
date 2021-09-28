function y = f_b_var (x)
     load data;
     y(1) = x(1)^2*gamma(1+2/b) - x(1)*gamma(1+1/b)^2;
endfunction
