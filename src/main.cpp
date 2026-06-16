#define USE_WIFI_NINA         false
#define USE_WIFI101           true
#include <WiFiWebServer.h>

const char ssid[] = "EEERover";
const char pass[] = "exhibition";
const int groupNumber = 20; // Set your group number to make the IP address constant - only do this on the EEERover network

WiFiWebServer server(80);

//motor
const int right_motor_DIR_pin = 2;
const int right_motor_PWM_pin = 3;
const int left_motor_DIR_pin = 8;
const int left_motor_PWM_pin = 9;

//type: infrared
const int IR_PIN = 4;
volatile unsigned long pulseCount = 0; 
unsigned long lastCheckTime = 0;
const unsigned long sampleWindow = 150;

int infrared_pulse_rate = 0;
float infrared_pulse_rate_exact = 0;

//circular buffer for last 10 IR readings
const int IR_HISTORY_SIZE = 15;
float ir_history[IR_HISTORY_SIZE];
int ir_history_index = 0;
int ir_history_count = 0;

//type: ultrasonic
const int ULTRASOUND_PIN = 6;   
bool previous = LOW;

//type: magneticfield
const int HALL_PIN = A0;
int restingValue = 0;

enum MagDir { MAG_UP, MAG_DOWN, MAG_NONE };

//circular buffer for magnetic field readings
const int MAG_HISTORY_SIZE = 15;
MagDir mag_history[MAG_HISTORY_SIZE];
int mag_history_index = 0;
int mag_history_count = 0;
MagDir modal_mag = MAG_NONE;

//circular buffer for ultrasound readings
const int US_HISTORY_SIZE = 15;
bool us_history[US_HISTORY_SIZE];
int us_history_index = 0;
int us_history_count = 0;
bool modal_ultrasound = LOW;

//age
struct RadioReading {
  bool valid;
  char age[5];
  unsigned long lastUpdate;
};

RadioReading currentReading = {false, "", 0};

char buffer[5] = "";
int bufferIndex = 0;

bool active_rock_scan = false;

//motor
int input_speed(){

  int speed_percentage = server.arg("speed").toInt();

  if(speed_percentage <= 1){
    return 0;
  }

  return 125 + 130 * speed_percentage / 100;
}

