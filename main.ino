#include <LiquidCrystal.h>
#include <Stepper.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>

// UART Registers for Arduino Mega 2560
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *)0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

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
const int stepsPerCycle = (stepsPerRevolution / 360) * ANGLE_PER_CYCLE;

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
  // Initialize UART communication
  U0init(9600);

  // Initialize LCD
  lcd.begin(16, 2);

  // Initialize RTC
  rtc.begin();

  // checking to make sure the rtc module is working properly . . .
  Wire.begin();
  if (!rtc.begin()) {
    U0putstring("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    U0putstring("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize stepper motor
  stepper.setSpeed(12); //rpm

  dht.begin();

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  lastMinUpdate = (int)rtc.now().minute();

  // Initialize pins
  DDRK &= ~(1 << 7); // DHTPIN as input
  DDRF &= ~(1 << 7); // WATER_SENSOR_PIN as input
  DDRF &= ~(1 << 5); // RESET_BUTTON_PIN as input
  DDRF &= ~(1 << 4); // START_BUTTON_PIN as input
  DDRF &= ~(1 << 3); // STOP_BUTTON_PIN as input
  DDRF &= ~(1 << 2); // MOTOR_UP_PIN as input
  DDRF &= ~(1 << 1); // MOTOR_DOWN_PIN as input
  DDRB |= (1 << 6); // LED_RED_PIN as output
  DDRJ |= (1 << 7); // LED_GREEN_PIN as output
  DDRH |= (1 << 5); // LED_BLUE_PIN as output
  DDRH |= (1 << 3); // LED_YELLOW_PIN as output
  DDRG |= (1 << 4); // FAN_PIN as output
  DDRG |= (1 << 3); // FAN_ENABLE_PIN as output

  // Turn off all LEDs initially
  PORTB &= ~(1 << 6); // LED_RED_PIN LOW
  PORTJ &= ~(1 << 7); // LED_GREEN_PIN LOW
  PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW
  PORTH &= ~(1 << 3); // LED_YELLOW_PIN LOW

  reportStateTransition("DISABLED"); // inital state for the system
}

void loop() {
  DateTime now = rtc.now();

  if (currentState == DISABLED) {
    // start button press?
    lcd.clear();
    PORTH |= (1 << 3); // LED_YELLOW_PIN HIGH
    PORTJ &= ~(1 << 7); // LED_GREEN_PIN LOW
    if (!(PINF & (1 << 4))) { // START_BUTTON_PIN is HIGH
      currentState = IDLE;
      PORTH &= ~(1 << 3); // LED_YELLOW_PIN LOW
      updateLCD();
      reportStateTransition("IDLE");
    }
  }
  else if (currentState == IDLE) {
    measureTempHum();
    PORTJ |= (1 << 7); // LED_GREEN_PIN HIGH
    PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW

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
    else if (!(PINF & (1 << 3))) { // STOP_BUTTON_PIN is HIGH
      currentState = DISABLED;
      reportStateTransition("DISABLED");
    }

    // high temp -> RUNNING state
    else if (temperature > TEMPERATURE_THRESHOLD) {
      currentState = RUNNING;
      reportStateTransition("RUNNING");
      updateLCD();
    }

    moveStepMotor(); //yuh
  }
  else if (currentState == ERROR_STATE) {
    // Error message to LCD that water level is low (DO NOT MONITOR TEMP + HUMIDITY)
    lcd.clear();
    lcd.print("WATER LEVEL IS");
    lcd.setCursor(0, 1);
    lcd.print("TOO LOW");
    PORTB |= (1 << 6); // LED_RED_PIN HIGH
    PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW
    PORTJ &= ~(1 << 7); // LED_GREEN_PIN LOW

    // Rest system to IDLE if water level is okay, and reset button pressed
    if ((checkWaterLevel() >= WATER_THRESHOLD) && !(PINF & (1 << 5))) { // RESET_BUTTON_PIN is HIGH
      currentState = IDLE;
      PORTB &= ~(1 << 6); // LED_RED_PIN LOW
      reportStateTransition("IDLE");
    }

    // Stop button?
    else if (!(PINF & (1 << 3))) { // STOP_BUTTON_PIN is HIGH
      currentState = DISABLED;
      PORTB &= ~(1 << 6); // LED_RED_PIN LOW
      reportStateTransition("DISABLED");
    }

    moveStepMotor();
  }
  else if (currentState == RUNNING) {
    updateLCD();
    measureTempHum();
    PORTH |= (1 << 5); // LED_BLUE_PIN HIGH
    PORTJ &= ~(1 << 7); // LED_GREEN_PIN LOW
    PORTG |= (1 << 3); // FAN_ENABLE_PIN HIGH
    PORTG |= (1 << 4); // FAN_PIN HIGH

    // Temp + hum monitoring
    if ((lastMinUpdate < (int)now.minute()) || ((int)(now.minute() == 0) && (lastMinUpdate != 0))) { //update ONLY every minute
      updateLCD();
    }

    // monitor water level!
    if (checkWaterLevel() < WATER_THRESHOLD) {
      currentState = ERROR_STATE;
      PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW
      PORTG &= ~(1 << 3); // FAN_ENABLE_PIN LOW
      PORTG &= ~(1 << 4); // FAN_PIN LOW
      U0putstring("TURNING OFF FAN");
      reportStateTransition("ERROR");
    }

    // stop button pressed??
    else if (!(PINF & (1 << 3))) { // STOP_BUTTON_PIN is HIGH
      currentState = DISABLED;
      PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW
      PORTG &= ~(1 << 3); // FAN_ENABLE_PIN LOW
      PORTG &= ~(1 << 4); // FAN_PIN LOW
      U0putstring("TURNING OFF FAN");
      reportStateTransition("DISABLED");
    }
    //if temp low -> IDLE
    else if ((!isnan(temperature)) && (temperature <= TEMPERATURE_THRESHOLD)) {
      PORTH &= ~(1 << 5); // LED_BLUE_PIN LOW
      PORTG &= ~(1 << 3); // FAN_ENABLE_PIN LOW
      PORTG &= ~(1 << 4); // FAN_PIN LOW
      U0putstring("TURNING OFF FAN");
      currentState = IDLE;
      reportStateTransition("IDLE");
    }
    moveStepMotor();
  }
}

float checkWaterLevel() {
  // Read water level sensor
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  float waterLevel = map(sensorValue, 0, 511, 0, 100);
  return waterLevel;
}

void moveStepMotor() {
  if (PINF & (1 << 2)) { // MOTOR_UP_PIN is HIGH
    U0putstring("MOVING STEPPER MOTOR CLKWISE ");
    U0putstring(ANGLE_PER_CYCLE);
    U0putstring(" DEGREES\n");
    stepper.step(stepsPerCycle);
  }
  else if (PINF & (1 << 1)) { // MOTOR_DOWN_PIN is HIGH
    U0putstring("MOVING STEPPER MOTOR CNTR CLKWISE ");
    U0putstring(ANGLE_PER_CYCLE);
    U0putstring(" DEGREES\n");
    stepper.step(-stepsPerCycle);
  }
}

void updateLCD() {
  DateTime now = rtc.now();
  lcd.clear();
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print("%");

  lastMinUpdate = now.minute(); //update last... update
}

void measureTempHum() {
  if (!isnan(temperature) && !isnan(humidity)) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
  }
}

