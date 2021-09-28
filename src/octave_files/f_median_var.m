function y = f_median_var (x)
     load data;
     y(1) = x(1)^2*gamma(1+2/x(2)) - (x(1)*gamma(1+1/x(2)))^2 - var_var;
     y(2) = x(2)*log(median_var/x(1)) + .3665;
endfunction
