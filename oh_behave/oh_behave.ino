#include "WaveTable.h"
#include <Adafruit_MCP4728.h>
#include <Wire.h>

/*
Serial interaction
<W,0,0,0,0,0,0,0> : set wave parameters, <W,(1)channel,(2)wave type,(3)wave length,(4)wave amp,(5)wave inter-pulse length, (6) wave reps, (7) wave baseline>
<S,0> : set state
*/

const uint Fs = 2000;  // Teensy sampling rate

bool enforceEarlyLick = false; // error out if the mouse licks pre-stim 
uint lickMax = 20; // how many licks are too many licks
uint lickDebounce = 0.05 * Fs; // so as not to over-count licks, mouse would have to lick >= 20 Hz for this to under-count 

bool waitForNextFrame = false; // if frame counting wait for a new frame to start to present a stimulus

uint contingentStim = 0; // index of the analog channel that the animal is responding to / detecting

// time lengths
uint trigLen =    Fs * 0.2; // trigger lenght in seconds
uint respLen =    Fs * 2;   // how long from stim start is a response considered valid,
uint valveLen =   Fs * 1;   // how long to open reward valve in samples
uint consumeLen = Fs * 3;   // how long from reward administration does the animal have to consume the reward
uint vacLen =     Fs * 1;   // how long to open reward valve in samples
uint pairDelay =  Fs * 0;   // in pairing trials, time between stim and reward
uint earlyLen =   Fs * 0.2; // how long to broadcast early lick
uint removeLen =  Fs * 1;

// channels
// ins
const uint wheelChan = 14;  // analog in
const uint frameChan = 23;  // frame counter channel, interrupt
const uint lickChan  = 22;  //lick channel
// outs
const uint trigChan1 = 0;  // trigger channel;
const uint trigChan2 = 1;  // trigger channel;
const uint trigChan3 = 2;  // trigger channel;
const uint trigChan4 = 3;  // trigger channel;

const uint valveChan1 = 4; // reward valve
const uint valveChan2 = 6; // vac line valve for reward removal

volatile uint16_t wheelVal = 0;
volatile int lickVal = 0;
volatile int rewardValveVal = 0;
volatile int removeValveVal = 0;

// state definitions
const uint IDLE       = 0;
const uint RESET      = 1;
const uint GO         = 2;
const uint NOGO       = 3;
const uint PAIR       = 4; 
const uint VALVEO     = 5; 
const uint VALVEC     = 6; 
const uint TRIGGER    = 7; 
const uint REWARD     = 8; 
const uint STIMULUS   = 9; 
const uint EARLYLICK = 10; 
volatile uint State = IDLE;

// Behavior tracking
const uint HIT  = 1;
const uint MISS = 2;
const uint CW   = 3;
const uint FA   = 4;
const uint LICK = 5;
volatile uint trialOutcome = 0;

// general timers/trackers
volatile bool stimEnd = false;
volatile bool conStimOn = false;

volatile bool respStart = true;
volatile bool respEnd = false;
volatile bool hasResponded = false;
volatile uint32_t respT = 0;

volatile uint lickCount = 0;
volatile uint lickLow = 0;
volatile bool firstLick = true;
volatile bool earlyStart = false;
volatile uint32_t earlyT = 0;

volatile bool dispStart = true;
volatile uint32_t dispT = 0;

volatile bool consumeStart = true;
volatile uint32_t consumeT = 0;

volatile bool removeStart = true;
volatile uint32_t removeT = 0;

volatile bool trigStart = true;
volatile bool trigEnd = false;
volatile uint32_t trigT = 0;