void reportStateTransition(String state) {
  DateTime now = rtc.now();
  U0putstring("State changed to ");
  U0putstring(state.c_str());
  U0putstring(" at ");
  U0putstring(now.hour());
  U0putchar(':');
  U0putstring(now.minute());
  U0putchar(':');
  U0putstring(now.second());
  U0putchar('\n');

  //Report turning on fan, if we just moved to RUNNING state . . .
  if (state == "RUNNING") {
    U0putstring("TURNING ON FAN");
    U0putstring(" at ");
    U0putstring(now.hour());
    U0putchar(':');
    U0putstring(now.minute());
    U0putchar(':');
    U0putstring(now.second());
    U0putchar('\n');
  }
}

void U0init(unsigned long U0baud)
{
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  // Check the RDA (Receive Data Available) bit in UCSR0A
  // RDA bit is bit 7
  return (*myUCSR0A & (1 << 7)) != 0;
}

unsigned char U0getchar()
{
  // Wait for data to be received
  while (!U0kbhit());
  // Read data from the buffer
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  // Wait for the transmit buffer to be empty
  while (!(*myUCSR0A & (1 << 5))); // Wait for TBE (Transmit Buffer Empty) bit
  // Put data into buffer, which will be transmitted
  *myUDR0 = U0pdata;
}

void U0putstring(char *U0data)
{
  while (*U0data != '\0') {
    U0putchar(*U0data);
    U0data++;
  }
}

