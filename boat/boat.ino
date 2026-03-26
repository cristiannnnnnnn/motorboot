
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "LittleFS.h"

uint8_t broadcastAddress[] = {0x10, 0x06, 0x1C, 0x41, 0x91, 0xD4};


//============================ Servo
/* Pinout
VIN - 5V Battery
GND - GND Battery
Control - GPIO 13
*/
static const int servoPin = 13;
Servo servo1;


//============================ Motors
/*
ENA - GPIO 14
IN1 - GPIO 27
IN2 - GPIO 26

ENB - GPIO 25
IN3 - GPIO 33
IN4 - GPIO 32
*/

// Motor A

#define MOTOR1PIN1 27 
#define MOTOR1PIN2 26
#define ENABLE1PIN 14


// Motor B
#define MOTOR2PIN1 33
#define MOTOR2PIN2 32
#define ENABLE2PIN 25


// Setting Motor PWM properties
const int freq = 30000;
const int pwmChannel = 6;
const int resolution = 8;

//============================ State variables
int lastTransmission = 0;

enum PacketType { CONTROL, CALIBRATION};

typedef struct control_message_received {
  PacketType type;
  int power; // -1000 to 1000
  int turn; // -1000 to 1000
} control_message_received;

typedef struct boat_calibration{
    PacketType type;
    // ESC (Electronic Speed Controller) settings
    int motor_off;
    int motor_lowest;      // The minimum pulse width (in microseconds) for the ESC.
    int motor_highest;     // The maximum pulse width (in microseconds) for the ESC.
    // Servo motor settings
    int servo_lowest;    // The minimum angle (or pulse width) for the servo.
    int servo_middle;    // The center/neutral position for the servo.
    int servo_highest;   // The maximum angle (or pulse width) for the servo.
} boat_calibration;



typedef union struct_message_received{
  PacketType type;
  control_message_received control;
  boat_calibration calibration;
} struct_message_received;


typedef struct struct_message_send {
  char mode; // 'n' normal 'u' 'c' calibration
  String message; // "waiting for confirmation on stick y" 
  float batteryStatus; // in volts
} struct_message_send;

boat_calibration currentCalibration;
boat_calibration defaultCalibration = {CONTROL, 0, 170, 205, 0, 90, 180};

const char* FILE_PATH = "/boat_cfg.bin";


void init_calibration(){
  if(LittleFS.exists(FILE_PATH)){
    File file = LittleFS.open(FILE_PATH, FILE_READ);
    if (file && file.size() == sizeof(boat_calibration)) {
      file.read((uint8_t *)&currentCalibration, sizeof(boat_calibration));
      file.close();
    }
    else{
      currentCalibration = defaultCalibration;
    }
  }
  else{
    currentCalibration = defaultCalibration;
  }
}

void save_calibration() {
  File file = LittleFS.open(FILE_PATH, FILE_WRITE);
  if (file) {
    file.write((uint8_t *)&currentCalibration, sizeof(boat_calibration));
    file.close();
    Serial.println("Calibration saved!");
  }
}

void print_calibration(boat_calibration cal){
  Serial.print("Current Calibration: motor_off = ");
  Serial.print(cal.motor_off);
  Serial.print(", motor_lowest = ");
  Serial.print(cal.motor_lowest);
  Serial.print(", motor_highest = ");
  Serial.print(cal.motor_highest);
  Serial.print(", servo_lowest = ");
  Serial.print(cal.servo_lowest);
  Serial.print(", servo_middle = ");
  Serial.print(cal.servo_middle);
  Serial.print(", servo_highest = ");
  Serial.println(cal.servo_highest);
}

// Data Packet
struct_message_received received_calibration;
struct_message_received received_packet;
struct_message_received current_control;
struct_message_send sendData;
esp_now_peer_info_t peerInfo;
String success;
//============================ Calibration Variables


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  lastTransmission = 0;
  memcpy(&received_packet, incomingData, sizeof(struct_message_received));

  if(received_packet.type == CALIBRATION){
    received_calibration = received_packet;
    Serial.println("Received new calibration data:");
    print_calibration(received_calibration.calibration);

    /*
    int new_motor_off = map(received_calibration.calibration.motor_off, 0, 1000, currentCalibration.motor_lowest, currentCalibration.motor_highest);
    int new_motor_lowest = map(received_calibration.calibration.motor_lowest, 0, 1000, currentCalibration.motor_lowest, currentCalibration.motor_highest);
    int new_motor_highest = map(received_calibration.calibration.motor_highest, 0, 1000, currentCalibration.motor_lowest, currentCalibration.motor_highest);
    */

    int new_servo_lowest = mapServo(received_calibration.calibration.servo_lowest);
    int new_servo_middle = mapServo(received_calibration.calibration.servo_middle);
    int new_servo_highest = mapServo(received_calibration.calibration.servo_highest);

    received_calibration.calibration.servo_lowest = new_servo_lowest;
    received_calibration.calibration.servo_middle = new_servo_middle;
    received_calibration.calibration.servo_highest = new_servo_highest;

    currentCalibration = received_calibration.calibration;
    save_calibration();
  }
  else{
    current_control = received_packet;
  }
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}
 
