function plotSaveDataAvailable(src, ~, fid, ax, up_every,f)

% OUTPUT from Teensy is,
% <loopNum, FrameNum, State, TrialOutcome, Ao0, Ao1, Licks, Wheel>

% read data
data = read(src,up_every,'char');

% if we have some data
if ~isempty(data)

    % write out raw data
    fprintf(fid,'%s',data');

    % parse for graphing and tracking teensy state -- this need to be as
    % efficient as possible
    strt = find(data=='<',1,'first');
    fin = find(data=='>',1,'last');
    data = data(strt:fin);
    data = sscanf(data,'<%d,%d,%d,%d,%d,%d,%d,%d,%d,%d>\n');    
    data = reshape(data,10,[])';

    data(data(:,7)==0,7) = NaN;

    % set data graphs
    ax.YTickLabel = {'Frames','Reward Valve','Remove Valve','Wheel','Piezo','Licks'};

    ax.Children(6).set('Ydata',[ax.Children(6).YData(size(data,1)+1:end) 1 + [diff(data(:,2)') 0]]); % frames
    ax.Children(5).set('Ydata',[ax.Children(5).YData(size(data,1)+1:end) 3 + data(:,9)']); % reward valve
    ax.Children(4).set('Ydata',[ax.Children(4).YData(size(data,1)+1:end) 5 + data(:,10)']); % remove valve
    ax.Children(3).set('Ydata',[ax.Children(3).YData(size(data,1)+1:end) 7 + data(:,8)'./1024]); % wheel
    ax.Children(2).set('Ydata',[ax.Children(2).YData(size(data,1)+1:end) 9 + data(:,5)'./4095]); % ao0
    ax.Children(1).set('Ydata',[ax.Children(1).YData(size(data,1)+1:end) 10 + data(:,7)']); % licks
   
    ot = find(data(:,4) ~= 0,1,'first'); 
    if ~isempty(ot)
        f.UserData.trialOutcome = data(ot,4);
        f.UserData.TeensyDone = true;
    else
        f.UserData.trialOutcome = data(1,4);
        f.UserData.TeensyDone = false;
    end

end


