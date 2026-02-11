function [D] = read_teensy_data(pth,runs)

D = [];
last_ix = 0;

for i = 1:numel(runs)

    fid = fopen([pth runs{i} '\data_stream.csv']);
    data = fscanf(fid,'<%d,%d,%d,%d,%d,%d,%d,%d>\n');
    fclose(fid);

    r = mod(numel(data),8); % find an incomplete line at the end
    data = data(1:end-r);
    data = reshape(data,8,[])';
    strt = find(data(:,1)==0,1,'first'); % find teensy restart (this is always done at the start of the experiment)
    data = data(strt:end,:);
    data(:,1) = data(:,1) + last_ix;
    D = cat(1,D,data);
    last_ix = data(end,1);
    
end

D = array2table(D,'VariableNames',{'LoopNum','FrameNum','State','TrialOutcome','Ao0','Ao1','Licks','Wheel'});
summary(D)
