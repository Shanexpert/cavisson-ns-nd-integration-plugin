load data;
[x, info] = fsolve("f_mean_median", [mean_var; .5]);
disp(x);
