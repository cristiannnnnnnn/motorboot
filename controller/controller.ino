#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <ezButton.h>
#include "LittleFS.h"
#include <SPI.h>
#include <TFT_eSPI.h>


TFT_eSPI tft = TFT_eSPI();

uint8_t broadcastAddress[] = {0x10, 0x06, 0x1C, 0x41, 0x84, 0x34};

#define TURN_PIN  34 // X axis (turn) on joystick board it will be called VRy
#define THROTTLE_PIN  35 // Y axis (throttle/power) on joystick board it will be called VRx
#define SW_THROTTLE_PIN   17 // Throttle stick button
#define SW_TURN_PIN 16 // Turn stick button

ezButton throttle_button(SW_THROTTLE_PIN);
ezButton turn_button(SW_TURN_PIN);

enum PacketType { CONTROL, CALIBRATION};

typedef struct control_message_send {
  PacketType type;
  int power; // -1000 to 1000
  int turn; // -1000 to 1000
} control_message_send;

typedef struct calibration_message_send {
  PacketType type;
  // ESC (Electronic Speed Controller) settings
  int motor_off;
  int motor_lowest;      // The minimum pulse width (in microseconds) for the ESC.
  int motor_highest;     // The maximum pulse width (in microseconds) for the ESC.
  // Servo motor settings
  int servo_lowest;    // The minimum angle (or pulse width) for the servo.
  int servo_middle;    // The center/neutral position for the servo.
  int servo_highest;   // The maximum angle (or pulse width) for the servo.
} calibration_message_send;

typedef union struct_message_send{
  PacketType type;
  control_message_send control;
  calibration_message_send calibration;

} struct_message_send;

typedef struct struct_message_received {
  char mode; // 'n' normal 'u' 'c' calibration
  String message; // "waiting for confirmation on stick y" 
  float batteryStatus; // in volts
} struct_message_received;

struct_message_received receivedData;
struct_message_send currentControlPacket = {.control = {.type = CONTROL, .power = 0, .turn = 0}};
struct_message_send boat_calibration_packet = {.calibration = {.type = CALIBRATION}};
esp_now_peer_info_t peerInfo;
String success;

struct_message_send lastPacket = {.control = {.type = CONTROL, .power = 500, .turn = 500}};
bool isWaiting = false;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
}

void sendData(uint8_t* data, size_t len) {
  if (isWaiting) return; 

  lastPacket = *((struct_message_send*) data);
  isWaiting = true;
  esp_now_send(broadcastAddress, data, len);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  isWaiting = false;
  //Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
    if(lastPacket.type == CALIBRATION){

    }
  }
}
 
typedef struct stick_calibration {
  int defaultX;
  int defaultY;
  int toleranceX;
  int toleranceY;
  int maxX;
  int maxY;
  int minX;
  int minY;
} stick_calibration;

stick_calibration currentCalibration;
stick_calibration defaultCalibration = {1830, 1780, 80, 80, 4095, 4095, 0, 0};

int getX(){
  int valueX = analogRead(TURN_PIN);
  int newX;

  if (abs(valueX - currentCalibration.defaultX) <= currentCalibration.toleranceX){
    newX = 0;
  }
  else if(valueX - currentCalibration.defaultX - currentCalibration.toleranceX > 0){
    newX = map(valueX, currentCalibration.defaultX + currentCalibration.toleranceX, currentCalibration.maxX, 0, 1000);
  }
  else{
    newX = map(valueX, currentCalibration.defaultX - currentCalibration.toleranceX, currentCalibration.minX, 0, -1000);
  }
  return newX;
}

int getY(){
  int valueY = analogRead(THROTTLE_PIN);
  int newY;

  if (abs(valueY - currentCalibration.defaultY) <= currentCalibration.toleranceY){
    newY = 0;
  }
  else if(valueY - currentCalibration.defaultY - currentCalibration.toleranceY > 0){
    newY = map(valueY, currentCalibration.defaultY + currentCalibration.toleranceY, currentCalibration.maxY, 0, 1000);
  }
  else{
    newY = map(valueY, currentCalibration.defaultY - currentCalibration.toleranceY, currentCalibration.minY, 0, -1000);
  }
  return newY;
}

