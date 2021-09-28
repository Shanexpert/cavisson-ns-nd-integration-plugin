load data;
[x, info] = fsolve("f_b_mean", [mean_var; .5]);
disp(x(1));
