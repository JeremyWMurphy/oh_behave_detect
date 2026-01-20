function [y] = map_jm(x,in_min,in_max,out_min,out_max)

y = round((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