// waveform parameters to be set over serial for each of the 4 DAC channels
volatile uint waveType[4] = { 1, 1, 1, 1 };   // wave types: 0 = whale, 1 = square
volatile uint waveDur[4] = { 20, 0, 0, 0 };   // duration of pulse, fixed for whale right now, set via serial in ms, converted to sample points
volatile uint waveAmp[4] = { 0, 0, 0, 0 };    // max voltage amplitude, in 12bit - 0-4095
volatile uint waveIPI[4] = { 20, 0, 0, 0 };   // duration between pulses, set via serial in ms, converted to sample points
volatile uint waveReps[4] = { 5, 0, 0, 0 };   // number of times to repeat wave pulse and interpulse interval
volatile uint rampStep[4] = { 0, 0, 0, 0 };   // specific to ramping variables, still in development
volatile uint whaleStep[4] = { 0, 0, 0, 0 };  // specific to ramping variables, still in development
volatile uint waveBase[4] = { 0, 0, 0, 0 };   // baseline prior to stimulus onset, in ms, allows for offsets between the stimuli, set via serial in ms, converted to sample points
// waveform tracking
volatile uint wavIncrmntr[4] = { 0, 0, 0, 0 };  // for keeping track of where we are in a given stimulus presentation
volatile uint repCntr[4] = { 0, 0, 0, 0 };      // for keeping track of pulse repitions
volatile uint ipiCntr[4] = { 0, 0, 0, 0 };      // for keeping track of where we are in an interpulse interval
volatile uint BaseCntr[4] = { 0, 0, 0, 0 };     // for keeping track of where we are in a baseline period
volatile uint whaleCntr[4] = { 0, 0, 0, 0 };    //
volatile uint waveParams[7] = { 0, 0, 0, 0, 0, 0, 0 };
volatile bool inIpi[4] = { false, false, false, false };
volatile bool inBase[4] = {true, true, true, true};
volatile bool stimOn[4] = { true, true, true, true };
volatile bool stimBegin[4] = {false, false, false, false};
volatile uint curVal[4] = { 0, 0, 0, 0 };
volatile uint chanSelect = 0;  //
const uint waveMax = 4095;     // it's a 12bit dac, so this will always be the max voltage out

// Serial coms
const byte numChars = 255;
volatile char receivedChars[numChars];
volatile bool newData = false;
volatile char msgCode;
uint param_id;
uint param_val;

// Counters
volatile uint32_t loopCount = 0;
volatile uint32_t frameCount = 0;
volatile uint32_t curFrame = 0;       // for waiting for next frame to begin trial
volatile uint32_t lastFrame = 0; // for triggering frames of a camera
volatile bool frameWaitStart = true;  // for waiting for next frame to begin trial

// objs
Adafruit_MCP4728 mcp;
IntervalTimer t1;

void setup() {

  analogReadResolution(10);

  Serial.begin(115200);  // baud rate here doesn't matter for teensy (?)
  Serial.println("Connected");

  Wire.begin();

  // Try to initialize DAC
  while (!mcp.begin(0x60)) {  // Be careful, this could be 0x60 or 0x64
    Serial.println("Failed to find MCP4728 chip");
  }
  
  Serial.println("Found MCP4728 chip");
  mcp.setChannelValue(MCP4728_CHANNEL_A, 0);
  mcp.setChannelValue(MCP4728_CHANNEL_B, 0);
  mcp.setChannelValue(MCP4728_CHANNEL_C, 0);
  mcp.setChannelValue(MCP4728_CHANNEL_D, 0);
  
  Wire.setClock(1000000);  // holy fucking shit, this must be set after mcp.begin or else it doesn't work

  // setup io
  attachInterrupt(frameChan, frameCounter, RISING);

  pinMode(valveChan1, OUTPUT);
  digitalWrite(valveChan1, LOW);

  pinMode(valveChan2, OUTPUT);
  digitalWrite(valveChan2, LOW);

  pinMode(trigChan1, OUTPUT);
  digitalWrite(trigChan1, LOW);
  
  pinMode(trigChan2, OUTPUT);
  digitalWrite(trigChan2, LOW);
  
  pinMode(trigChan3, OUTPUT);
  digitalWrite(trigChan3, LOW);
  
  pinMode(trigChan4, OUTPUT);
  digitalWrite(trigChan4, LOW);

  // start main interrupt timer program at specified sample rate
  t1.begin(ohBehave, 1E6 / Fs);

}

// functions called by timer should be short, run as quickly as
// possible, and should avoid calling other functions if possible.

void ohBehave() {

  if (State == IDLE) {  // do nothing state

  } else if (State == RESET) {  // reset main
    // this should only be called for one loop of ohBehave before reverting to state 0
    endOfTrialCleanUp();
    loopCount = 0;
    frameCount = 0;
    trialOutcome = 0;
    State = IDLE;

  } else if (State == GO) {  // GO
    goNoGo();

  } else if (State == NOGO) {  // NO-GO
    goNoGo();

  } else if (State == PAIR){ // pairing state
    pairing();

  } else if (State == TRIGGER) {  // send triggers
    fireTrig();

  } else if (State == VALVEO) {  // open valve1
    digitalWrite(valveChan1, HIGH);

  } else if (State == VALVEC) {  // close valve1
    digitalWrite(valveChan1, LOW);

  } else if (State == REWARD) {  // trigger a typical reward
    justReward();
  
  } else if (State == STIMULUS) {  // just send the stimulus
    justStim();
  
  } else if (State == EARLYLICK){ // if early licks are enforced, and there's an early lick, break the trial and reset
    dealWithEarlyLick();
  }
  
  dataReport();
  pollData();
  recvSerial();
  parseData();
  loopCount++;

}

