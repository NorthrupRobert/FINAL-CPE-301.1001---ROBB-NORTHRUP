#include <LiquidCrystal.h>
#include <Stepper.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>

// Pin definitions
#define WATER_SENSOR_PIN A15
#define FAN_PIN 26
#define FAN_ENABLE_PIN 28
#define STEPPER_PIN1 8
#define STEPPER_PIN2 10
#define STEPPER_PIN3 9
#define STEPPER_PIN4 11
#define RESET_BUTTON_PIN 5
#define START_BUTTON_PIN 6
#define STOP_BUTTON_PIN 7
#define MOTOR_UP_PIN 4
#define MOTOR_DOWN_PIN 3
#define DHTPIN A3
#define LED_RED_PIN 50
#define LED_GREEN_PIN 46
#define LED_BLUE_PIN 44
#define LED_YELLOW_PIN 48

// Constants
#define WATER_THRESHOLD 25 // percentage, water in res before ERROR state
#define TEMPERATURE_THRESHOLD 25 // Celsius, temp to engage RUNNING state (above)

// Stepper motor configuration
const int stepsPerRevolution = 2048; // per my stepper motor
#define ANGLE_PER_CYCLE 15 //change accordingly
const int stepsPerCycle = (stepsPerRevolution / 360)*ANGLE_PER_CYCLE;

// Initialize objects
LiquidCrystal lcd(35, 37, 39, 41, 43, 45); // LCD pins
Stepper stepper(stepsPerRevolution, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4); // this is for that stepper motor
DHT dht(DHTPIN, DHT11); // DHT11 sensor
RTC_DS3231 rtc; // Real-time clock module used

// Function Initialization
void moveStepMotor();
void measureTempHum();

// Define states
enum State {
  DISABLED,
  IDLE,
  ERROR_STATE,
  RUNNING
};

// VARIOUS VARIABLES
State currentState = DISABLED;
int lastMinUpdate;
float temperature, humidity;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  
  // Initialize LCD
  lcd.begin(16, 2);
  
  // Initialize RTC
  rtc.begin();

  // checking to make sure the rtc module is working properly . . .
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Initialize stepper motor
  stepper.setSpeed(12); //rpm

  dht.begin();

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  lastMinUpdate = (int)rtc.now().minute();
  
  // Initialize pins
  pinMode(DHTPIN, INPUT_PULLUP); // Manually enable pull-up resistor, KEEP THIS PULLED UP!!!!
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_UP_PIN, INPUT_PULLUP);
  pinMode(MOTOR_DOWN_PIN, INPUT_PULLUP);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(FAN_ENABLE_PIN, OUTPUT);
  
  // Turn off all LEDs initially
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);

  reportStateTransition("DISABLED"); // inital state for the system
}

