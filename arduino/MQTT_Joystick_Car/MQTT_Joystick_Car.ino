#include <vector>
#include <MQTT.h>
#include <WiFi.h>
#ifdef __SMCE__
#include <OV767X.h>
#endif
#include <Smartcar.h>

MQTTClient mqtt;
WiFiClient net;

//wifi credentials
const char ssid[] = "***";
const char pass[] = "****";

// INFRARED SENSORS
const auto FRONT_IR_PIN = 0;
const auto LEFT_IR_PIN = 1;
const auto RIGHT_IR_PIN = 2;
const auto BACK_IR_PIN = 3;
const auto FRONT_INNER_LEFT_IR_PIN = 40;
const auto FRONT_OUTER_LEFT_IR_PIN = 41;
const auto FRONT_INNER_RIGHT_IR_PIN = 42;
const auto FRONT_OUTER_RIGHT_IR_PIN = 43;
const auto BACK_INNER_LEFT_IR_PIN = 50;
const auto BACK_OUTER_LEFT_IR_PIN = 51;
const auto BACK_INNER_RIGHT_IR_PIN = 52;
const auto BACK_OUTER_RIGHT_IR_PIN = 53;
const auto FRONT_STRAIGHT_IR_PIN = 44;
const auto FRONT_STRAIGHT_INNER_LEFT_IR_PIN = 45;
const auto FRONT_STRAIGHT_OUTER_LEFT_IR_PIN = 46;
const auto FRONT_STRAIGHT_INNER_RIGHT_IR_PIN = 47;
const auto FRONT_STRAIGHT_OUTER_RIGHT_IR_PIN = 48;
const auto BACK_STRAIGHT_IR_PIN = 54;
const auto BACK_STRAIGHT_INNER_LEFT_IR_PIN = 55;
const auto BACK_STRAIGHT_OUTER_LEFT_IR_PIN = 56;
const auto BACK_STRAIGHT_OUTER_RIGHT_IR_PIN = 57;
const auto BACK_STRAIGHT_INNER_RIGHT_IR_PIN = 58;

// Topic keywords
const String motorSpeed = "DIT133Group13/Speed";
const String keepSpeed = "DIT133Group13/CruiseControl";
const String rotateLeft = "DIT133Group13/RotateLeft";
const String rotateRight = "DIT133Group13/RotateRight";
const String FindRight = "DIT133Group13/FindRight";
const String FindLeft = "DIT133Group13/FindLeft";


//instantiation of car components
ArduinoRuntime arduinoRuntime;
BrushedMotor leftMotor(arduinoRuntime, smartcarlib::pins::v2::leftMotorPins);
BrushedMotor rightMotor(arduinoRuntime, smartcarlib::pins::v2::rightMotorPins);
DifferentialControl control(leftMotor, rightMotor);
GY50 gyroscope(arduinoRuntime, 37);
const auto pulsesPerMeter = 600;
DirectionlessOdometer leftOdometer(arduinoRuntime, smartcarlib::pins::v2::leftOdometerPin,[]() { leftOdometer.update(); }, pulsesPerMeter);
DirectionlessOdometer rightOdometer(arduinoRuntime, smartcarlib::pins::v2::rightOdometerPin,[]() { rightOdometer.update(); }, pulsesPerMeter);


//instantiates infrared sensors
GP2D120 frontIR(arduinoRuntime, FRONT_IR_PIN); // measure distances between 5 and 25 centimeters
GP2D120 backIR(arduinoRuntime, BACK_IR_PIN);
GP2D120 leftIR(arduinoRuntime, LEFT_IR_PIN);
GP2D120 rightIR(arduinoRuntime, RIGHT_IR_PIN);
GP2D120 frontInnerLeftIR(arduinoRuntime, FRONT_INNER_LEFT_IR_PIN);
GP2D120 frontOuterLeftIR(arduinoRuntime, FRONT_OUTER_LEFT_IR_PIN);
GP2D120 frontInnerRightIR(arduinoRuntime, FRONT_INNER_RIGHT_IR_PIN);
GP2D120 frontOuterRightIR(arduinoRuntime, FRONT_OUTER_RIGHT_IR_PIN);
GP2D120 backInnerLeftIR(arduinoRuntime, BACK_INNER_LEFT_IR_PIN);
GP2D120 backOuterLeftIR(arduinoRuntime, BACK_OUTER_LEFT_IR_PIN);
GP2D120 backInnerRightIR(arduinoRuntime, BACK_INNER_RIGHT_IR_PIN);
GP2D120 backOuterRightIR(arduinoRuntime, BACK_OUTER_RIGHT_IR_PIN);
GP2D120 frontStraightIR(arduinoRuntime, FRONT_STRAIGHT_IR_PIN);
GP2D120 frontStraightInnerLeftIR(arduinoRuntime, FRONT_STRAIGHT_INNER_LEFT_IR_PIN);
GP2D120 frontStraightOuterLeftIR(arduinoRuntime, FRONT_STRAIGHT_OUTER_LEFT_IR_PIN);
GP2D120 frontStraightInnerRightIR(arduinoRuntime, FRONT_STRAIGHT_INNER_RIGHT_IR_PIN);
GP2D120 frontStraightOuterRightIR(arduinoRuntime, FRONT_STRAIGHT_OUTER_RIGHT_IR_PIN);
GP2D120 backStraightIR(arduinoRuntime, BACK_STRAIGHT_IR_PIN);
GP2D120 backStraightInnerLeftIR(arduinoRuntime, BACK_STRAIGHT_INNER_LEFT_IR_PIN);
GP2D120 backStraightOuterLeftIR(arduinoRuntime, BACK_STRAIGHT_OUTER_LEFT_IR_PIN);
GP2D120 backStraightInnerRightIR(arduinoRuntime, BACK_STRAIGHT_OUTER_RIGHT_IR_PIN);
GP2D120 backStraightOuterRightIR(arduinoRuntime, BACK_STRAIGHT_INNER_RIGHT_IR_PIN);