// States

void goNoGo() {
  if (waitForNextFrame && frameWaitStart) {  // if we're waiting for the next frame to start
    curFrame = frameCount;
    frameWaitStart = false;
  } else if (!waitForNextFrame || frameCount > curFrame) { // if we aren't waiting for the next frame or it is the next frame, start the trial
    waveWrite();  // present stim
    if (!stimBegin[contingentStim]){
      if (enforceEarlyLick){
        if (lickVal==HIGH && firstLick){
          lickCount = 1;
          firstLick = false;
        } else if (lickLow < lickDebounce){
          if (lickVal==LOW){
            lickLow++;
          } else {
            lickLow = 0;
          }
        } else if (lickLow >= lickDebounce && lickVal==HIGH) {
          lickCount++;
          lickLow = 0;
        }
        if (lickCount > lickMax){
          trialOutcome = LICK;
          lickCount = 0;
          lickLow = 0;
          firstLick = true;
          State = EARLYLICK;
        }
      }      
    } else if (stimBegin[contingentStim] && !respEnd){
      if (respStart){ // as soon as the contingent stim starts, exit no lick period and begin response window                    
        respT = loopCount;
        respStart = false;
      }          
      if (!hasResponded) {    
        if ((State == GO) && (lickVal == HIGH)) {  // check for licks, if any, then HIT or FA, mark it, don't keep checking
          trialOutcome = HIT;
          hasResponded = true;
        } else if ((State == NOGO) && (lickVal == HIGH)) {  // FA
          trialOutcome = FA;
          hasResponded = true;
        }
      }
      if (loopCount - respT > respLen) {
        respEnd = true;
      }
    } else if (respEnd) {  // if stim and resp window are both over, evaluate outcome
      if (dispStart) {
        dispT = loopCount;
        dispStart = false;
      }
      if (trialOutcome == 0) {  // if there was never a response, assign miss or CW
        if (State == GO) {
          trialOutcome = MISS;
        } else if (State == NOGO) {
          trialOutcome = CW;
        }
      }
      if (loopCount - dispT > valveLen) {  // end of reward dispense         
        digitalWrite(valveChan1, LOW);
        if (consumeStart){
          consumeT = loopCount;
          consumeStart = false;
        }
        if (loopCount - consumeT > consumeLen){
          removeReward();
        }
      } else if (trialOutcome == HIT) {
        digitalWrite(valveChan1, HIGH);
      }
    }
  }
}

void justStim() {
  // state to just present the stimuli without a trial structure
  if (waitForNextFrame && frameWaitStart) {  // if we're waiting for the next frame to start
    curFrame = frameCount;
    frameWaitStart = false;
  } else if (!waitForNextFrame || frameCount > curFrame) {   
    waveWrite();  // present stim
    if (stimEnd) {  // if stim and resp window are both over, evaluate outcome
        endOfTrialCleanUp();      
    }
  }
} 

void justReward() {
  // so you can just push a button and get a typical reward
  if (dispStart) {
    dispT = loopCount;
    dispStart = false;
  }
  if (loopCount - dispT > valveLen) {  // end of reward dispense, reset things
     endOfTrialCleanUp();
  } else {
    digitalWrite(valveChan1, HIGH);
  }
}

