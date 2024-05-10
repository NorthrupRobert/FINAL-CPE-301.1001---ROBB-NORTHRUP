const int waterSensorPin = A15;  // Define analog pin for water sensor
const int threshold = 25;        // Threshold for low water level (in percentage)

void setup() {
  Serial.begin(9600);  // Initialize serial communication
}

void loop() {
  int sensorValue = analogRead(waterSensorPin);  // Read the sensor value
  float percentage = map(sensorValue, 0, 255, 0, 100);  // Map the sensor value to percentage
  
  Serial.print("Water level: ");
  Serial.print(percentage);
  Serial.println("%");
  
  if (percentage < threshold) {
    Serial.println("Water level is low. Refill the tank!");
  }
  
  delay(10000);  // Wait for a second before taking the next reading
}