// Create car object and apply all modules.
SmartCar car(arduinoRuntime, control, gyroscope, leftOdometer, rightOdometer);

// Global Car-Control variables
boolean blockForward = false; // Prevents moving forward
boolean blockReverse = false; // Prevents reversing
boolean cliffFrontLeft = false; // Cliff detected on car's front - left side
boolean cliffFrontRight = false; // Cliff detected on car's front - right side
boolean cliffBackLeft = false; // Cliff detected on car's back - left side
boolean cliffBackRight = false; // Cliff detected on car's back - right side
boolean cruiseControl = false; // Cruise control on or off
boolean rotateCar = false; // Is car currently rotating
int lmSpeed = 0; // Current left motorspeed
int rmSpeed = 0; // Current right motorspeed


//Define MQTT Broker
const auto mqttBrokerUrl = "127.0.0.1";
const auto maxDistance = 400;


//main loop
void loop() {

  // put your main code here, to run repeatedly:
  if (mqtt.connected()) {
    mqtt.loop();
  }
  // Verify cars surroundings
  checkSensors();

  // Keeps car in requested input.
  maintainSpeed();

  // Avoid over-using the CPU if we are running in the emulator
#ifdef __SMCE__
  delay(1);
#endif
}

/**
 * Checks the sensors if it's possible to move the vehicle back or forward. Prevents movement if car is blocked.
 */

void checkSensors() {
  // Front and Back sensor check
  objectDetection();
  cliffDetection();
  
  /**
   * When the car tries to move towards an obstacle it will be stopped:
   * When the car is about to drive off a cliff it will be stopped
   * When the car is about to turn off a cliff an opposite turn will be forced
   * This only applies when the user is driving the car, thus Rotating the car is not affected.
   */
   if(!rotateCar)  {
    if (lmSpeed+rmSpeed > 0 && blockForward) {         // If car is moving forward && path is blocked or cliff
      
      car.setSpeed(0);
    } else if (lmSpeed+rmSpeed < 0 && blockReverse) {  // if car is moving backwards and path is blocked or cliff
      car.setSpeed(0);
    } else if (((cliffFrontLeft != cliffFrontRight) && (rmSpeed > 0 && cliffFrontLeft)) || (cliffBackLeft != cliffBackRight && (rmSpeed < 0 && cliffBackLeft))) { // Turns right car to avoid cliff
      rightMotor.setSpeed(0);
      lmSpeed /= 4;
    } else if (((cliffFrontLeft != cliffFrontRight) && (lmSpeed > 0 && cliffFrontRight)) || (cliffBackLeft != cliffBackRight && (lmSpeed < 0 && cliffBackRight))) { // Turns car left to avoid cliff
      leftMotor.setSpeed(0);
      rmSpeed /= 4;
    }
   }
}

/**
 * This checks if there's an obstacle infront or behind the car.
 */
void objectDetection() {
  int minDistance = 1;
  
  // Checks if direction
  blockForward = frontStraightIR.getDistance() || frontStraightInnerLeftIR.getDistance() || frontStraightOuterLeftIR.getDistance() || 
                 frontStraightInnerRightIR.getDistance() || frontStraightOuterRightIR.getDistance() > minDistance;
  blockReverse = backStraightIR.getDistance() || backStraightInnerLeftIR.getDistance() || backStraightOuterLeftIR.getDistance() || 
                 backStraightInnerRightIR.getDistance() || backStraightOuterRightIR.getDistance() > minDistance;
                 Serial.println(blockForward);  
}

