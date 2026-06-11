#define USE_WIFI_NINA         false
#define USE_WIFI101           true
#include <WiFiWebServer.h>

const char ssid[] = "EEERover";
const char pass[] = "exhibition";
const int groupNumber = 20; // Set your group number to make the IP address constant - only do this on the EEERover network

WiFiWebServer server(80);

//motor
const int right_motor_DIR_pin = 8;
const int right_motor_PWM_pin = 9;
const int left_motor_DIR_pin = 2;
const int left_motor_PWM_pin = 3;

//type: infrared
const int IR_PIN = 4;
volatile unsigned long pulseCount = 0; 
unsigned long lastCheckTime = 0;
const unsigned long sampleWindow = 2000;

int infrared_pulse_rate = 0;
float infrared_pulse_rate_exact = 0;

//type: ultrasonic
const int ULTRASOUND_PIN = 6;   
bool previous = LOW;

//type: magneticfield
const int HALL_PIN = A0;
int restingValue = 0;

enum MagDir { MAG_UP, MAG_DOWN, MAG_NONE };

//age
struct RadioReading {
  bool valid;
  char age[5];
  unsigned long lastUpdate;
};

RadioReading currentReading = {false, "", 0};

char buffer[5] = "";
int bufferIndex = 0;

//motor
int input_speed(){

  int speed_percentage = server.arg("speed").toInt();

  return 255 * speed_percentage / 100;
}

void moveForward(){
  
  int speed = input_speed();

  //better to set direction first, and then turn motors on
  digitalWrite(right_motor_DIR_pin, 0);
  digitalWrite(left_motor_DIR_pin, 0);

  analogWrite(right_motor_PWM_pin, speed);
  analogWrite(left_motor_PWM_pin, speed);

  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), F("Moving Forward") );

}

void moveRight(){

  int speed = input_speed();

  digitalWrite(right_motor_DIR_pin, 0);
  digitalWrite(left_motor_DIR_pin, 1);

  analogWrite(right_motor_PWM_pin, speed);
  analogWrite(left_motor_PWM_pin, speed);

  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), F("Turning Right"));
  
}

void moveLeft(){

  int speed = input_speed();

  digitalWrite(right_motor_DIR_pin, 1);
  digitalWrite(left_motor_DIR_pin, 0);

  analogWrite(right_motor_PWM_pin, speed);
  analogWrite(left_motor_PWM_pin, speed);

  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), F("Turning Left"));
  
}

void moveReverse(){

  int speed = input_speed();

  digitalWrite(right_motor_DIR_pin, 1);
  digitalWrite(left_motor_DIR_pin, 1);

  analogWrite(right_motor_PWM_pin, speed);
  analogWrite(left_motor_PWM_pin, speed);

  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), F("Reversing") );
  
}

void stop_rover(){

  analogWrite(right_motor_PWM_pin, 0);
  analogWrite(left_motor_PWM_pin, 0);

  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), F("Rover Stopped"));
  
}

void controller_movement(){

  //getting the speed value
  float speed = server.arg("speed").toFloat(); 

  if(speed > 1){
    speed = 1;
  }
  else if(speed < -1){
    speed  = -1;
  }

  //getting analogue_stick value
  float analogue_stick_X_axis = server.arg("analogue_stick_X_axis").toFloat(); 

  if(analogue_stick_X_axis > 1){
    analogue_stick_X_axis = 1;
  }
  else if(analogue_stick_X_axis < -1){
    analogue_stick_X_axis  = -1;
  }
  else if(abs(analogue_stick_X_axis) < 0.075){ //found value using gamepad-tester, my controller stays below 0.075 when not used
    analogue_stick_X_axis = 0;
  }

  float vertical_movement = speed;
  float horizontal_movement = 0;

  if(abs(speed) < 0.05){ //allows the rover to turn in place
    horizontal_movement = analogue_stick_X_axis;
  }
  else{
    horizontal_movement = analogue_stick_X_axis * abs(speed); //the speed determines how sharp the turn is
  }

  int left_wheel_speed = (vertical_movement + horizontal_movement) * 255;
  int right_wheel_speed =  (vertical_movement - horizontal_movement) * 255;

  if(left_wheel_speed > 255){
    left_wheel_speed = 255;
  }
  else if(left_wheel_speed < -255){
    left_wheel_speed  = -255;
  }

  if(right_wheel_speed > 255){
    right_wheel_speed = 255;
  }
  else if(right_wheel_speed < -255){
    right_wheel_speed  = -255;
  }

  if(left_wheel_speed >= 0){
    digitalWrite(left_motor_DIR_pin, 0);
  }
  else{
    digitalWrite(left_motor_DIR_pin, 1);
  }

  if(right_wheel_speed >= 0){
    digitalWrite(right_motor_DIR_pin, 0); 
  }
  else{
    digitalWrite(right_motor_DIR_pin, 1);
  }

  analogWrite(left_motor_PWM_pin, abs(left_wheel_speed) * 0.98);
  analogWrite(right_motor_PWM_pin, abs(right_wheel_speed));

  if(left_wheel_speed == 0 && right_wheel_speed == 0){
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("Idle"));
  }
  else if(left_wheel_speed > 0 && right_wheel_speed > 0){
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("Moving Forward"));
  }
  else if(left_wheel_speed < 0 && right_wheel_speed < 0){
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("Reversing"));
  }
  else{
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("Turning"));
  }
}