struct_message_send getControl(){
  

  struct_message_send packet;
  packet.control.type = CONTROL;
  packet.control.power = getY();
  packet.control.turn = getX();
  return packet;
}

const char* FILE_PATH = "/stick_cfg.bin";


void initCalibration(){
  
  if(LittleFS.exists(FILE_PATH)){
    File file = LittleFS.open(FILE_PATH, FILE_READ);
    if (file && file.size() == sizeof(stick_calibration)) {
      file.read((uint8_t *)&currentCalibration, sizeof(stick_calibration));
      file.close();
    }
    else{
      currentCalibration = defaultCalibration;
    }
  }
  else{
    currentCalibration = defaultCalibration;
  }
  
  Serial.print("Current Calibration: defaultX = ");
  Serial.print(currentCalibration.defaultX);
  Serial.print(", defaultY = ");
  Serial.print(currentCalibration.defaultY);
  Serial.print(", toleranceX = ");
  Serial.print(currentCalibration.toleranceX);
  Serial.print(", toleranceY = ");
  Serial.print(currentCalibration.toleranceY);
  Serial.print(", maxX = ");
  Serial.print(currentCalibration.maxX);
  Serial.print(", maxY = ");
  Serial.print(currentCalibration.maxY);
  Serial.print(", minX = ");
  Serial.print(currentCalibration.minX);
  Serial.print(", minY = ");
  Serial.println(currentCalibration.minY);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  initCalibration();

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(1);

  throttle_button.setDebounceTime(50);
  turn_button.setDebounceTime(50);
 
 
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

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


int throttle_pressed_time = 0;
int turn_pressed_time = 0;
enum ModeType { NORMAL, CALIBRATION_BOAT, CALIBRATION_STICKS};
ModeType mode = NORMAL;

// Stick calibration state
enum StickCalibrationStage { 
  CAL_DEFAULT,  // Sample both X and Y for default + tolerance
  CAL_MAX_X, 
  CAL_MIN_X,
  CAL_MAX_Y, 
  CAL_MIN_Y,
  CAL_DONE
};
StickCalibrationStage calStage = CAL_DEFAULT;
stick_calibration tempCalibration;
bool waitingForRelease = false; // Require button release before next stage

const char* calInstructions[] = {
  "Center sticks, press any button",
  "Move stick X to MAX, press THROTTLE",
  "Move stick X to MIN, press THROTTLE",
  "Move stick Y to MAX, press TURN",
  "Move stick Y to MIN, press TURN",
  "Calibration complete!"
};

enum BoatCalibrationStage {
  SERVO_LOWEST,
  SERVO_MIDDLE,
  SERVO_HIGHEST,
  MOTOR_LOWEST,
  MOTOR_HIGHEST,
  MOTOR_OFF
};
BoatCalibrationStage boatCalStage = SERVO_LOWEST;
calibration_message_send tempBoatCalibration;



const char* boatCalInstructions[] = {
  "Move servo to lowest (left) position, press TURN",
  "Move servo to middle position, press TURN",
  "Move servo to highest (right) position, press TURN",
  "Move throttle to lowest position, press THROTTLE",
  "Move throttle to highest position, press THROTTLE",
  "Set throttle to OFF position, press THROTTLE",
  "Boat calibration complete!"
};

void saveCalibration() {
  File file = LittleFS.open(FILE_PATH, FILE_WRITE);
  if (file) {
    file.write((uint8_t *)&currentCalibration, sizeof(stick_calibration));
    file.close();
    Serial.println("Calibration saved!");
  }
  Serial.print("Saved Calibration: defaultX = ");
  Serial.print(currentCalibration.defaultX);
  Serial.print(", defaultY = ");
  Serial.print(currentCalibration.defaultY);
  Serial.print(", toleranceX = ");
  Serial.print(currentCalibration.toleranceX);
  Serial.print(", toleranceY = ");
  Serial.print(currentCalibration.toleranceY);
  Serial.print(", maxX = ");
  Serial.print(currentCalibration.maxX);
  Serial.print(", maxY = ");
  Serial.print(currentCalibration.maxY);
  Serial.print(", minX = ");
  Serial.print(currentCalibration.minX);
  Serial.print(", minY = ");
  Serial.println(currentCalibration.minY);
}

void sampleDefaultCalibration() {
  delay(500);
  const int NUM_SAMPLES = 300;
  const int SAMPLE_DELAY = 50;
  
  long sumX = 0, sumY = 0;
  int minX = 4095, maxX = 0;
  int minY = 4095, maxY = 0;
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.println("Sampling... keep sticks centered");
  
  for(int i = 0; i < NUM_SAMPLES; i++) {
    int x = analogRead(TURN_PIN);
    int y = analogRead(THROTTLE_PIN);
    
    sumX += x;
    sumY += y;
    
    if(x < minX) minX = x;
    if(x > maxX) maxX = x;
    if(y < minY) minY = y;
    if(y > maxY) maxY = y;
    
    delay(SAMPLE_DELAY);
  }
  
  int avgX = sumX / NUM_SAMPLES;
  int avgY = sumY / NUM_SAMPLES;
  
  tempCalibration.defaultX = avgX;
  tempCalibration.defaultY = avgY;
  tempCalibration.toleranceX = max(avgX - minX, maxX - avgX) *2 + 10; // +10 for safety margin
  tempCalibration.toleranceY = max(avgY - minY, maxY - avgY) *2 + 10;
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("Default calibration done!");
  tft.setCursor(10, 30);
  tft.print("X: "); tft.print(avgX); tft.print(" +/-"); tft.println(tempCalibration.toleranceX);
  tft.print("Y: "); tft.print(avgY); tft.print(" +/-"); tft.println(tempCalibration.toleranceY);
  delay(1000);
}

void displayCalibrationStatus() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println(calInstructions[calStage]);
  
  tft.setCursor(10, 40);
  if(calStage == CAL_DONE){
    // Show final calibration values (not yet saved)
    tft.print("defX:"); tft.print(tempCalibration.defaultX);
    tft.print(" defY:"); tft.println(tempCalibration.defaultY);
    tft.print("maxX:"); tft.print(tempCalibration.maxX);
    tft.print(" maxY:"); tft.println(tempCalibration.maxY);
    tft.print("minX:"); tft.print(tempCalibration.minX);
    tft.print(" minY:"); tft.println(tempCalibration.minY);
    tft.println();
    tft.println("Press TURN to confirm");
  } else {
    tft.print("X: ");
    tft.print(analogRead(TURN_PIN));
    tft.print("  Y: ");
    tft.println(analogRead(THROTTLE_PIN));
  }
}

