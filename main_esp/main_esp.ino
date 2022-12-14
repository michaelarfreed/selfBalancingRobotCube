#include <Adafruit_VL53L0X.h>
#include <ArduinoJson.h>
#include <JY901.h>
#include <PID_v1.h>
#include <Servo.h>
#include <WiFi.h>
#include <Wire.h>
#include "ESPAsyncWebServer.h"

// PID fun-ness
double Input, Output, Setpoint; //Input is sensor reading (current angle); Setpoint is desired angle; Output is what PID says angle should be
double kp = 0.9, ki = 0, kd = 0;    // tuning parameters
PID myPID(&Input, &Output, &Setpoint,kp,ki,kd, P_ON_E, DIRECT);

// // motors
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 14;

// esc
int escPin = 27;

int escPwmMin = 100;
int escPwmMed = 140;
int escPwmMax = 180;

int minAngle = -10;
int maxAngle = 10;

int escSig = 0;

Servo ESC;

// PWM stuff
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 0;

// constants associated with IMU
float acc     [3];
float gyro    [3];
float angles  [3];
float mag     [3];
short dstatus [4];

float gpsHeight;
float gpsYaw;
float gpsVelocity;

float lpressure;
float laltitude;

// lidar
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// wifi communication with matlab
const char* ssid = "Michaela-ESP-Access";  
const char* password = "123456789"; 

AsyncWebServer server(80);
DynamicJsonDocument jsonBuffer(256);

void setup() 
{
  Serial.begin(115200);
  while (! Serial) {
    delay(1);
  }
  // set pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  Serial.println("printing!");

  // configure LED PWM functionality
  ledcSetup(pwmChannel, freq, resolution);
  Serial.println("printing 2!");
  ledcAttachPin(enable1Pin, pwmChannel);
  Serial.println("printing 3!");

  // ESC
  ESC.attach(escPin);
  ESC.write(escSig);
  
  // setup accelerometer
  JY901.StartIIC();
  Serial.println("printing 4!");

  Input = get_angle(angles);
  Serial.println("printing 5!");
  Setpoint = 0;
  myPID.SetMode(AUTOMATIC); 
  Serial.println("printing 6!");
  myPID.SetOutputLimits(-255,255);
  Serial.println("exiting setup");
} 

void loop() 
{
  // get the angle
  Input = get_angle(angles);
  myPID.Compute();
  Serial.print(" pid_output:");Serial.print(Output);
//  set_motor(Output);  // convert the value to a speed
  set_gimbal_motor(Output);
  Serial.print(" escSig:");Serial.print(escSig);
  // print data for debugging
  Serial.print(" Angle_x:"); Serial.print(angles[0]); Serial.print(" Angle_y:"); Serial.print(angles[1]); Serial.print(" Angle_z:"); Serial.print(angles[2]); //Serial.print("\n"); 
  Serial.print("\n");
              
  delay(5);
}

void get_acc(float (& acc) [3])
{
  JY901.GetAcc();
  acc[0] = (float)JY901.stcAcc.a[0]/32768*16;
  acc[1] = (float)JY901.stcAcc.a[1]/32768*16;
  acc[2] = (float)JY901.stcAcc.a[2]/32768*16;

}

void get_gyro(float (& gyro) [3])
{
  JY901.GetGyro();
  gyro[0] = (float)JY901.stcGyro.w[0]/32768*2000;
  gyro[1] = (float)JY901.stcGyro.w[1]/32768*2000;
  gyro[2] = (float)JY901.stcGyro.w[2]/32768*2000;

}

double get_angle( float (& angles) [3])
{
  get_angles(angles);
  return angles[1];
}

void get_angles(float (& angle) [3])
{
  JY901.GetAngle();
  angle[0] = (float)JY901.stcAngle.Angle[0]/32768*180;
  angle[1] = (float)JY901.stcAngle.Angle[1]/32768*180;
  angle[2] = (float)JY901.stcAngle.Angle[2]/32768*180;

}

void get_mag(float (& mag) [3])
{
  JY901.GetMag();
  mag[0] = (float)JY901.stcMag.h[0];
  mag[1] = (float)JY901.stcMag.h[1];
  mag[2] = (float)JY901.stcMag.h[2];

}

//String get_time()
//{
//  JY901.GetTime();
//  return (JY901.stcTime.ucYear + "-" + JY901.stcTime.ucMonth.toString() + "-" + JY901.stcTime.ucDay.toString() + " " + JY901.stcTime.ucHour + ":" + JY901.stcTime.ucMinute + ":" + JY901.stcTime.ucSecond + "." + JY901.stcTime.usMiliSecond/1000);
//
//}

void get_gps(float & gpsHeight, float & gpsYaw, float & gpsVelocity) 
{
  JY901.GetGPSV();
  gpsHeight =   (float)JY901.stcGPSV.sGPSHeight/10;
  gpsYaw =      (float)JY901.stcGPSV.sGPSYaw/10;
  gpsVelocity = (float)JY901.stcGPSV.lGPSVelocity/1000;  

}

void get_dstatus(short (&dstatus) [4]) 
{
  JY901.GetDStatus();
  dstatus[0] = (short)JY901.stcDStatus.sDStatus[0];
  dstatus[1] = (short)JY901.stcDStatus.sDStatus[1];
  dstatus[2] = (short)JY901.stcDStatus.sDStatus[2];
  dstatus[3] = (short)JY901.stcDStatus.sDStatus[3];
}

void get_pressure(float & lpressure, float & laltitude)
{
  JY901.GetPress();
  lpressure = JY901.stcPress.lPressure;
  laltitude = (float)JY901.stcPress.lAltitude/100;

}

void server_function(AsyncWebServerRequest * request, uint8_t *data_in, size_t len, size_t index, size_t total) 
{
  
  // Using the webwrite() command in matlab you can send a value that can be read and associated witha  specific action 
  String msg = String((char *)data_in, len); // takes the given value 
  Serial.print("received message: ");   Serial.println(msg);
  
  // parse the json message. the format is {"theta1":[float]; "theta2":[float]; "pen":[int]}
  DeserializationError error = deserializeJson(jsonBuffer, msg); 
  if (error) {
    request->send_P(200, "text/plain", "-2"); 
    Serial.println("error in json parsing");
    return;
  }
}

void set_motor(double dutyCycle) 
{
  int dc = int(dutyCycle);
  if (dutyCycle > 0) // spin forwards
  {
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
  } else { // spin backwards
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
    dc = abs(dc);
  }
//  dc = map(dc, 0, 45, 220, 255); 
  dc = 255;
  Serial.print(" duty_cycle: "); Serial.print(dc);
  // set duty-cycle
  ledcWrite(pwmChannel, dc);
}

void set_gimbal_motor(double angle)
{
  escSig = map(angle, minAngle, maxAngle, escPwmMin, escPwmMax);
  escSig = 180;
  ESC.write(escSig);
}
