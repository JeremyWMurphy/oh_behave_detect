function fig = make_ui_figure(fs,n_sec_disp,s,sig_amps)

default_id = 'NA';
pth_main  = 'C:\Users\Jeremy\Documents\';

%% main figure
ss = get(0,'screensize');
wd = ss(3);
ht = ss(4);

fig = uifigure('Position',[round(wd*0.25),round(ht*0.25),round(wd*0.8),round(ht*0.67)],'Color','black');

fig.UserData = struct('trialOutcome',0,'run_type',1,'state',0,'Done',0);

tg = uitabgroup(fig,'Position',[0,0,round(wd*0.8),round(ht*0.67)]);
t1 = uitab(tg,'Title','Data','BackgroundColor','black');
gl = uigridlayout(t1,[10 20],'BackgroundColor','black');

%% main data tab

%% main axes
ax = axes(gl);
ax.Layout.Row = [3 10];
ax.Layout.Column = [5 15];
ax.NextPlot = 'add';
ax.Color = [0 0 0];
ax.XColor = [1 1 1];
ax.YColor = [1 1 1];
ax.XLabel.String = 'SECS';
ax.YLim = [0 8];
ax.XLim = [1 fs*n_sec_disp];
ax.Title.Color = [1 1 1];
ax.Title.FontSize = 18;
ax.Title.FontWeight = 'normal';
ax.Title.String = 'Waiting to start';

ax.YTick = [1 3 5 7];
ax.YTickLabel = {'Piezo','Wheel','Frames','Licks'};
nan_vec = nan(fs*n_sec_disp,1);

ax.XAxis.Visible  = 'off';

plot(ax,nan_vec,'m'); % piezo signal
plot(ax,nan_vec,'g'); % wheel
plot(ax,nan_vec,'c'); % frame raw
plot(ax,nan_vec,'y'); % lick detector

%% notes text box

notes = uitextarea(gl,'Value','Notes Here ...');
notes.Layout.Column = [1 4];
notes.Layout.Row = [3 9];
notes.BackgroundColor = [0 0 0];
notes.FontColor = [0 1 0];
%% set id name and save path

% id label
id_txt = uilabel(gl, ...
    'Text','Run ID:',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[1 1 1]);
id_txt.Layout.Row = 1;
id_txt.Layout.Column = 1;

% subj name edit field
edt = uieditfield(gl, 'Value',default_id, ...
    'BackgroundColor',[0 0 0],...
    'FontColor',[1 1 1]);
edt.Layout.Row = 1;
edt.Layout.Column = [2 4];

% display and set save path field
pth_txt = uilabel(gl, ...
    'Text',pth_main,...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[1 1 1], ...
    'FontSize', 16);
pth_txt.Layout.Row = 1;
pth_txt.Layout.Column = [6 20];

pth_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Set Path',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) pthButtonPushed(pth_txt));
pth_btn.Layout.Row = 1;
pth_btn.Layout.Column = 5;

%% trial type toggle (Lick, Pair, Detect, live stream)

% display and set save path field
run_txt = uilabel(gl, ...
    'Text','Run Type:',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[1 1 1]);
run_txt.Layout.Row = 2;
run_txt.Layout.Column = 1;

tt3_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Detect',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) ttButtonPushed(fig,1,ax));
tt3_btn.Layout.Row = 2;
tt3_btn.Layout.Column = 2;

tt1_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Pair',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) ttButtonPushed(fig,2,ax));
tt1_btn.Layout.Row = 2;
tt1_btn.Layout.Column = 3;

tt4_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Live',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) ttButtonPushed(fig,4,ax));
tt4_btn.Layout.Row = 2;
tt4_btn.Layout.Column = 4;

%% run start, run end, quit buttons

% start button
strt_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Begin',...
    'FontColor',[1 1 1],...
     "ButtonPushedFcn", @(src,event) flowControl(fig,1));
strt_btn.Layout.Row = 10;
strt_btn.Layout.Column = 1;

% stop button
stp_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'End',...
    'FontColor',[1 1 1],...
     "ButtonPushedFcn", @(src,event) flowControl(fig,2));
stp_btn.Layout.Row = 10;
stp_btn.Layout.Column = 2;

% quit button
quit_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Quit',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) flowControl(fig,3));
quit_btn.Layout.Row = 10;
quit_btn.Layout.Column = 3;

%% reward button, open valve, close valve