void displayBoatCalibrationHeader(){
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.println(boatCalInstructions[boatCalStage]);
}


uint16_t interpolateColor(uint16_t color1, uint16_t color2, float fraction) {
  // BGR565 format (blue and red swapped)
  uint8_t b1 = (color1 >> 11) & 0x1F;
  uint8_t g1 = (color1 >> 5) & 0x3F;
  uint8_t r1 = color1 & 0x1F;

  uint8_t b2 = (color2 >> 11) & 0x1F;
  uint8_t g2 = (color2 >> 5) & 0x3F;
  uint8_t r2 = color2 & 0x1F;

  uint8_t b = b1 + (b2 - b1) * fraction;
  uint8_t g = g1 + (g2 - g1) * fraction;
  uint8_t r = r1 + (r2 - r1) * fraction;

  //BGR565
  return (b << 11) | (g << 5) | r;
}

int old_throttle = 0;
int old_turn = 0;

void drawHomeScreen(bool force = false) {
  int throttle = currentControlPacket.control.power;
  int turn = currentControlPacket.control.turn;

  // Only clear throttle bar area if throttle changed
  if(throttle != old_throttle || force) {
    tft.fillRect(0, 24, 157, 20, 0x0000);
    old_throttle = throttle;

    int color;
    if (throttle > 0){
      // Green (0x07E0) to Red (0x001F) in BGR565
      color = interpolateColor(0x07E0, 0x001F, (float)(throttle-1)/1000.0);
    }
    else{
      // Green (0x07E0) to Red (0x001F) for negative
      color = interpolateColor(0x051F, 0x029F, (float)(-throttle)/1000.0);
    }

    tft.drawRoundRect(56, 24, 100, 20, 3, 0xFFFF);

    int rectWidth = map(abs(throttle), 0 , 1000, 0 , 96);

    tft.fillRoundRect(58, 26, rectWidth, 16, 3, color);
    

    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString(String(throttle), 28, 34);

  }

  if(turn !=old_turn || force){
    tft.fillRect(0, 68, 157, 50, 0x0000);
    old_turn = turn;
    
    tft.drawRoundRect(8, 88, 145, 23, 11, 0xFFFF);
    int circle_pos = map(turn, -1000, 1000, 17, 144);
    tft.fillCircle(circle_pos, 99, 9, 0xFFFF);

    tft.setFreeFont(NULL);
    tft.setTextFont(1);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString(String(map(turn, -1000, 1000, -1000, 1000 )), 80, 76);
  }
}

