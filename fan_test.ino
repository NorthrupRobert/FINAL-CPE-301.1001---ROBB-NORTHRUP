#define ENABLE 28
#define DIRA 26
#define DIRB 24

int i;

void setup() {
  pinMode(ENABLE, OUTPUT);
  pinMode(DIRA, OUTPUT);
  pinMode(DIRB, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  Serial.println("One way, then reverse");
  digitalWrite(ENABLE, HIGH);
  for (i = 0; i < 5; i++) {
    digitalWrite(DIRA, HIGH);
    digitalWrite(DIRB, LOW);
    delay(500);
    digitalWrite(DIRA, LOW);
    digitalWrite(DIRB, HIGH);
    delay(500);
  }
  digitalWrite(ENABLE, LOW);
  delay(2000);
}