valve_txt = uilabel(gl, ...
    'Text','Reward Port:',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
valve_txt.Layout.Row = 9;
valve_txt.Layout.Column = [16 20];
valve_txt.FontSize = 18;
valve_txt.FontColor = [1 1 1];

rew_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Give Reward',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) rewButtonPushed(s) ...    
);
rew_btn.Layout.Row = 10;
rew_btn.Layout.Column = [16 17];

open_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Open',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) vOpenButtonPushed(s) ...    
);
open_btn.Layout.Row = 10;
open_btn.Layout.Column = 18;

close_btn = uibutton(gl,...
    'BackgroundColor',[0 0 0],...
    'Text', 'Close',...
    'FontColor',[1 1 1],...
    "ButtonPushedFcn", @(src,event) vCloseButtonPushed(s) ...    
);
close_btn.Layout.Row = 10;
close_btn.Layout.Column = 19;

%% outcome

hit_txt = uilabel(gl, ...
    'Text','Hit',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
hit_txt.Layout.Row = 2;
hit_txt.Layout.Column = [6 7];
hit_txt.FontSize = 32;
hit_txt.FontColor = [0.5 0.5 0.5];

miss_txt = uilabel(gl, ...
    'Text','Miss',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
miss_txt.Layout.Row = 2;
miss_txt.Layout.Column = [8 9];
miss_txt.FontSize = 32;
miss_txt.FontColor = [0.5 0.5 0.5];

cw_txt = uilabel(gl, ...
    'Text','CW',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
cw_txt.Layout.Row = 2;
cw_txt.Layout.Column = [10 11];
cw_txt.FontSize = 32;
cw_txt.FontColor = [0.5 0.5 0.5];

fa_txt = uilabel(gl, ...
    'Text','FA',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
fa_txt.Layout.Row = 2;
fa_txt.Layout.Column = [12 13];
fa_txt.FontSize = 32;
fa_txt.FontColor = [0.5 0.5 0.5];

el_txt = uilabel(gl, ...
    'Text','EL',...    
    'BackgroundColor',[0 0 0],...
    'FontColor',[0.5 0.5 0.5]);
el_txt.Layout.Row = 2;
el_txt.Layout.Column = [14 15];
el_txt.FontSize = 32;
el_txt.FontColor = [0.5 0.5 0.5];

%% performance axes

axb = axes(gl);
axb.Layout.Row = [2 4];
axb.Layout.Column = [15 20];
axb.NextPlot = 'add';
axb.Color = [0 0 0];
axb.XColor = [1 1 1];
axb.YColor = [1 1 1];
axb.XLabel.String = 'N';
axb.YLim = [0 5];
axb.XLim = [0 10];
axb.Title.Color = [1 1 1];
axb.Title.FontSize = 14;
axb.Title.FontWeight = 'normal';
axb.Title.String = 'Performance';

axb.YTick = [1 2 3 4];
axb.YTickLabel = {'Hits','Misses','CWs','FAs'};
bh = barh(axb,[1 2 3 4],[0 0 0 0]);
bh.EdgeColor = 'none';
bh.FaceColor = 'flat';
bh.CData = ([3, 252, 107; 252, 206, 3; 3, 240, 252; 252, 3, 74]./255)./3;

axc = axes(gl);
axc.Layout.Row = [5 8];
axc.Layout.Column = [15 20];
axc.NextPlot = 'add';
axc.Color = [0 0 0];
axc.XColor = [1 1 1];
axc.YColor = [1 1 1];
axc.XLabel.String = 'Stimulus Amplitude';
axc.XTickLabelRotation = 90;
axc.YLim = [0 1];
axc.XLim = [0 sig_amps(end)];
axc.YLabel.String = 'P(hit)';
plot(axc,sig_amps,zeros(size(sig_amps)),'-sg'); % piezo signal

end

%% callbacks

function pthButtonPushed(txt)
    selpath = uigetdir('C:\Users\jeremy\Desktop\Data_Temp\');
    txt.Text = selpath;
end

function rewButtonPushed(s)
    write(s,'<S,7>','string');
end

function vOpenButtonPushed(s)
    write(s,'<S,5>','string');
end

function vCloseButtonPushed(s)
    write(s,'<S,6>','string');
end

function ttButtonPushed(fig,tt,ax)
    % set run type
    fig.UserData.run_type = tt;
    if tt == 1
        ax.Title.String = 'Detection Run... ';
    elseif tt == 2
        ax.Title.String = 'Pairing Run... ';
    elseif tt == 4
        ax.Title.String = 'live streaming, not saving...';
    end

end

function flowControl(fig,st)
  fig.UserData.state = st;
end
