/**
 * The side sensors are constantly detecting the ground which means that they will always return a value above 0.
 * If they detect nothing it means that there is a cliff ahead and we can use that information to avoid cliffs.
 */

 void cliffDetection() {
  cliffFrontLeft = frontOuterLeftIR.getDistance() == 0 || frontInnerLeftIR.getDistance() == 0;
  cliffFrontRight = frontOuterRightIR.getDistance() == 0 || frontInnerRightIR.getDistance() == 0;
  cliffBackLeft = backOuterLeftIR.getDistance() == 0 || backInnerLeftIR.getDistance() == 0;
  cliffBackRight = backOuterRightIR.getDistance() == 0 || backInnerRightIR.getDistance() == 0;
  blockForward = blockForward || (frontIR.getDistance() == 0);
  blockReverse = blockReverse || (backIR.getDistance() == 0);
 }

/**
   Set the speed of left/right motor of car.
   Setting different speed will turn car and move it forward/backward
   @PARAM motor - Information about which motor's speed we're changing
   @PARAM newSpeed - The speed that will be set on the motor.
*/
void setMotorSpeed(String message) {

  String slash = "/";
  int slashIndex = message.indexOf(slash);
  int leftSpeed = (message.substring(0, slashIndex -1)).toInt();
  int rightSpeed = (message.substring(slashIndex +1, message.length()-1)).toInt();
  double directionOfTravel = leftSpeed + rightSpeed;
  
  lmSpeed = rmSpeed = 0;
  
  rotateCar = false;
  if ((directionOfTravel > 0 && !blockForward) || (directionOfTravel < 0 && !blockReverse) || directionOfTravel == 0) {
    leftMotor.setSpeed(leftSpeed);
    rightMotor.setSpeed(rightSpeed);
    lmSpeed = leftSpeed;  
    rmSpeed = rightSpeed;  
  }
}

/**
   Still standing rotation. Checks if car is (almost) standing still before rotating.
*/

void stillStandingRotation(String direction, int toggle) {
  rotateCar = (toggle == 1 && lmSpeed + rmSpeed == 0);
  if (rotateCar && direction == rotateLeft) {
    lmSpeed = -50;
    rmSpeed = 50;
  } else if(rotateCar && direction == rotateRight) {
    lmSpeed = 50;
    rmSpeed = -50;
  } else { // Stops rotation.
    car.overrideMotorSpeed(0, 0);
  }
}

/**
   This keeps rotations going as long as required without multi-threading
*/

void maintainSpeed() {
  if(rotateCar) {
    car.overrideMotorSpeed(lmSpeed, rmSpeed);
  } 
}
//Rotates the car until none of the front sensors detect any obstacle
void findPath(String Side){
    if (Side == FindLeft){
      while(frontStraightIR.getDistance() != 0 || frontStraightInnerRightIR.getDistance() != 0 || frontStraightInnerLeftIR.getDistance() != 0 || frontStraightOuterRightIR.getDistance() != 0 || frontStraightOuterLeftIR.getDistance() != 0){
        leftMotor.setSpeed(-10);
        rightMotor.setSpeed(10);
        }
    } 
    else if(Side == FindRight){
      while(frontStraightIR.getDistance() != 0 || frontStraightInnerRightIR.getDistance() != 0 || frontStraightInnerLeftIR.getDistance() != 0 || frontStraightOuterRightIR.getDistance() != 0 || frontStraightOuterLeftIR.getDistance() != 0){
        leftMotor.setSpeed(10);
        rightMotor.setSpeed(-10);
        }
    }
    delay(550);
    leftMotor.setSpeed(0);
    rightMotor.setSpeed(0);
}

//method to set up the car, MQTT client, wifi connection and MQTT message handling
void setup() {
  Serial.begin(9600);
  blockForward = blockReverse = false;


  WiFi.begin(ssid, pass);
  mqtt.begin(mqttBrokerUrl, 1883, net);

  //WIFI connection while loop
  Serial.println("Connecting to WiFi...");
  auto wifiStatus = WiFi.status();
  while (wifiStatus != WL_CONNECTED && wifiStatus != WL_NO_SHIELD) {
    delay(1000);
    wifiStatus = WiFi.status();
  }

  //MQTT Broker connection while loop
  Serial.println("Connecting to MQTT broker");
  while (!mqtt.connect("arduino", "public", "public")) {
    delay(1000);
  }
  
  mqtt.subscribe("DIT133Group13/#", 1);


  /**
     Receives and interprets message from MQTT Broker
     @PARAM topic - Is the attribute we are changing.
     @PARAM message - Is the new value we're changing it to.
  */

  mqtt.onMessage([](String topic, String message) {

    // If the topic sent is regarding motor-speed and the car is not currently rotating.
    if (!rotateCar && topic == motorSpeed) {
      setMotorSpeed(message);
    } else if (topic == rotateLeft || topic == rotateRight) { // If message is toggling rotations
      stillStandingRotation(topic, message.toInt());
    } else if (topic == FindLeft || topic == FindRight){
      findPath(topic);
      }
  });

  // Keep emulator from overloading.
  delay(1);
}