void setup() {

  init_calibration();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  Serial.begin(115200);
  servo1.attach(servoPin);
  
  pinMode(MOTOR1PIN1, OUTPUT);
  pinMode(MOTOR1PIN2, OUTPUT);
  pinMode(MOTOR2PIN1, OUTPUT);
  pinMode(MOTOR2PIN2, OUTPUT);
  pinMode(ENABLE1PIN, OUTPUT);
  pinMode(ENABLE2PIN, OUTPUT);
  ledcAttachChannel(ENABLE1PIN, freq, resolution, pwmChannel);
  ledcAttachChannel(ENABLE2PIN, freq, resolution, pwmChannel);

  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  neutralizeValues();


  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}
 
// State variables
bool motorRunning = false;
bool forward = true;

void handlemotor(int currentPower, int currentTurn){


  if(currentPower < 0 && (forward == true || motorRunning == false)){
    Serial.println("REVERSING MOTOR");
    digitalWrite(MOTOR1PIN1, LOW);
    digitalWrite(MOTOR1PIN2, HIGH);
    digitalWrite(MOTOR2PIN1, LOW);
    digitalWrite(MOTOR2PIN2, HIGH);
    forward = false;
    motorRunning = true;  
  }
  else if(currentPower > 0 && (forward == false || motorRunning == false)){
    Serial.println("FORWARDING MOTOR");
    digitalWrite(MOTOR1PIN1, HIGH);
    digitalWrite(MOTOR1PIN2, LOW);
    digitalWrite(MOTOR2PIN1, HIGH);
    digitalWrite(MOTOR2PIN2, LOW);
    forward = true;
    motorRunning = true;  
  }
  else if(currentPower == 0){
    Serial.println("STOPPING MOTOR");
    digitalWrite(MOTOR1PIN1, LOW);
    digitalWrite(MOTOR1PIN2, LOW);
    digitalWrite(MOTOR2PIN1, LOW);
    digitalWrite(MOTOR2PIN2, LOW);
    ledcWrite(ENABLE1PIN, currentCalibration.motor_off);
    ledcWrite(ENABLE2PIN, currentCalibration.motor_off);
    motorRunning = false;  
  }

  int power = map(abs(currentPower), 0, 1000, currentCalibration.motor_lowest, currentCalibration.motor_highest);
  //Serial.print("Mapped power: ");
  //Serial.println(power);
  ledcWrite(ENABLE1PIN, power);
  ledcWrite(ENABLE2PIN, power);

}

int mapServo(int currentTurn){
  int turn;
  if(currentTurn < 0){
    turn = map(currentTurn, -1, -1000, currentCalibration.servo_middle, currentCalibration.servo_lowest);
  }
  else if(currentTurn > 0){
    turn = map(currentTurn, 1, 1000, currentCalibration.servo_middle, currentCalibration.servo_highest);
  }
  else{
    turn = currentCalibration.servo_middle;
  }
  return turn;
}

void handleServo(int currentPower, int currentTurn){

  int turn;
  if(currentTurn < 0){
    turn = map(currentTurn, -1, -1000, currentCalibration.servo_middle, currentCalibration.servo_lowest);
  }
  else if(currentTurn > 0){
    turn = map(currentTurn, 1, 1000, currentCalibration.servo_middle, currentCalibration.servo_highest);
  }
  else{
    turn = currentCalibration.servo_middle;
  }
  Serial.print("Received turn: ");
  Serial.println(currentTurn);
  Serial.print("Mapped turn: ");
  Serial.println(turn);
    servo1.write(turn);

}

void neutralizeValues(){
  current_control.control.power = currentCalibration.motor_off;
  current_control.control.turn = currentCalibration.servo_middle;
}


void loop() {
  int time = millis();
  if(lastTransmission > 1500){
    neutralizeValues();
  }

  // getting current values because packet may arrive async during loop
  int currentPower = current_control.control.power;
  int currentTurn = current_control.control.turn;

  /*
  Serial.print("Received Power: ");
  Serial.println(currentPower);
  Serial.print("Received Turn: ");
  Serial.println(currentTurn);
  */

  handlemotor(currentPower, currentTurn);
  handleServo(currentPower, currentTurn);


  lastTransmission += (millis()-time);
}