//type: infrared

void countPulses() {
    pulseCount++;
}

void infrared_pulses(){

    if (millis() - lastCheckTime >= sampleWindow) {
        noInterrupts();
        unsigned long finalCount = pulseCount;
        pulseCount = 0; 
        interrupts();
        
        float rate = (float)finalCount * 1000.0 / sampleWindow;
        infrared_pulse_rate_exact = rate;

        Serial.print("Measured rate: ");
        Serial.print(rate);
        Serial.println(" Hz");

        // determine which rock with the mid value of 547 and 312
        if (rate > 430) {
          infrared_pulse_rate = 547;
          Serial.println(">>> Rate: 547 s^-1 <<<");
        } else if (rate > 200) { 
          infrared_pulse_rate = 312;
          Serial.println(">>> Rate: 312 s^-1 <<<");
        } else {
          infrared_pulse_rate = 0;
          Serial.println("No distinct IR signal");
        }
        
        Serial.println("---------------------------------------");
 
        lastCheckTime = millis();
    }
}

//type: magneticfield

MagDir readMagnet() {
    int v = analogRead(HALL_PIN);
    int delta = v - restingValue;
    if (delta > 300) {
        return MAG_UP;
    } 
    else if (delta < -300) {
        return MAG_DOWN;
    }
    return MAG_NONE;
}

//age
void radioStart() {
  Serial1.begin(600);
}

bool isAgeChar(char c) {
  return (c >= '0' && c <= '9');
}

float ageInBillions(RadioReading r) {
  if (!r.valid) {
    return -1;
  }

  int ageNumber =
      (r.age[1] - '0') * 100 +
      (r.age[2] - '0') * 10 +
      (r.age[3] - '0');

  return ageNumber / 100.0;
}

void radioUpdate() {

  while (Serial1.available()) {

    char c = Serial1.read();

    if (c == '#') {
      bufferIndex = 0;
      buffer[bufferIndex++] = c;
    }
    else if (bufferIndex > 0 &&
             bufferIndex < 4 &&
             isAgeChar(c)) {
      buffer[bufferIndex++] = c;
    }
    else {
      bufferIndex = 0;
      buffer[0] = '\0';

      currentReading.valid = false;
      currentReading.age[0] = '\0';
    }

    if (bufferIndex == 4) {

      buffer[4] = '\0';

      for (int i = 0; i < 5; i++) {
        currentReading.age[i] = buffer[i];
      }

      currentReading.valid = true;
      currentReading.lastUpdate = millis();

      float age = ageInBillions(currentReading);

      Serial.print("Rock age: ");
      Serial.print(currentReading.age);
      Serial.print(" -> ");
      Serial.print(age, 2);
      Serial.println(" billion years");

      bufferIndex = 0;
    }
  }
}


