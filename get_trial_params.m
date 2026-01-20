function [n_trials,baseln,itis,lick_pause_time] = get_trial_params(f)

p = f.Children(1).Children(2).Children(1).Children;

n_trials = p(20).Value;
baseln = p(22).Value./1000;
itis = [p(24).Value p(26).Value]./1000;
lick_pause_time = p(28).Value;