int last_frame_time = 1000;
int frame_delay = 50;

int last_calibration_sample_time = 1000;
int calibration_sample_delay = 100;

ModeType last_iteration_mode = CALIBRATION_STICKS; // Start with a different mode to force screen update on first loop

void loop(){
  throttle_button.loop();
  turn_button.loop();
  int time = millis();
  bool throttle_pressed = !throttle_button.getState();
  bool turn_pressed = !turn_button.getState();
  if(!throttle_pressed)  {
    throttle_pressed_time = 0;
  }
  if(!turn_pressed){
    turn_pressed_time = 0;
  }

  /*
  Serial.print("Mode = ");
  switch(mode){
    case NORMAL:
      Serial.println("NORMAL");
      break;
    case CALIBRATION_BOAT:
      Serial.println("CALIBRATION_BOAT");
      break;
    case CALIBRATION_STICKS:
      Serial.println("CALIBRATION_STICKS");
      break;
  }
  */

  if(mode == NORMAL){

    if(last_iteration_mode != NORMAL){
      tft.fillScreen(TFT_BLACK);
    }
    struct_message_send package = getControl();

    if(turn_pressed && turn_pressed_time > 2000){
      mode = CALIBRATION_STICKS;
      calStage = CAL_DEFAULT;
      waitingForRelease = true; // User is holding button, require release first
      package.control.power = 0;
      package.control.turn = 0;
      turn_pressed_time = 0;
    }
    else if(throttle_pressed && throttle_pressed_time > 2000){
      mode = CALIBRATION_BOAT;
      waitingForRelease = true; // User is holding button, require release first
      package.control.power = 0;
      package.control.turn = 0;
      throttle_pressed_time = 0;
    }

    currentControlPacket = package;
    if (millis() - last_frame_time >= frame_delay) {
      last_frame_time = millis();
      if(last_iteration_mode != NORMAL){
        drawHomeScreen(true); // Force full redraw when coming back from calibration
      }
      else{
        drawHomeScreen();
      }
    }
    sendData((uint8_t*) &currentControlPacket, sizeof(currentControlPacket));
    last_iteration_mode = NORMAL;
  }
  else if (mode == CALIBRATION_STICKS){
    //Serial.println("In stick calibration mode");

    displayCalibrationStatus();

    // Check if buttons are released to reset waitingForRelease
    bool anyButtonHeld = !throttle_button.getState() || !turn_button.getState();
    if(!anyButtonHeld && waitingForRelease){
      waitingForRelease = false;
      Serial.println("Button released, ready for next input");
    }

    // Determine which button to use based on current stage
    // CAL_DEFAULT -> any button, X variables -> throttle button, Y variables -> turn button
    bool useThrottleButton = (calStage == CAL_MAX_X || calStage == CAL_MIN_X);
    bool useAnyButton = (calStage == CAL_DEFAULT);
    bool useVerifyButton = (calStage == CAL_DONE);
    bool acceptPressed = !waitingForRelease && (
                         useAnyButton ? (throttle_button.isPressed() || turn_button.isPressed()) :
                         useVerifyButton ? throttle_button.isPressed() : 
                         useThrottleButton ? throttle_button.isPressed() : turn_button.isPressed());

    // use opposite button for verification at the end to avoid accidental confirmation during calibration
    if((useThrottleButton && throttle_pressed && throttle_pressed_time > 2000) || (!useThrottleButton && turn_pressed && turn_pressed_time > 2000)){
      Serial.println(useThrottleButton ? "Throttle" : "Turn");
      Serial.println(throttle_pressed);
      Serial.println(turn_pressed);
      Serial.println(throttle_pressed_time);
      Serial.println(turn_pressed_time);
      Serial.println("Calibration cancelled, returning to normal mode...");
      mode = NORMAL;
      calStage = CAL_DEFAULT; // Reset for next time
      throttle_pressed_time = 0;
      waitingForRelease = false;
    }
    
    // Accept current value with appropriate button press
    if(acceptPressed){
      int valueX = analogRead(TURN_PIN);
      int valueY = analogRead(THROTTLE_PIN);

      switch(calStage){
        case CAL_DEFAULT:
          Serial.println("CAL_DEFAULT: Starting default sampling...");
          sampleDefaultCalibration();
          time = millis(); // Reset time to avoid immediate next stage transition
          calStage = CAL_MAX_X;
          waitingForRelease = true;
          break;
        case CAL_MAX_X:
          Serial.print("CAL_MAX_X: maxX = ");
          Serial.println(valueX);
          tempCalibration.maxX = valueX;
          calStage = CAL_MIN_X;
          waitingForRelease = true;
          break;
        case CAL_MIN_X:
          Serial.print("CAL_MIN_X: minX = ");
          Serial.println(valueX);
          tempCalibration.minX = valueX;
          calStage = CAL_MAX_Y;
          waitingForRelease = true;
          break;
        case CAL_MAX_Y:
          Serial.print("CAL_MAX_Y: maxY = ");
          Serial.println(valueY);
          tempCalibration.maxY = valueY;
          calStage = CAL_MIN_Y;
          waitingForRelease = true;
          break;
        case CAL_MIN_Y:
          Serial.print("CAL_MIN_Y: minY = ");
          Serial.println(valueY);
          tempCalibration.minY = valueY;
          calStage = CAL_DONE;
          waitingForRelease = true;
          break;
        case CAL_DONE:
          Serial.println("CAL_DONE: Saving calibration...");
          // Apply and save calibration on confirmation
          currentCalibration = tempCalibration;
          saveCalibration();
          tft.fillScreen(TFT_BLACK);
          mode = NORMAL;
          calStage = CAL_DEFAULT; // Reset for next time
          waitingForRelease = true; // Reset since we're leaving calibration mode
          break;
      }
      delay(300); // Debounce
    }
    last_iteration_mode = CALIBRATION_STICKS;

  }
  else if (mode == CALIBRATION_BOAT){
    if(last_iteration_mode != CALIBRATION_BOAT){
      displayBoatCalibrationHeader();
      drawHomeScreen(true);
    }
    //Serial.println("In boat calibration mode");
    if(turn_pressed && turn_pressed_time > 2000){
      mode = NORMAL;
      turn_pressed_time = 0;
      waitingForRelease = true;
    }

    bool useThrottleButton = (boatCalStage == SERVO_LOWEST || boatCalStage == SERVO_MIDDLE || boatCalStage == SERVO_HIGHEST);
    bool acceptPressed = !waitingForRelease && (useThrottleButton ? throttle_pressed : turn_pressed);

    if(waitingForRelease && !throttle_pressed && !turn_pressed){
      waitingForRelease = false;
      Serial.println("Button released, ready for next input");
    }

    if(acceptPressed){
      switch(boatCalStage){
        case SERVO_LOWEST:
          Serial.print("SERVO_LOWEST ACCEPTED ");
          tempBoatCalibration.servo_lowest = currentControlPacket.control.turn;
          boatCalStage = SERVO_MIDDLE;
          waitingForRelease = true;
          currentControlPacket.control.turn = 0;

          break;
        case SERVO_MIDDLE:
          tempBoatCalibration.servo_middle = currentControlPacket.control.turn;
          boatCalStage = SERVO_HIGHEST;
          waitingForRelease = true;
          currentControlPacket.control.turn = 0;

          break;
        case SERVO_HIGHEST:
          tempBoatCalibration.servo_highest = currentControlPacket.control.turn;
          boatCalStage = MOTOR_LOWEST;
          waitingForRelease = true;
          currentControlPacket.control.turn = 0;

          break;
        case MOTOR_LOWEST:
          tempBoatCalibration.motor_lowest = currentControlPacket.control.power;
          boatCalStage = MOTOR_HIGHEST;
          waitingForRelease = true;
          currentControlPacket.control.power = 0;

          break;
        case MOTOR_HIGHEST:
          tempBoatCalibration.motor_highest = currentControlPacket.control.power;
          boatCalStage = MOTOR_OFF;
          waitingForRelease = true;
          currentControlPacket.control.power = 0;

          break;
        case MOTOR_OFF:
          tempBoatCalibration.motor_off = currentControlPacket.control.power;
          tempBoatCalibration.type = CALIBRATION; // Set packet type before copying
          boatCalStage = SERVO_LOWEST; // Reset for next time
          waitingForRelease = true;
          tft.fillScreen(TFT_BLACK);
          mode = NORMAL;
          boat_calibration_packet.calibration = tempBoatCalibration; 
          sendData((uint8_t*) &boat_calibration_packet, sizeof(boat_calibration_packet));
          Serial.println("Sent boat calibration data:");
          Serial.printf("Packet Type: %d\n", boat_calibration_packet.type);
          Serial.print("Servo Lowest: ");Serial.println(boat_calibration_packet.calibration.servo_lowest);
          Serial.print("Servo Middle: ");Serial.println(boat_calibration_packet.calibration.servo_middle);
          Serial.print("Servo Highest: ");Serial.println(boat_calibration_packet.calibration.servo_highest);
          Serial.print("Motor Lowest: ");Serial.println(boat_calibration_packet.calibration.motor_lowest);
          Serial.print("Motor Highest: ");Serial.println(boat_calibration_packet.calibration.motor_highest);
          Serial.print("Motor Off: ");Serial.println(boat_calibration_packet.calibration.motor_off);
          
          currentControlPacket.control.power = 0;
          Serial.println("Boat calibration complete!");
      }
      displayBoatCalibrationHeader();
      drawHomeScreen(true);
      delay(300);
    }
    else{
      int changeX = getX()/10;
      int changeY = getY()/30;

      if(millis() - last_calibration_sample_time >= calibration_sample_delay){

        last_calibration_sample_time = millis();
        if(changeX != 0 || changeY != 0){
          if(boatCalStage == SERVO_LOWEST || boatCalStage == SERVO_MIDDLE || boatCalStage == SERVO_HIGHEST){
            currentControlPacket.control.turn += changeX;
          }
          else{
            currentControlPacket.control.power += changeY;
          }
          // Constrain values to expected ranges (this would depend on your specific hardware requirements)
          currentControlPacket.control.turn = constrain(currentControlPacket.control.turn, -1000, 1000);
          currentControlPacket.control.power = constrain(currentControlPacket.control.power, -1000, 1000);
        }

      }

      drawHomeScreen(); 
      sendData((uint8_t*) &currentControlPacket, sizeof(currentControlPacket));
    }


    last_iteration_mode = CALIBRATION_BOAT;
  }
  else{
    //wtf
  }


  /*
  Serial.print(" Throttle Pressed: ");
  Serial.println(throttle_pressed_time);
  Serial.print(" Turn Pressed: ");
  Serial.println(turn_pressed_time);
  */

  if(throttle_pressed){
    throttle_pressed_time += millis() - time;
  }
  else{
    throttle_pressed_time = 0;
  }
  if(turn_pressed){
    turn_pressed_time += millis() - time;
  }
  else{
    turn_pressed_time = 0;
  }
}