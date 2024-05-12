   /*
  ULN2003-Stepper-Motor-Driver
  modified on 25 Nov 2020
  by Amir Mohammad Shojaee @ Electropeak
  Home
  
  based on Arduino library Example
*/

#include <Stepper.h>
const int stepsPerRevolution = 2048;
const int rpm = 12;
Stepper stepper1 = Stepper(stepsPerRevolution, 8, 10, 9, 11);

void setup() {
  stepper1.setSpeed(rpm);
}

void loop() {
  stepper1.step(stepsPerRevolution);
  delay(100);
  stepper1.step(-stepsPerRevolution);
  delay(100);
}