void loop() {
  DateTime now = rtc.now();
  // Serial.print(now.hour(), DEC);
  // Serial.print(':');
  // Serial.print(now.minute(), DEC);
  // Serial.print(':');
  // Serial.println(now.second(), DEC);

  // Serial.print("State: "); Serial.print(currentState);
  // Serial.print(" // Temp:"); Serial.print(dht.readTemperature());
  // Serial.print(" // Humidity:"); Serial.print(dht.readHumidity());
  // Serial.print(" // Water Level:"); Serial.print(checkWaterLevel());
  // Serial.print(" // Start:"); Serial.print(digitalRead(START_BUTTON_PIN));
  // Serial.print(" // Stop:"); Serial.print(digitalRead(STOP_BUTTON_PIN));
  // Serial.print(" // Reset:"); Serial.print(digitalRead(STOP_BUTTON_PIN));
  // Serial.print(" // MUP:"); Serial.print(digitalRead(MOTOR_UP_PIN));
  // Serial.print(" // MDN:"); Serial.print(digitalRead(MOTOR_DOWN_PIN));
  // Serial.print(" // Minute:"); Serial.print((int)now.minute());
  // Serial.print(" // Update:"); Serial.print(lastMinUpdate);
  // Serial.print("\n");

    if(currentState == DISABLED) {
      // start button press?
      lcd.clear();
      digitalWrite(LED_YELLOW_PIN, HIGH); // Yellow LED ON
      digitalWrite(LED_GREEN_PIN, LOW); // Green LED OFF
      if (digitalRead(START_BUTTON_PIN) == HIGH) {
        currentState = IDLE;
        digitalWrite(LED_YELLOW_PIN, LOW); // Yellow LED OFF
        updateLCD();
        reportStateTransition("IDLE");
      }
    }
    else if(currentState == IDLE) {
      //Serial.print("&&&&&&&&&&&&&&&");
      measureTempHum();
      digitalWrite(LED_GREEN_PIN, HIGH);
      digitalWrite(LED_BLUE_PIN, LOW);

      // temp + hum monitoring
      if ((lastMinUpdate < (int)now.minute()) || ((int)(now.minute() == 0) && (lastMinUpdate != 0))) { //update ONLY every minute
        updateLCD();
      }

      // Keep water in res at or above threshold
      if (checkWaterLevel() < WATER_THRESHOLD) {
        currentState = ERROR_STATE;
        reportStateTransition("ERROR");
      }
      
      // Stop button?
      else if (digitalRead(STOP_BUTTON_PIN) == HIGH) {
        currentState = DISABLED;
        reportStateTransition("DISABLED");
      }

      // high temp -> RUNNING state
      else if (temperature > TEMPERATURE_THRESHOLD) {
        currentState = RUNNING;
        reportStateTransition("RUNNING");
        updateLCD();
      }

      //Serial.print("->->->->->->");
      moveStepMotor(); //yuh
    }
    else if(currentState == ERROR_STATE) {
      // Error message to LCD that water level is low (DO NOT MONITOR TEMP + HUMIDITY)
      lcd.clear();
      lcd.print("WATER LEVEL IS");
      lcd.setCursor(0, 1);
      lcd.print("TOO LOW");
      digitalWrite(LED_RED_PIN, HIGH);
      digitalWrite(LED_BLUE_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, LOW);
      
      // Rest system to IDLE if water level is okay, and reset button pressed
      if ((checkWaterLevel() >= WATER_THRESHOLD) && (digitalRead(RESET_BUTTON_PIN)  == HIGH )) {
        currentState = IDLE;
        digitalWrite(LED_RED_PIN, LOW); // Red LED OFF
        reportStateTransition("IDLE");
      }
      
      // Stop button?
      else if (digitalRead(STOP_BUTTON_PIN) == HIGH) {
        currentState = DISABLED;
        digitalWrite(LED_RED_PIN, LOW); // Red LED OFF
        reportStateTransition("DISABLED");
      }

      //Serial.print("->->->->->->");
      moveStepMotor();
    }
    else if (currentState == RUNNING) {
      //Serial.print("WEARERUNNINGWEARERUNNING");
      updateLCD();
      measureTempHum();
      digitalWrite(LED_BLUE_PIN, HIGH);
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(FAN_ENABLE_PIN, HIGH);
      digitalWrite(FAN_PIN, HIGH);

      // Temp + hum monitoring
      if ((lastMinUpdate < (int)now.minute()) || ((int)(now.minute() == 0) && (lastMinUpdate != 0))) { //update ONLY every minute
        updateLCD();
      }

      // monitor water level!
      if (checkWaterLevel() < WATER_THRESHOLD) {
        currentState = ERROR_STATE;
        digitalWrite(LED_BLUE_PIN, LOW); // Blue LED OFF
        digitalWrite(FAN_ENABLE_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
        Serial.print("TURNING OFF FAN");
        Serial.print(" at ");
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.println(now.second(), DEC);
        Serial.print("\n");
        reportStateTransition("ERROR");
      }

      // stop button pressed??
      else if (digitalRead(STOP_BUTTON_PIN) == HIGH) {
        currentState = DISABLED;
        digitalWrite(LED_BLUE_PIN, LOW); // Blue LED OFF
        digitalWrite(FAN_ENABLE_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
        Serial.print("TURNING OFF FAN");
        Serial.print(" at ");
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.println(now.second(), DEC);
        Serial.print("\n");
        reportStateTransition("DISABLED");
      }
      //if temp low -> IDLE
      else if ((!isnan(temperature)) && (temperature <= TEMPERATURE_THRESHOLD)) {
        digitalWrite(LED_BLUE_PIN, LOW); // Blue LED OFF
        digitalWrite(FAN_ENABLE_PIN, LOW);
        digitalWrite(FAN_PIN, LOW);
        Serial.print("TURNING OFF FAN");
        Serial.print(" at ");
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.println(now.second(), DEC);
        Serial.print("\n");
        currentState = IDLE;
        reportStateTransition("IDLE");
      }
      //Serial.print("->->->->->->");
      moveStepMotor();
    }
  
  //delay(1000); //Delay to prevent rapid state changes
}

float checkWaterLevel() {
  // Read water level sensor
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  float waterLevel = map(sensorValue, 0, 511, 0, 100);
  return waterLevel;
}

void moveStepMotor() {
  //Serial.print("@@@@@@@@@@@@");
  if(digitalRead(MOTOR_UP_PIN) == 1) {
    Serial.print("MOVING STEPPER MOTOR CLKWISE "); Serial.print(ANGLE_PER_CYCLE); Serial.print(" DEGREES\n");
    stepper.step(stepsPerCycle);
  }
  else if(digitalRead(MOTOR_DOWN_PIN) == 1) {
    Serial.print("MOVING STEPPER MOTOR CNTR CLKWISE "); Serial.print(ANGLE_PER_CYCLE); Serial.print(" DEGREES\n");
    stepper.step(-stepsPerCycle);
  }
}

void updateLCD() {
  DateTime now = rtc.now();
  //if ((lastMinUpdate < now.minute()) || ((now.minute() == 0) && (lastMinUpdate != 0))) { //update ONLY every minute
    lcd.clear();
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print("%");

    lastMinUpdate = now.minute(); //update last... update
  //}
}

void measureTempHum() {
  if (!isnan(temperature) && !isnan(humidity)) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
  }
}

void reportStateTransition(String state) {
  DateTime now = rtc.now();
  Serial.print("State changed to ");
  Serial.print(state);
  Serial.print(" at ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  //Report turning on fan, if we just moved to RUNNING state . . .
  if (state == "RUNNING") {
    Serial.print("TURNING ON FAN");
    Serial.print(" at ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.println(now.second(), DEC);
  }
}

