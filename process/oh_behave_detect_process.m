function [] = oh_behave_detect_process()

fs = 2e3;
ttypes = dictionary('goTrial',2,'noGoTrial',3,'pairTrial',4,'lickAbort',10);

pth = 'D:\data\pom-opto-imaging\behavior\gpr26_162\';
runs = {'162_2026-02-09_T16-54-21'};

data = read_teensy_data(pth,runs);

s = get_teensy_behavior(data,fs,ttypes);




