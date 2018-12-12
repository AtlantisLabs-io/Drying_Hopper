/*
  Heater.cpp - Library for controlling the heaters.
  The heater library can control heaters using PWM (time base set to 0) or
  by using a relay in which case the Time base is set to a larger value (maybe 2000).

  Created by Matthew P. Rogge, Nov 24, 2015.
  Released into the public domain.

*/

#include "Arduino.h"
#include "Heater.h"

Heater::Heater(double* setTemp)
  : _thermistor(THERMISTOR_PIN,
                R_NOMINAL,
                T_NOMINAL,
                NUM_THERMISTOR_SAMPLES,
                B_COEFFICIENT,
                SERIES_RESISTOR),
    _pid( &_thermistor.temp,
          &_dutyCycle,
          setTemp,
          KP,
          KI,
          KD,
          DIRECT)
{
  _timeBase = TIME_BASE;
  _temp = &_thermistor.temp;
  _pid.SetSampleTime(_timeBase);
  _pid.SetTunings(KP, KI, KD);
  _pid.SetOutputLimits(MIN_DUTY_CYCLE, MAX_DUTY_CYCLE);
  _heaterPin = HEATER_PIN;
  pinMode(HEATER_PIN, OUTPUT);
  _PWM = PWM;
  _maxDutyCycle = MAX_DUTY_CYCLE;
  _minDutyCycle = MIN_DUTY_CYCLE;
  _pid.SetMode(AUTOMATIC);

}

void Heater::activate()
{
  //Get the time
  _now = millis();
  _thermistor.sampleTemp();//Always sample temp so that the PID gets a good input

  //If one timebase has passed start over. Get the new PID setting and turn the relay on.
  if (_now >= (_timeBase + _startTime)) {
    _startTime = _now;
    _temp = &_thermistor.temp;
    _pid.Compute();

    if (!_PWM) { //IE, PWM isn't being used so use the time base as the duty cycle period.
      // Determine the length of time that the relay should be on

      _durration = ((_timeBase * _dutyCycle) / 100L);

      if (_dutyCycle > 1.0) { // Only turn on if duty cycle is greater than 1
        digitalWrite(_heaterPin , HIGH);//Turn the heater on.
      }

    } else {//Using PWM, so just set the PWM on the heater pin
      analogWrite(_heaterPin, _dutyCycle);

    }
  }

  //If the relay should now be off, turn it off
  if (!_PWM && digitalRead(_heaterPin) && _now >= _startTime + _durration) {
    digitalWrite(_heaterPin, LOW);//Turn the heater off
  }
}

// sampleTemp is used for states where the heater should be inactive.
void Heater::sampleTemp()
{
  _thermistor.sampleTemp();//Always sample temp so that the PID gets a good input
}

void Heater::setDutyCycle(float dutyCycle)
{
  if (_pid.GetMode() == AUTOMATIC) { // if in Automatic switch to manual
    _pid.SetMode(MANUAL);
  }
  //Validate the duty cycle
  if (dutyCycle < _minDutyCycle) {
    dutyCycle = _minDutyCycle;
  }
  if (dutyCycle > _maxDutyCycle) {
    dutyCycle = _maxDutyCycle;
  }

  _dutyCycle = dutyCycle;
}

double Heater::getDutyCycle() {
  return _dutyCycle;
}

int Heater::getRaw() {
  return _thermistor.getADC();
}

void Heater::off()
{
  _pid.SetMode(MANUAL);
  _dutyCycle = 0.0;
  digitalWrite(_heaterPin, LOW);//turn the relay off
}

void Heater::setMode(int mode)
{
  _pid.SetMode(mode);
}

int Heater::getMode()
{
  return _pid.GetMode();
}
float Heater::getTemp() {
  return _thermistor.temp;
}
