function y = f_b_median (x)
     load data;
     y(1) = b*log(median_var/x(1)) - log(-log(.5));
endfunction
