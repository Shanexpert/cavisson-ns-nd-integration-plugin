function y = f_mean_median (x)
     load data;
     y(1) = x(2)*log(median_var/x(1)) + .3665;
     y(2) = x(1)/x(2) * gamma(1/x(2)) - mean_var;
endfunction