void pairing() {
  // stim-reward pairing
  if (waitForNextFrame && frameWaitStart) {  // if we're waiting for the next frame to start
    curFrame = frameCount;
    frameWaitStart = false;
  } else if (!waitForNextFrame || frameCount > curFrame) { // if we aren't waiting for the next frame or it is the next frame, start the trial
    waveWrite();  // present stim
    if (!stimBegin[contingentStim]){
      if (enforceEarlyLick){
        if (lickVal==HIGH && firstLick){
          lickCount = 1;
          firstLick = false;
        } else if (lickLow < lickDebounce){
          if (lickVal==LOW){
            lickLow++;
          } else {
            lickLow = 0;
          }
        } else if (lickLow >= lickDebounce && lickVal==HIGH) {
          lickCount++;
          lickLow = 0;
        }
        if (lickCount > lickMax){
          trialOutcome = LICK;
          lickCount = 0;
          lickLow = 0;
          firstLick = true;
          State = EARLYLICK;
        }
      } 
    } else if (stimBegin[contingentStim] && !respEnd){
      if (respStart){ // as soon as the contingent stim starts, exit no lick period and begin response window                    
        respT = loopCount;
        respStart = false;
      }
      if (loopCount - respT > pairDelay) {
        respEnd = true;
      }
    }
    if (stimEnd && respEnd) {  // if stim and resp window are both over, evaluate outcome
      if (dispStart) {
        trialOutcome = HIT;
        dispT = loopCount;
        dispStart = false;
      }
      if (loopCount - dispT > valveLen) {  // end of reward dispens, reset things          
        digitalWrite(valveChan1, LOW); 
        if (consumeStart){
          consumeT = loopCount;
          consumeStart = false;
        }
        if (loopCount - consumeT > consumeLen){
          removeReward();
        }
      } else {
        digitalWrite(valveChan1, HIGH);
      }
    }
  }
}

// other functions

void waveWrite() {
  // waveform generator
  for (int i = 0; i < 4; i++) {  // for each dac channel
    if (stimOn[i]) {  // if there's a stim on
      if (inBase[i] && waveBase[i]>0) {  // is it in baseline?
        curVal[i] = 0;  // then output stays at 0
        BaseCntr[i]++;  // increment the baseline counter
        if (BaseCntr[i] >= waveBase[i]) {
          inBase[i] = false;  // if we've gone past the baseline period, then end it
          BaseCntr[i] = 0;    // and reset counter
        }
      } else if (inIpi[i]) {  // check if in an inter-pulse interval
        curVal[i] = 0;        // if yes, output is 0
        ipiCntr[i]++;         // increment
        if (ipiCntr[i] >= waveIPI[i]) {  // check if we're at the end of the ipi
          inIpi[i] = false;              // we're out of ipi period
          ipiCntr[i] = 0;                // reset counter
        }
      } else {  // if not in baseline or in an ipi, then we are presenting the waveform
        // asign wave value based on wave type
        stimBegin[i] = true; // mark that this particular stimulus has started
        if (waveType[i] == 0) {  // whale stim
          if (wavIncrmntr[i] % whaleStep[i] == 0) {
            curVal[i] = map(asymCos[whaleCntr[i]], 0, waveMax, 0, waveAmp[i]);
            whaleCntr[i]++;
          }
        } else if (waveType[i] == 1) {  // square wave
          curVal[i] = waveAmp[i];
        } else if (waveType[i] == 2) {  // ramp up
          curVal[i] = linspace((float)waveDur[i], 0, (float)waveAmp[i], wavIncrmntr[i]);
        } else if (waveType[i] == 3) {  // ramp down
          curVal[i] = linspace((float)waveDur[i], (float)waveAmp[i], 0, wavIncrmntr[i]);
        } else if (waveType[i] == 4) {  // pyramid
          if (wavIncrmntr[i] < waveDur[i] / 2) {
            curVal[i] = linspace((float)waveDur[i] / 2, 0, (float)waveAmp[i], wavIncrmntr[i]);
          } else {
            curVal[i] = linspace((float)waveDur[i] / 2, (float)waveAmp[i], 0, wavIncrmntr[i] - waveDur[i] / 2);
          }
        }
        wavIncrmntr[i] = wavIncrmntr[i] + 1;
      }
    }
    if (wavIncrmntr[i] >= waveDur[i]) {  // if it's the end of one wave
      if (repCntr[i] < waveReps[i] - 1) {  // but if it's not the end of the number of wave repititions
        repCntr[i] = repCntr[i] + 1;       // increment rep counter
        whaleCntr[i] = 0;
        wavIncrmntr[i] = 0;  // reset wave indexer
        inIpi[i] = true;     // go into ipi
        curVal[i] = 0;
      } else {  // else that's the end of the requested signal, so reset stuff
        repCntr[i] = 0;
        whaleCntr[i] = 0;
        wavIncrmntr[i] = 0;
        inIpi[i] = false;  // go into ipi
        stimOn[i] = false;
        curVal[i] = 0;
      }
    }
  }
  mcp.setChannelValue(MCP4728_CHANNEL_A, curVal[0]);  // send the value to the dac
  mcp.setChannelValue(MCP4728_CHANNEL_B, curVal[1]);  // send the value to the dac
  mcp.setChannelValue(MCP4728_CHANNEL_C, curVal[2]);  // send the value to the dac
  mcp.setChannelValue(MCP4728_CHANNEL_D, curVal[3]);  // send the value to the dac
  // check if all stimuli are done
  for (int i = 0; i < 4; i++) {
    if (stimOn[i]) {
      break;
    }
    stimEnd = true;
  }
}  // end waveWrite

