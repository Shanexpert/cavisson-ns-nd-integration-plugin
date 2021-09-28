load data;
[x, info] = fsolve("f_mean_var", [mean_var;.5]);
disp(x);
