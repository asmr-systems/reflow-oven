String command;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(1000);
  //Serial.write("COOL");
  //Serial.write("\n");

  while(Serial.available()) {
    command = Serial.readString();// read the incoming data as string
    Serial.println(command);
  }
}
