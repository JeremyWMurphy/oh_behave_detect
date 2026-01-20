function [chan,pulse_type,pulse_len,pulse_amp,pulse_intrvl,pulse_reps,pulse_base] = get_wave_params(f)

p = f.Children(1).Children(2).Children(1).Children;

chan = num2str(p(3).Value);
pulse_type = num2str(p(5).Value);
pulse_len = num2str(p(9).Value);
pulse_amp = num2str(p(7).Value);
pulse_intrvl = num2str(p(11).Value);
pulse_reps = num2str(p(13).Value);
pulse_base = num2str(p(15).Value);