void rock_scan(){

  //ROCK TYPE
  
  String rock_type;

  //type: ultrasonic

  bool ultrasound = digitalRead(ULTRASOUND_PIN);

  //for testing
  if (ultrasound != previous) {
      if (ultrasound == HIGH){
          Serial.println("Ultrasound detected");
      } 
      else {
          Serial.println("Ultrasound not detected");
      }
  }

  previous = ultrasound;

  //type: magneticfield
  String magnetic_field;

  MagDir m = readMagnet();

  //for testing
  int v = analogRead(HALL_PIN);
  Serial.print("  Delta: ");
  Serial.print(v - restingValue);
  Serial.print("  State: ");
  switch (m) {
      case MAG_UP:   Serial.println("UP");   break;
      case MAG_DOWN: Serial.println("DOWN"); break;
      case MAG_NONE: Serial.println("none"); break;
  }

  if(m == MAG_UP){
    magnetic_field = "UP";
  }
  else if(m == MAG_DOWN){
    magnetic_field = "DOWN";
  }
  else{
    magnetic_field = "NONE";
  }

  //finding the actual rock type

  if(infrared_pulse_rate == 547 && ultrasound == HIGH && magnetic_field == "DOWN"){
    rock_type = "Basaltoid";
  }

  else if(infrared_pulse_rate == 312 && ultrasound == LOW && magnetic_field == "DOWN"){
    rock_type = "Gravion";
  }

  else if(infrared_pulse_rate == 312 && ultrasound == HIGH && magnetic_field == "UP"){
    rock_type = "Regolix";
  }

  else if(infrared_pulse_rate == 547 && ultrasound == LOW && magnetic_field == "UP"){
    rock_type = "Lunarite";
  }

  else{
    rock_type = "UNKOWN";
  }

  //rock age

  String rock_age;

  if(ageInBillions(currentReading) < 0){
    rock_age = "UNKNOWN";
  }
  else{
    rock_age = String(ageInBillions(currentReading));
  }

  String ultrasound_value;

  if(ultrasound){
    ultrasound_value = "HIGH";
  }
  else{
    ultrasound_value = "LOW";
  }

  String rock_data = rock_age + "-" + rock_type + "-" + String(infrared_pulse_rate_exact) + "-" + ultrasound_value + "-" + magnetic_field;

  //change this to return age-type-infrared-ultrasound-magneticfield, like this, dashed
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.send(200, F("text/plain"), rock_data);
  
}

void setup()
{

  Serial.begin(9600);

  //age
  radioStart();
    
  //motor
  pinMode(right_motor_PWM_pin, OUTPUT);
  pinMode(right_motor_DIR_pin, OUTPUT);
  pinMode(left_motor_PWM_pin, OUTPUT);
  pinMode(left_motor_DIR_pin, OUTPUT);

  analogWrite(right_motor_PWM_pin, 0);
  analogWrite(left_motor_PWM_pin, 0);

  //type: infrared
  pinMode(IR_PIN, INPUT);
    
  // speed the countPulses function when the pin goes from LOW to HIGH
  attachInterrupt(digitalPinToInterrupt(IR_PIN), countPulses, RISING);
    
  lastCheckTime = millis();

  
  //type: ultrasonic
  pinMode(ULTRASOUND_PIN, INPUT);

  //type: magneticfield
  analogReadResolution(12); 
  pinMode(HALL_PIN, INPUT);

  // Average 20 readings over 200ms to get a stable baseline
  delay(100); 
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(HALL_PIN);
    delay(10);
  }
  restingValue = sum / 20;

  Serial.print("Calibrated resting value: ");
  Serial.println(restingValue);

  //Wait 10s for the serial connection before proceeding
  //This ensures you can see messages from startup() on the monitor
  //Remove this for faster startup when the USB host isn't attached
  while (!Serial && millis() < 10000);  

  Serial.println(F("\nStarting Web Server"));

  //Check WiFi shield is present
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println(F("WiFi shield not present"));
    while (true);
  }

  //Configure the static IP address if group number is set
  if (groupNumber)
    WiFi.config(IPAddress(192,168,0,groupNumber+1));

  // attempt to connect to WiFi network
  Serial.print(F("Connecting to WPA SSID: "));
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED)
  {
    delay(500);
    Serial.print('.');
  }

  //Register the callbacks to respond to HTTP requests
  server.on(F("/moveForward"), moveForward);
  server.on(F("/moveRight"), moveRight);
  server.on(F("/moveLeft"), moveLeft);
  server.on(F("/moveReverse"), moveReverse);
  server.on(F("/stop"), stop_rover);
  server.on(F("/rock_scan"), rock_scan);
  server.on(F("/controller_movement"), controller_movement);
  
  server.begin();
  
  Serial.print(F("HTTP server started @ "));
  Serial.println(static_cast<IPAddress>(WiFi.localIP()));
}

//Call the server polling function in the main loop
void loop()
{
  server.handleClient();
  infrared_pulses();
  radioUpdate();

}

