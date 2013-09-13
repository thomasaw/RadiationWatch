//////////////////////////////////////////////////
// Radiation-Watch.org
// URL http://www.radiation-watch.org/
//////////////////////////////////////////////////

#include "Arduino.h"
#include "RadiationWatch.h"

int signCount;  //Counter for Radiation Pulse

//Called via Interrupt when a radiation pulse is detected
void triggerRadiationPulse() {
  signCount++;
  tone(8, 800, 1); // Output classic geiger counter tick noise
}

RadiationWatch::RadiationWatch(int signPin, int noisePin, int signIRQ) : _signPin(signPin), _noisePin(noisePin), _signIRQ(signIRQ)
{
  signCount = 0;
  _prevTime = 0;
  index = 0;
  
  noiseCount = 0;
  
  sON = 0;
  nON = 0;
  
  _cpm = 0;
  cpmIndex = 0;
  cpmIndexPrev = 0;
  
  totalSec = 0;
  totalHour = 0;
  
  cpmTimeMSec = 0;
  cpmTimeSec = 0;
}

void RadiationWatch::setup()
{
  //PIN setting for Radiation Pulse
  pinMode(_signPin,INPUT);
  digitalWrite(_signPin,HIGH);

  //PIN setting for Noise Pulse
  pinMode(_noisePin,INPUT);
  digitalWrite(_noisePin,HIGH);
  
  //Attach interrupt handler to catch incoming radiation pulses, 
  //and execute triggerRadiationPulse() when this happens.
  attachInterrupt(_signIRQ, triggerRadiationPulse, FALLING);

  //Initialize cpmHistory[]
  for(int i = 0; i < kHistoryCount;i++ )
  {
    _cpmHistory[i] = 0;
  }
  
  _prevTime = millis();
}

int RadiationWatch::signPin()
{
  return digitalRead(_signPin);
}

int RadiationWatch::noisePin()
{
  return digitalRead(_noisePin);
}

void RadiationWatch::loop()
{
  // Raw data of Radiation Pulse: Not-detected -> High, Detected -> Low
  int sign = signPin();
  // Raw data of Noise Pulse: Not-detected -> Low, Detected -> High
  int noise = noisePin();

  //Noise Pulse normally keeps high for about 100[usec]
  if(noise==1 && nON==0)
  {//Deactivate Noise Pulse counting for a while
    nON = 1;
    noiseCount++;
  }else if(noise==0 && nON==1){
    nON = 0;
  }

  //Output readings to serial port, after 10000 loops
  if(index==10000) //About 160-170 msec in Arduino Nano(ATmega328)
  {
    //Get current time
    int currTime = millis();

    //No noise detected in 10000 loops
    if(noiseCount == 0)
    {
      //Shift an array for counting log for each 6 sec.
      if( totalSec % 6 == 0 && cpmIndexPrev != totalSec)
      {
        cpmIndexPrev = totalSec;
        cpmIndex++;
        
        if(cpmIndex >= kHistoryCount)
        {
          cpmIndex = 0;
        }
        
        if(_cpmHistory[cpmIndex] > 0)
        {
          _cpm -= _cpmHistory[cpmIndex];
        }
        _cpmHistory[cpmIndex] = 0;
      }
      
      //Store count log
      _cpmHistory[cpmIndex] += signCount;
      //Add number of counts
      _cpm += signCount;
      
      //Get ready time for 10000 loops
      cpmTimeMSec += abs(currTime - _prevTime);
      //Transform from msec. to sec. (to prevent overflow)
      if(cpmTimeMSec >= 1000)
      {
        cpmTimeMSec -= 1000;
        //Add measurement time to calcurate cpm readings (max=20min.)
        if( cpmTimeSec >= 20*60 )
        {
          cpmTimeSec = 20*60;
        }else{
          cpmTimeSec++;
        }
        
        //Total measurement time
        totalSec++;
        //Transform from sec. to hour. (to prevent overflow)
        const int kSecondsInHour = 60 * 60;
        if(totalSec >= kSecondsInHour)
        {
          totalSec -= kSecondsInHour;
          totalHour++;
        }
      }
      
      printStatus();
      
      index=0;
    }
    
    //Initialization for next 10000 loops
    _prevTime = currTime;
    signCount = 0;
    noiseCount = 0;
  }
  
  index++;
}

void RadiationWatch::printKey()
{
}

void RadiationWatch::printStatus()
{
}

boolean RadiationWatch::isAvailable()
{
  return cpmTime() != 0;
}

double RadiationWatch::cpmTime()
{
  return cpmTimeSec / 60.0;
}

double RadiationWatch::cpm()
{
  double min = cpmTime();
  
  if (min != 0) {
    return _cpm / min;
  }
  else {
    return 0;
  }
}

static const double kAlpha = 53.032; // cpm = uSv x alpha

double RadiationWatch::uSvh()
{
  return cpm() / kAlpha;
}

double RadiationWatch::uSvhError()
{
  double min = cpmTime();
  
  if (min != 0) {
    return sqrt(_cpm) / min / kAlpha;
  }
  else {
    return 0;
  }
}

RadiationWatchPrinter::RadiationWatchPrinter(int signPin, int noisePin, int signIRQ) : RadiationWatch(signPin, noisePin, signIRQ)
{
}

void RadiationWatchPrinter::printKey()
{
  //CSV-formatting for serial output (substitute , for _)
  Serial.println("hour[h]_sec[s]_count_cpm_uSv/h_uSv/hError");
}

void RadiationWatchPrinter::printStatus()
{
  char msg[256]; //Message buffer for serial output
  //String buffers of float values for serial output
  char cpmBuff[20];
  char uSvBuff[20];
  char uSvdBuff[20];
    
  //Elapsed time of measurement (max=20min.)
  double min = cpmTime();
  dtostrf(cpm(), -1, 3, cpmBuff);
  dtostrf(uSvh(), -1, 3, uSvBuff);  // uSv/h
  dtostrf(uSvhError(), -1, 3, uSvdBuff);  // error of uSv/h
    
  //Create message for serial port
  sprintf(msg, "%d,%d.%03d,%d,%s,%s,%s",
    totalHour,totalSec,
    cpmTimeMSec,
    signCount,
    cpmBuff,
    uSvBuff,
    uSvdBuff
    );
    
  //Send message to serial port
  Serial.println(msg);
}


