/* Todo
  -Use a watchdog timer to make sure the controller doesn't lock up.
  - check on increaed power to heater without increased temp. (bad thermistor or bad heater)
  -c heck decreased power with increase in temp. (SSR stuck on?)

*/

#include <LiquidCrystal_I2C.h>

//include libraries
#include "ClickEncoder.h"
#include "TimerOne.h"
#include "PID_v1.h"
#include "Thermistor.h"
#include "Configuration.h"
#include "Heater.h"



//Create the configuration
//Configuration configuration;


ClickEncoder *encoder;

//process variables
int setTemp;
int actTemp;
int heaterDS;
int fanDS;


//encoder variables
int16_t last, value;
ClickEncoder::Button bState;

//LCD and update timing
//LCD
LiquidCrystal_I2C lcd(0x3F, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

//timing
unsigned long now;
unsigned long updateTime;
int updateInterval = 1000;
unsigned long setTempStartTime;//The time at which the temperature is set using the menu system. Used to keep track of heating time out
unsigned long outOfBandStartTime;// The time at which a the temperature has gone out of the allowed temperature band (Set temp +or- allowed deviation)
unsigned long tempChangeStartTime;//The time at which a temperature change interval is initiated.

//Safety
boolean heating = false;// true if the heater is set to a temperature above it's current actual temperature.
boolean cooling = false;//true if the heater is set to a temperature below it's current actual temperature.
boolean outOfBand = false; //true if the temperature is outside of the maximum allowed deviation.
boolean tempRising = false; //true if the temperature has risen over the previous interval for which it was checked.
int previousTemp; //The previous temperature used to determine if the temperature is rising or falling

//create the heater
double setTempD;
Heater heater(&setTempD);

//fan
int fanState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
long fanInterval = 0;
long interval;

//Setup State Machine

//States enum
enum States {
  DISPLAY_MENU,
  CHANGE_SET_TEMP,
  CHANGE_HEATER_DS,
  CHANGE_FAN_DS,
  SAVE_SETTINGS,
  SAFETY_SHUTDOWN
} ;

States currentState;
boolean stateChanged = false;

//Declare state functions
void displayMenu();
void changeSetTemp();
void changeHeaterDS();
void changeFanDS();
void saveSettings();
void safetyShutdown();

//Pointers to State functions
void (*state_table[])() = {
  displayMenu,
  changeSetTemp,
  changeHeaterDS,
  changeFanDS,
  saveSettings,
  safetyShutdown
};

//Menu Setup
struct MenuItem {
  String text;
  int* value;
  States state;
};

int selectedItem = 0;
int topItem = 0;
const int numMenuItems = 4;
struct MenuItem menu[numMenuItems] = {
  {"Set Temp", &setTemp, CHANGE_SET_TEMP},
  {"Act Temp", &actTemp, DISPLAY_MENU},
  {"Heater DS", &heaterDS, CHANGE_HEATER_DS},
  {"Fan DS", &fanDS, CHANGE_FAN_DS},
  //  {"Save Settings", NULL, SAVE_SETTINGS}
};

void timerIsr() {
  encoder->service();
}

void setup() {
  Serial.begin(250000);
  //Set initial state
  currentState = DISPLAY_MENU;

  lcd.init();                      // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();

  //Start LCD
  lcd.setCursor(0, 0);
  lcd.print("Drying hopper");
  lcd.setCursor(0, 1);
  lcd.print("v0.2");
  delay(1000);
  lcd.clear();

  //Encoder
  encoder = new ClickEncoder(A1, A0, A2);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  //fan
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  heater.setMode(MANUAL);
  heater.setDutyCycle(0);

  //Print initial values to LCD
  drawMenu();
}

void loop() {
  //Do tasks that must be done on each loop
  //Safety check
  safetyCheck();

  //run fan

  //activate heaters
  heater.activate();
  actTemp = heater.getTemp();


  //update the menu if the updateTime has been reached.
  //This keeps the displayed temperatures and duty cycles up to date
  now = millis();
  if (now >= updateTime && currentState == DISPLAY_MENU) {
    updateTime = now + updateInterval;
    heaterDS = heater.getDutyCycle();
    updateMenu();
    Serial.print("Heating = ");
    Serial.println(heating);
    Serial.print("Cooling = ");
    Serial.println(cooling);
    Serial.print("outOfBand = ");
    Serial.println(outOfBand);
    Serial.print("tempRising = ");
    Serial.println(tempRising);
    Serial.print("preTemp-actTemp = ");
    Serial.println(previousTemp - actTemp);
    Serial.println();
    Serial.println();
    //drawMenu();
  }

  //Call function of the current state
  state_table[currentState]();
}

void displayMenu() {
  value = value + encoder->getValue();
  bState = encoder->getButton();

  //If the button has been clicked, switch to selected item's state
  if (bState == ClickEncoder::Clicked) {
    currentState = menu[selectedItem].state;
    stateChanged = true;
    return;
  }

  //If the encoder has been turned, advance the menu
  int encoderThreshold = 3;

  if (value > -encoderThreshold && value < encoderThreshold) {
    return;
  } else if (value > 0) {
    menuDown();
    value = 0;
    drawMenu();
  } else {
    menuUp();
    value = 0;
    drawMenu();
  }

  //drawMenu();
}

void drawMenu() {

  //draw the menu
  lcd.clear();
  if (topItem == selectedItem) {
#ifdef SERIAL_PRINT_MENU
    Serial.print(">");
#endif
    lcd.setCursor(0, 0);
    lcd.print (">");

  } else {
#ifdef SERIAL_PRINT_MENU
    Serial.print(" ");
#endif
    lcd.setCursor(1, 0);

  }
  lcd.print(menu[topItem].text);
  if (menu[topItem].value != NULL) {
    lcd.setCursor(16 - getDigits(*menu[topItem].value), 0);
    lcd.print(*menu[topItem].value);
  }
#ifdef SERIAL_PRINT_MENU
  Serial.print(menu[topItem].text);
  Serial.print("\t");
  Serial.println(getDigits(*menu[topItem].value));
#endif

  if (topItem + 1 == selectedItem) {
#ifdef SERIAL_PRINT_MENU
    Serial.print(">");
#endif

    lcd.setCursor(0, 1);
    lcd.print (">");
  } else {
#ifdef SERIAL_PRINT_MENU
    Serial.print(" ");
#endif

    lcd.setCursor(1, 1);
  }
#ifdef SERIAL_PRINT_MENU
  Serial.print(menu[topItem + 1].text);
  Serial.print("\t");
  Serial.println(getDigits(*menu[topItem + 1].value));
  Serial.println();
#endif

  lcd.print(menu[topItem + 1].text);
  if (menu[topItem + 1].value != NULL) {
    lcd.setCursor(16 - getDigits(*menu[topItem + 1].value), 1);
    lcd.print(*menu[topItem + 1].value);
  }
}

void updateMenu() {
  //Update the values being displayed without rewriting the entire lcd screen.

  if (menu[topItem].value != NULL) {
    lcd.setCursor(16 - getDigits(*menu[topItem].value) - 1, 0); //The extra -1 is for a space that fixes changes between values with different numbers of digits
    lcd.print(" ");
    lcd.print(*menu[topItem].value);
  }

  if (menu[topItem + 1].value != NULL) {
    lcd.setCursor(16 - getDigits(*menu[topItem + 1].value) - 1 , 1);
    lcd.print(" ");
    lcd.print(*menu[topItem + 1].value);
  }
}

void menuUp() {
  if (selectedItem > 0) {
    if (topItem == selectedItem) {
      topItem--;
    }
    selectedItem--;
  }
}

void menuDown() {
  if (selectedItem < numMenuItems - 1) {
    if (topItem != selectedItem) {
      topItem++;
    }
    selectedItem++;
  }
}

//getDigits figures out how many digits are in a positive integer
int getDigits(int val) {
  if (val == 0) return 1;
  int count = 0;
  while (val != 0)
  {
    // n = n/10
    val /= 10;
    ++count;
  }
  return count;
}


void changeSetTemp() {
  static int prevTemp;

  //If the state has just been changed to changeSetTemp, set up the screen
  if (stateChanged) {
    stateChanged = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(menu[selectedItem].text);
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
  }

  bState = encoder->getButton();

  //If the button has been clicked, switch to selected item's state
  if (bState == ClickEncoder::Clicked) {
    currentState = DISPLAY_MENU;
    heater.setMode(AUTOMATIC);
    setTempD = (double)setTemp; //store the set temp in the double used by the PID controller

    if (setTemp > actTemp) {//Set safety variables for heating
      setTempStartTime = tempChangeStartTime = millis();
      heating = true;
      cooling = false;
      outOfBand = false; //not out of band durring heat up or cool down
    } else if (setTemp < actTemp) {//Set safety variables for cooling.
      setTempStartTime = tempChangeStartTime =  millis();
      cooling = true;
      heating = false;
      outOfBand = false; //not out of band durring heat up or cool down
    }

    drawMenu();
    return;
  }

  setTemp += encoder->getValue();
  if (setTemp < MIN_SET_TEMP) {
    setTemp = MIN_SET_TEMP;
  }

  if (setTemp > MAX_SET_TEMP) {
    setTemp = MAX_SET_TEMP;
  }
  if (setTemp != prevTemp) {
    lcd.setCursor(6, 1);
    lcd.print("   ");
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
    prevTemp = setTemp;
  }

}

void changeHeaterDS() {
  static int prevHeaterDS;

  //If the state has just been changed to changeSetTemp, set up the screen
  if (stateChanged) {
    stateChanged = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(menu[selectedItem].text);
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
  }
  bState = encoder->getButton();

  //If the button has been clicked, switch to selected item's state
  if (bState == ClickEncoder::Clicked) {
    heater.setMode(MANUAL);
    setTemp = 0;
    setTempD = 0;
    heater.setDutyCycle(heaterDS);

    //Unset heating and cooling flags b/c duty cycle doesn't specify temperature and has no temperature control
    heating = false;
    cooling = false;
    currentState = DISPLAY_MENU;
    drawMenu();
    return;
  }

  heaterDS += encoder->getValue();
  if (heaterDS < MIN_DUTY_CYCLE) {
    heaterDS = MIN_DUTY_CYCLE;
  }

  if (heaterDS > MAX_DUTY_CYCLE) {
    heaterDS = MAX_DUTY_CYCLE;
  }
  if (heaterDS != prevHeaterDS) {
#ifdef SERIAL_PRINT_MENU
    Serial.println(heaterDS);
#endif

    lcd.setCursor(6, 1);
    lcd.print("   ");
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
    prevHeaterDS = heaterDS;
  }

}

void changeFanDS() {
  static int prevDS;

  //If the state has just been changed to changeSetTemp, set up the screen
  if (stateChanged) {
    stateChanged = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(menu[selectedItem].text);
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
  }

  bState = encoder->getButton();

  //If the button has been clicked, switch to selected item's state
  if (bState == ClickEncoder::Clicked) {
    fanInterval = fanDS * 20;
    if (fanDS == 0) {
      //      digitalWrite(FAN_PIN, LOW);

    }
    analogWrite(FAN_PIN, fanDS);
    currentState = DISPLAY_MENU;
    drawMenu();
    return;
  }

  fanDS += encoder->getValue();
  if (fanDS < FAN_MIN_DS) {
    fanDS = FAN_MIN_DS;
  }

  if (fanDS > FAN_MAX_DS) {
    fanDS = FAN_MAX_DS;
  }
  if (fanDS != prevDS) {
    lcd.setCursor(6, 1);
    lcd.print("   ");
    lcd.setCursor(9 - getDigits(*menu[selectedItem].value), 1);
    lcd.print(*menu[selectedItem].value);
    prevDS = fanDS;
  }

}


void saveSettings() {
#ifdef SERIAL_PRINT_MENU
  Serial.println("Saving Settings!");
  Serial.println();
#endif

  currentState = DISPLAY_MENU;
  drawMenu();
}

void safetyCheck() {
  //Check for over temperature
  if (actTemp > MAX_ALLOWABLE_TEMP) {
    currentState = SAFETY_SHUTDOWN;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAFETY SHUTDOWN!");
    lcd.setCursor(0, 1);
    lcd.print("OVER TEMP");
    return;
  }

  //Check for disconnected Thermistor

  // Check that set temp has been reached in allowed time
  if (actTemp < 0) {
    currentState = SAFETY_SHUTDOWN;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAFETY SHUTDOWN!");
    lcd.setCursor(0, 1);
    lcd.print("THERMISTOR ERROR");
    return;
  }
  //Check that heating has completed
  if (heating && actTemp > setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP) {
    heating = false;
  }

  //Check that cooling has completed
  if (cooling && actTemp < setTemp + MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP) {
    cooling = false;
  }
  //Check that heating has completed
  if (actTemp > setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP && heating) {
    heating = false;
  }

  //Determine if the temperature is out of the allowed temp band. If so, record the start time.
  if (!heating
      && !cooling
      && heater.getMode() == AUTOMATIC
      &&  (actTemp > setTemp + MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP || actTemp < setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP)) {
    if (!outOfBand) {
      outOfBand = true;
      outOfBandStartTime = millis();
    }
  }

  //Unset outOfBand flag if the temp is in the allowed band.
  if (!heating
      && !cooling
      && heater.getMode() == AUTOMATIC
      && actTemp < setTemp + MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP
      && actTemp > setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP) {
    outOfBand = false;
  }

  //Check to see if there has been a heater error due to the temperature being outside of the allowed band for too long.
  if (!heating
      && !cooling
      && heater.getMode() == AUTOMATIC
      &&  (actTemp > setTemp + MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP || actTemp < setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP)
      && millis() - outOfBandStartTime  > MAX_TIME_OUTSIDE_ALLOWABLE_DEVIATION * 1000L) {
    currentState = SAFETY_SHUTDOWN;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAFETY SHUTDOWN!");
    lcd.setCursor(0, 1);
    lcd.print("HEATING ERROR");
    return;
  }

  //Check for heat up timeout.
  if (heating && actTemp < setTemp - MAX_ALLOWABLE_DEVIATION_FROM_SET_TEMP && (millis() - setTempStartTime) > MAX_TIME_TO_REACH_SET_TEMP * 1000L) {
    currentState = SAFETY_SHUTDOWN;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAFETY SHUTDOWN!");
    lcd.setCursor(0, 1);
    lcd.print("HEATUP TIMEOUT");
    return;
  }

  //update tempRising
  if (now - tempChangeStartTime > TEMP_CHANGE_CHECK_INTERVAL * 1000L) {
    if (previousTemp - actTemp >= 0) {
      tempRising = false;
    } else {
      tempRising = true;
    }
    previousTemp = actTemp;
    tempChangeStartTime = now;

    //Check to see that the temperature is decreasing if the set temp is sufficiently below the actual temp.
    //This error may mean that a heater is stuck on. A dangerous situation that the microcontroller may not fix unless it is equiped with a way to cut power to the heater.
    if (cooling
        && tempRising
        && actTemp >= MIN_TEMP_FOR_FAST_COOLING) {
      currentState = SAFETY_SHUTDOWN;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SAFETY SHUTDOWN!");
      lcd.setCursor(0, 1);
      lcd.print("COOLING ERROR");
      return;
    }
  }

}

void safetyShutdown() {
  heater.setMode(MANUAL);
  digitalWrite(HEATER_PIN, LOW);
  while (true) {
    ;
  }

}