void breakWave(){
  // for ending a stimulus early
  for (int i = 0; i < 4; i++) { // break stimulus delivery early and reset waveform parameters so they don't carry over
    stimOn[i] = false;
    stimBegin[i] = false;
    inBase[i] = false;
    BaseCntr[i] = 0; 
    repCntr[i] = 0;
    whaleCntr[i] = 0;
    wavIncrmntr[i] = 0;
    inIpi[i] = false;
    ipiCntr[i] = 0;
    curVal[i] = 0;
  }
}

void endOfTrialCleanUp(){
  // general end of trial/state reset 
  for (int i = 0; i < 4; i++) {
      stimOn[i] = true;
      stimBegin[i] = false;
      inBase[i] = true;
  }
  digitalWrite(valveChan1, LOW);
  digitalWrite(valveChan2, LOW);
  digitalWrite(trigChan1, LOW);
  digitalWrite(trigChan2, LOW);
  digitalWrite(trigChan3, LOW);
  digitalWrite(trigChan4, LOW);
  earlyStart = true;
  conStimOn = false;
  stimEnd = false;
  respEnd = false;
  hasResponded = false;
  respStart = true;
  dispStart = true;
  consumeStart = true;
  removeStart = true;
  frameWaitStart = true;
  trialOutcome = 0;
  lickCount = 0;
  lickLow = 0;
  firstLick = true;
  State = IDLE;
}

void removeReward(){
  if (removeStart) {
    removeT = loopCount;
    removeStart = false;
  } else if (loopCount - removeT > removeLen) {
    digitalWrite(valveChan2, LOW);
    endOfTrialCleanUp();
  } else if (trialOutcome == HIT) {
    digitalWrite(valveChan2, HIGH);
  }
}

void dealWithEarlyLick(){
  // interrupt trial if early lick is detected -- if we're enforcing that rule
  if (earlyStart){
    earlyT = loopCount;
    earlyStart = false;
    trialOutcome = LICK;
    breakWave();
  } else if (loopCount - earlyT > earlyLen){
    endOfTrialCleanUp();
  }
}

void fireTrig() {
  // fire a digital pulse on all trigger channels
  if (trigStart) {
    trigT = loopCount;
    trigStart = false;
  }
  if (loopCount - trigT > trigLen) {
    State = IDLE; 
    trigStart = true;
    digitalWrite(trigChan1, LOW);
    digitalWrite(trigChan2, LOW);
    digitalWrite(trigChan3, LOW);
    digitalWrite(trigChan4, LOW);
  } else {
    digitalWrite(trigChan1, HIGH);
    digitalWrite(trigChan2, HIGH);
    digitalWrite(trigChan3, HIGH);
    digitalWrite(trigChan4, HIGH);
  }
}

void pollData() {
  // get data in values
  wheelVal = analogRead(wheelChan);
  lickVal = digitalRead(lickChan);
  rewardValveVal = digitalRead(valveChan1);
  removeValveVal = digitalRead(valveChan2);
}

void dataReport() {
  // send out data over serial
  Serial.print("<");
  Serial.print(loopCount);
  Serial.print(",");
  Serial.print(frameCount);
  Serial.print(",");
  Serial.print(State);
  Serial.print(",");
  Serial.print(trialOutcome);
  Serial.print(",");
  Serial.print(curVal[0]);
  Serial.print(",");
  Serial.print(curVal[1]);
  Serial.print(",");
  Serial.print(lickVal);
  Serial.print(",");
  Serial.print(wheelVal);
  Serial.print(",");
  Serial.print(rewardValveVal);
  Serial.print(",");
  Serial.print(removeValveVal);
  Serial.print(">");
  Serial.println("");
}

void frameCounter() {
  frameCount++;
}