void moveForward(){
  
  int speed = input_speed();

  //better to set direction first, and then turn motors on
  digitalWrite(right_motor_DIR_pin, 1);
  digitalWrite(left_motor_DIR_pin, 1);

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

  digitalWrite(right_motor_DIR_pin, 0);
  digitalWrite(left_motor_DIR_pin, 0);

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

  analogue_stick_X_axis = -analogue_stick_X_axis; //left/right was reversed in testing, this fixes it

  float vertical_movement = speed;
  float horizontal_movement = 0;

  if(abs(speed) < 0.05){ //allows the rover to turn in place
    horizontal_movement = analogue_stick_X_axis * 0.9;
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

  //adding 128PWM so that rover moves at low speed values
    if(left_wheel_speed > 0){
      left_wheel_speed = 125 + left_wheel_speed * 130 / 255;
    }
    else if(left_wheel_speed < 0){
      left_wheel_speed  =  -125 + left_wheel_speed * 130 / 255;
    }

    if(right_wheel_speed > 0){
      right_wheel_speed =  125 + right_wheel_speed * 130 / 255;
    }
    else if(right_wheel_speed < 0){
      right_wheel_speed  =  -125 + right_wheel_speed * 130 / 255;
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
    static unsigned long lastPulse = 0;
    unsigned long now = micros();
    if (now - lastPulse > 500) {  // ignore pulses within 500µs of each other
        pulseCount++;
        lastPulse = now;
    }
}

void infrared_pulses(){

    if (millis() - lastCheckTime >= sampleWindow) {
        noInterrupts();
        unsigned long finalCount = pulseCount;
        pulseCount = 0; 
        interrupts();
        
        float rate = (float)finalCount * 1000.0 / sampleWindow;
        infrared_pulse_rate_exact = rate;

        // Store reading in circular buffer
        ir_history[ir_history_index] = rate;
        ir_history_index = (ir_history_index + 1) % IR_HISTORY_SIZE;
        if (ir_history_count < IR_HISTORY_SIZE) ir_history_count++;

        // Classify each historical reading into a bucket
        int count_547 = 0;
        int count_312 = 0;
        int count_0   = 0;

        for (int i = 0; i < ir_history_count; i++) {
            float r = ir_history[i];
            if      (r > 370)  count_547++;
            else if (r > 200)  count_312++;
            else               count_0++;
        }

        // Pick the modal bucket
        if (count_547 >= count_312 && count_547 >= count_0) {
            infrared_pulse_rate = 547;
        } else if (count_312 >= count_547 && count_312 >= count_0) {
            infrared_pulse_rate = 312;
        } else {
            infrared_pulse_rate = 0;
        }

        Serial.print("IR modal rate: "); Serial.print(infrared_pulse_rate);
        Serial.print(" | counts — 547: "); Serial.print(count_547);
        Serial.print("  312: "); Serial.print(count_312);
        Serial.print("  0: "); Serial.println(count_0);

        lastCheckTime = millis();
    }
}

//type: magneticfield

MagDir readMagnet() {
    int v = analogRead(HALL_PIN);
    int delta = v - restingValue;
    if (delta > 60) {
        return MAG_UP;
    } 
    else if (delta < -60) {
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

void update_sensors() {
  // --- Magnetic ---
  MagDir m = readMagnet();
  mag_history[mag_history_index] = m;

  mag_history_index = (mag_history_index + 1) % MAG_HISTORY_SIZE;

  if (mag_history_count < MAG_HISTORY_SIZE){
    mag_history_count++;
  }

  int count_up = 0;
  int count_down = 0;
  int count_none = 0;

  for (int i = 0; i < mag_history_count; i++) {
    if(mag_history[i] == MAG_UP){
      count_up++;
    }

    else if(mag_history[i] == MAG_DOWN){
      count_down++;
    }
    
    else{
      count_none++;
    }
  }

  if(count_up >= count_down && count_up >= count_none) {
    modal_mag = MAG_UP;
  }
  else if(count_down >= count_up && count_down >= count_none){
    modal_mag = MAG_DOWN;
  }
  else{
    modal_mag = MAG_NONE;
  }

  // --- Ultrasound ---
  bool us = digitalRead(ULTRASOUND_PIN);
  us_history[us_history_index] = us;
  us_history_index = (us_history_index + 1) % US_HISTORY_SIZE;

  if (us_history_count < US_HISTORY_SIZE){
    us_history_count++;
  }

  int count_high = 0;
  int count_low = 0;

  for (int i = 0; i < us_history_count; i++) {

    if (us_history[i]){
        count_high++;
      }
      
    else{
        count_low++;
    }
  }

  if(count_high >= count_low){
    modal_ultrasound = HIGH;
  }

  else{
    modal_ultrasound = LOW;
  }
}

void radioUpdate() {

  if (currentReading.valid && (millis() - currentReading.lastUpdate > 1000)) {
    currentReading.valid = false;
    currentReading.age[0] = '\0';
    Serial.println("Radio signal lost — age reset");
  }

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

      // Read magnet state
      MagDir m = modal_mag;
      String magState;
      
      switch (m) {
        case MAG_UP:   magState = "UP";   break;
        case MAG_DOWN: magState = "DOWN"; break;
        default:       magState = "NONE"; break;
      }

      // Read ultrasound state
      String ultrasoundState = modal_ultrasound ? "HIGH" : "LOW";

      Serial.print("Rock age: ");
      Serial.print(currentReading.age);
      Serial.print(" -> ");
      Serial.print(age, 2);
      Serial.print(" billion years");
      Serial.print(" | Magnet: ");
      Serial.print(magState);
      Serial.print(" | Ultrasound: ");
      Serial.print(ultrasoundState);
      Serial.print(" | IR: ");
      Serial.print(infrared_pulse_rate_exact, 2);
      Serial.println(" Hz");

      bufferIndex = 0;
    }
  }
}

void rock_scan_toggle(){

  active_rock_scan = !active_rock_scan;

  if(active_rock_scan){

    noInterrupts();
    pulseCount = 0;
    interrupts();

    lastCheckTime = millis();
    ir_history_count = 0;
    ir_history_index = 0;

    mag_history_count = 0;
    mag_history_index = 0;
    modal_mag = MAG_NONE;

    us_history_count = 0;
    us_history_index = 0;
    modal_ultrasound = LOW;

    attachInterrupt(digitalPinToInterrupt(IR_PIN), countPulses, RISING);

    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("SCANNING"));
  }

  else{

    detachInterrupt(digitalPinToInterrupt(IR_PIN));

    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.send(200, F("text/plain"), F("SCAN ENDED"));
  }
}


void rock_scan(){

  //ROCK TYPE
  
  String rock_type;

  //type: ultrasonic

  bool ultrasound = modal_ultrasound;

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

  MagDir m = modal_mag;

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

  bool mag_valid = false;

  if(magnetic_field != "NONE"){
    mag_valid = true;
  }

  bool ir_valid = false;

  if(infrared_pulse_rate == 547 || infrared_pulse_rate == 312){
    ir_valid = true;
  }

  if(mag_valid){

    //magnetic direction + ultrasound
    if(ultrasound == HIGH && magnetic_field == "DOWN"){
      rock_type = "Basaltoid";
    }
    else if(ultrasound == LOW && magnetic_field == "DOWN"){
      rock_type = "Gravion";
    }
    else if(ultrasound == HIGH && magnetic_field == "UP"){
      rock_type = "Regolix";
    }
    else{ // LOW + UP
      rock_type = "Lunarite";
    }
  }

  else if(ir_valid){
    //agnetic gave NONE, classify with IR + ultrasound
    if(infrared_pulse_rate == 547 && ultrasound == HIGH){
      rock_type = "Basaltoid";
    }
    else if(infrared_pulse_rate == 312 && ultrasound == LOW){
      rock_type = "Gravion";
    }
    else if(infrared_pulse_rate == 312 && ultrasound == HIGH){
      rock_type = "Regolix";
    }
    else{ // 547 + LOW
      rock_type = "Lunarite";
    }
  }
  else{
    // no magnetic direction and no IR signal
    rock_type = "UNKNOWN";
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

  String rock_data = rock_age + "-" + rock_type + "-" + String(infrared_pulse_rate) + "-" + ultrasound_value + "-" + magnetic_field;

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
  // attachInterrupt(digitalPinToInterrupt(IR_PIN), countPulses, RISING);
    
  lastCheckTime = millis();

  
  //type: ultrasonic
  pinMode(ULTRASOUND_PIN, INPUT);

  //type: magneticfield
  analogReadResolution(12); 
  pinMode(HALL_PIN, INPUT);

  // Average 20 readings over 200ms to get a stable baseline
  delay(3000); 
  long sum = 0;
  for (int i = 0; i < 200; i++) {
    sum += analogRead(HALL_PIN);
    delay(10);
  }
  restingValue = sum / 200;

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
    WiFi.config(IPAddress(169,254,135,groupNumber+1));

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
  server.on(F("/rock_scan_toggle"), rock_scan_toggle);
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
  
  if(active_rock_scan){
    infrared_pulses();
    radioUpdate();
    update_sensors();
  }
}

