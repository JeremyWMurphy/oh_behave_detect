function [s] = get_teensy_behavior(d,fs,ttypes)

piezo = d.Ao0;
piezo = 5*(piezo./4095);
state = d.State;
licks = d.Licks;
wheel = d.Wheel;
trial_outcome = d.TrialOutcome;

go_trials = find(diff(state)==ttypes('goTrial'));
nogo_trials = find(diff(state)==ttypes('noGoTrial'));
pair_trials = find(diff(state)==ttypes('pairTrial'));

tot_trials = numel(go_trials) + numel(nogo_trials) + numel(pair_trials);
all_trials_ixs = [[ones(size(go_trials)); 2*ones(size(nogo_trials)); 3*ones(size(pair_trials))] ...
    [go_trials; nogo_trials; pair_trials]];
[~,ix] = sort(all_trials_ixs(:,2),'ascend');
all_trials_ixs = all_trials_ixs(ix,:);

if ~isempty(go_trials)
    fprintf('\nGo-NoGo Run detected')
    fprintf(['\n' num2str(numel(go_trials))  ' Go trials'])
    fprintf(['\n' num2str(numel(nogo_trials))  ' NoGo trials'])
elseif ~isempty(pair_trials)
    fprintf('\nPairing Run detected')
    fprintf(['\n' num2str(numel(pair_trials))  ' Pairing trials'])
else
    error('\nUnrecognized run type')
end

% find indices of early lick aborted trials
abort_trials = zeros(tot_trials,2);
if any(state==10)
    for i = 1:size(all_trials_ixs,1)-1
        state_win = state(all_trials_ixs(i,2)+1:all_trials_ixs(i+1,2)-1);
        if any(diff(state_win)==ttypes('lickAbort')-ttypes('goTrial') | diff(state_win)==ttypes('lickAbort')-ttypes('noGoTrial')) 
            abort_trials(i,1) = all_trials_ixs(i,2);
            abort_trials(i,2) = 1;
        else
            abort_trials(i,1) = all_trials_ixs(i,2);
        end
    end
    % last trial
    state_win = state(all_trials_ixs(end,2):end);
    if any(diff(state_win)==ttypes('lickAbort')-ttypes('goTrial') | diff(state_win)==ttypes('lickAbort')-ttypes('noGoTrial')) 
        abort_trials(end,1) = all_trials_ixs(i+1,2);
        abort_trials(end,2) = 1;
    else
        abort_trials(end,1) = all_trials_ixs(i+1,2);
    end
end

fprintf(['\n' num2str(sum(abort_trials(:,2))) ' aborted trials']);

if ~all(abort_trials(:,1) == all_trials_ixs(:,2))
    error('\ntrial indices are funky, might want to take a closer look')
end

all_trials_ixs = [all_trials_ixs(:,2) all_trials_ixs(:,1) abort_trials(:,2)];

beh = [];
% for each trial, get outcome
for i = 1:size(all_trials_ixs,1)-1
    ttype = all_trials_ixs(i,2);
    abrt = all_trials_ixs(i,3);
    b = [];
    if ~abrt
        if ttype == 1 % go trial
            trl_ixs = all_trials_ixs(i,1)+1:all_trials_ixs(i+1,1)-1;
            pz_win = piezo(trl_ixs);
            pz_amp = round(max(pz_win),2);
            pz_onset = find(diff(pz_win)>0,1,'first');
            lk_win = licks(trl_ixs);
            lk_ixs = find(diff(lk_win)>0);
            lk_ix = lk_ixs(find(lk_ixs>pz_onset & lk_ixs<= pz_onset+2*fs,1,'first'));
            if isempty(lk_ix) % it was a miss
                b = [pz_amp 0 0];
            else % hit
                b = [pz_amp 1 (lk_ix-pz_onset)./fs];
            end

        elseif ttype == 2 % no go
            trl_ixs = all_trials_ixs(i,1)+1:all_trials_ixs(i+1,1)-1;          
            lk_win = licks(trl_ixs);
            lk_ixs = find(diff(lk_win)>0);
            lk_ix = find(lk_ixs);
            if isempty(lk_ix) % it was a correct withold
                b = [0 2 0];
            else % fa
                b = [0 3 0];
            end

        elseif ttype == 3 % pair

        end
        beh = cat(1,beh,b);
    end

end

p_amps = unique(beh(:,1));
beh_summ = [];
for i = 1:numel(p_amps)
    pts = beh(beh(:,1)==p_amps(i),:);
    if p_amps(i)==0
        fa_rate = nnz(pts(:,2)==3)./(nnz(pts(:,2)==3)+nnz(pts(:,2)==2));
        b = [p_amps(i) fa_rate 0];
    else
        hit_rate = nnz(pts(:,2)==1)./(nnz(pts(:,2)==1)+nnz(pts(:,2)==0));
        rt = mean(pts(:,3));
        b = [p_amps(i) hit_rate rt];
    end
    beh_summ = cat(1,beh_summ,b);
end

figure,
subplot(1,2,1)
plot(beh_summ(:,1),beh_summ(:,2),'-ok')   
subplot(1,2,2)
plot(beh_summ(:,1),beh_summ(:,3),'-ok')   

end