void recvSerial() {
  // reading in serial data with end markers "<" and ">"
  static boolean recvInProgress = false;
  static byte ndx = 0;
  const char startMarker = '<';
  const char endMarker = '>';
  volatile char rc;
  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void parseData() {  // split the data into its parts
  // parse the serial data once it is read in
  int cntr = 0;
  char *ptr;
  if (newData == true) {
    volatile char *strtokIndx;  // this is used by strtok() as an index
    strtokIndx = strtok((char *)receivedChars, ",");  // get the first part - the string
    msgCode = *strtokIndx;
    if (msgCode == 'W') {  // setting wave parameters
      ptr = strtok(NULL, ",");
      while ((ptr != NULL) && (cntr < 7)) {
        waveParams[cntr] = atoi(ptr);
        cntr++;
        ptr = strtok(NULL, ",");
      }
      // select channel
      chanSelect = waveParams[0];
      if (chanSelect > 3) {
        Serial.println("Bad channel selection, setting channel 0 to the requested values");
        chanSelect = 0;
      }
      // set wave type
      waveType[chanSelect] = waveParams[1];
      // set duration in sample points
      waveDur[chanSelect] = (volatile uint)round((waveParams[2] / 1000.0) * Fs);
      if ((waveType[chanSelect] == 0) && (waveDur[chanSelect] < SamplesNum)) {
        Serial.println("The requested duration is too short for the whalestim, setting to minimum of 25 ms");
        waveDur[chanSelect] = SamplesNum;
      } else if ((waveType[chanSelect] == 0) && (waveDur[chanSelect] % SamplesNum != 0)) {
        Serial.println("The requested duration for the whalestim must be divisible by 25, shifting duration up to next multiple of 25.");
        waveDur[chanSelect] = waveDur[chanSelect] + (waveDur[chanSelect] % SamplesNum);
      }
      // set amplitude
      waveAmp[chanSelect] = waveParams[3];
      // set interpulse interval
      waveIPI[chanSelect] = (volatile uint)round((waveParams[4] / 1000.0) * Fs);
      // set number of pulses
      waveReps[chanSelect] = waveParams[5];
      // get baseline length
      waveBase[chanSelect] = (volatile uint)round((waveParams[6] / 1000.0) * Fs);
      // this is always computed, but only used if ramping up or down
      rampStep[chanSelect] = (volatile uint)ceil((float)waveAmp[chanSelect] / waveDur[chanSelect]);
      // this is always computed, but only used if using whale stim
      whaleStep[chanSelect] = (volatile uint)ceil((float)waveDur[chanSelect] / SamplesNum);  // how quickly to step through the asymCosine
    } else if (msgCode == 'S') {  // setting State
      ptr = strtok(NULL, ",");
      State = atoi(ptr);
    } else if (msgCode == 'P'){ // setting parameters
      ptr = strtok(NULL, ",");
      param_id = atoi(ptr);
      ptr = strtok(NULL, ",");
      param_val = atoi(ptr);
      if (param_id == 1){ // enforce_lick, bool
        if (param_val == 1){
          enforceEarlyLick = true;
        } else {
          enforceEarlyLick = false;
        }
      } else if (param_id == 2){ //max lick, uint
        lickMax = param_val;
      } else if (param_id == 3){ // wait for frame, bool
        if (param_val == 1){
          waitForNextFrame = true;
        } else {
          waitForNextFrame = false;
        }
      } else if (param_id == 4){ // contingent stim index, uint
        contingentStim = param_val;
      } else if (param_id == 5){ // trigger broadcast length, 
        trigLen = (volatile uint)round((param_val / 1000.0) * Fs);
      } else if (param_id == 6){ // response window length
        respLen = (volatile uint)round((param_val / 1000.0) * Fs); 
      } else if (param_id == 7){ // how long to open reward valve
        valveLen = (volatile uint)round((param_val / 1000.0) * Fs);
      } else if (param_id == 8){ // how long from reward administration does the animal have to consume the reward
        consumeLen = (volatile uint)round((param_val / 1000.0) * Fs);
      } else if (param_id == 9){ // in pairing trials, time between stim and reward
        pairDelay = (volatile uint)round((param_val / 1000.0) * Fs);
      } else if (param_id == 10){ // how long to broadcast early lick
        earlyLen = (volatile uint)round((param_val / 1000.0) * Fs);
      } else if (param_id == 11){ // how long to open remove reward valve for
        removeLen = (volatile uint)round((param_val / 1000.0) * Fs);
      } 
    }
    newData = false;
  }
}

int linspace(float const n, float const d1, float const d2, int const i) {
  float n1 = n - 1;
  return round(d1 + (i) * (d2 - d1) / n1);
}

void loop() {
}
