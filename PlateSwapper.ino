#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <VL53L1X.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
const char* ssid = "SSID";
const char* password = "PASS";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

VL53L1X sensor;
int currentDistance = 0;

WebServer server(80);

// Stepper motor pins
#define EN_PIN_PL   25
#define STEP_PIN_PL 33
#define DIR_PIN_PL  32

#define EN_PIN_XL   14
#define STEP_PIN_XL 27
#define DIR_PIN_XL  26

#define EN_PIN_YL   15
#define STEP_PIN_YL 13
#define DIR_PIN_YL  12

// Endstop pins
#define ENDSTOP_PL 18
#define ENDSTOP_YL 5
#define ENDSTOP_XL 19

AccelStepper stepperPL(AccelStepper::DRIVER, STEP_PIN_PL, DIR_PIN_PL);
AccelStepper stepperYL(AccelStepper::DRIVER, STEP_PIN_YL, DIR_PIN_YL);
AccelStepper stepperXL(AccelStepper::DRIVER, STEP_PIN_XL, DIR_PIN_XL);

float motorSpeed = 3000;
float motorAccel = 2000;
int plateNumber = 0;

void showOnOLED(String msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.println(msg);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  showOnOLED("Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  showOnOLED("IP:\n" + WiFi.localIP().toString());

  pinMode(EN_PIN_PL, OUTPUT); pinMode(EN_PIN_YL, OUTPUT); pinMode(EN_PIN_XL, OUTPUT);
  digitalWrite(EN_PIN_PL, LOW); digitalWrite(EN_PIN_YL, LOW); digitalWrite(EN_PIN_XL, LOW);
ArduinoOTA.begin();
  pinMode(ENDSTOP_PL, INPUT_PULLUP);
  pinMode(ENDSTOP_YL, INPUT_PULLUP);
  pinMode(ENDSTOP_XL, INPUT_PULLUP);

  setStepperParams();

  sensor.setTimeout(500);
  if (sensor.init()) {
    sensor.setDistanceMode(VL53L1X::Long);
    sensor.setMeasurementTimingBudget(50000);
    sensor.startContinuous(50);
  }

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/home", handleHome);
  server.on("/loadplate", handleLoadPlate);
  server.on("/removeplate", handleRemovePlate);
  server.on("/status", handleStatus);
  server.on("/updateparams", handleUpdateParams);
  server.on("/plate", handlePlateNumber);
  server.on("/endstops", handleEndstops);
  server.on("/distance", handleDistance);
  server.on("/openchamber", handleOpenChamber);
  server.on("/closechamber", handleCloseChamber);
  server.on("/loadtoprinter", handleLoadToPrinter);
  server.on("/unloadfromprinter", handleUnloadFromPrinter);
  server.begin();
}

void loop() {
    ArduinoOTA.handle();
  currentDistance = sensor.read();
  server.handleClient();
  stepperPL.run();
  stepperYL.run();
  stepperXL.run();
}

void setStepperParams() {
  if (motorSpeed < 500) motorSpeed = 500;
  if (motorAccel < 300) motorAccel = 300;
  stepperPL.setMaxSpeed(500);
  stepperPL.setAcceleration(300);
  stepperYL.setMaxSpeed(motorSpeed);
  stepperYL.setAcceleration(motorAccel);
  stepperXL.setMaxSpeed(motorSpeed);
  stepperXL.setAcceleration(motorAccel);
}

void handleUpdateParams() {
  if (server.hasArg("speed")) motorSpeed = server.arg("speed").toFloat();
  if (server.hasArg("accel")) motorAccel = server.arg("accel").toFloat();
  setStepperParams();
  server.send(200, "text/plain", "Params Updated");
}

void handleMove() {
  showOnOLED("Manual Move");
  setStepperParams();
  if (server.hasArg("pl")) stepperPL.moveTo(server.arg("pl").toInt());
  if (server.hasArg("yl")) stepperYL.moveTo(server.arg("yl").toInt());
  if (server.hasArg("xl")) stepperXL.moveTo(server.arg("xl").toInt());
  while (stepperPL.distanceToGo() || stepperYL.distanceToGo() || stepperXL.distanceToGo()) {
    stepperPL.run(); stepperYL.run(); stepperXL.run();
  }
  server.send(200, "text/plain", "Motors Moved");
}

void homeMotor(AccelStepper &stepper, int pin) {
  stepper.moveTo(stepper.currentPosition() - 100000);
  while (digitalRead(pin)) stepper.run();
  stepper.setCurrentPosition(0);
}

void handleHome() {
  showOnOLED("Homing...");
  homeMotor(stepperPL, ENDSTOP_PL);
  homeMotor(stepperXL, ENDSTOP_XL);
  homeMotor(stepperYL, ENDSTOP_YL);
  plateNumber = 0;
  server.send(200, "text/plain", "Homing Done");
}

void handleLoadPlate() {
  showOnOLED("Load Plate");
  stepperYL.moveTo(120); stepperXL.moveTo(500); //pozycja 0
  while (stepperYL.distanceToGo() || stepperXL.distanceToGo()) { stepperYL.run(); stepperXL.run(); }
  stepperPL.moveTo(2500 + plateNumber * 260); //2500 z 2600 zmienione
  while (stepperPL.distanceToGo()) stepperPL.run();
  stepperYL.moveTo(6000);
  while (stepperYL.distanceToGo()) stepperYL.run();
  stepperXL.moveTo(700);
  while (stepperXL.distanceToGo()) stepperXL.run();
stepperXL.setMaxSpeed(100000);
  stepperXL.setAcceleration(20000);
   stepperXL.moveTo(430);
  while (stepperXL.distanceToGo()) stepperXL.run();
  stepperXL.setMaxSpeed(motorSpeed);
  stepperXL.setAcceleration(motorAccel);
  stepperPL.moveTo(0);
  while (stepperPL.distanceToGo()) stepperPL.run();
  plateNumber++;
  server.send(200, "text/plain", "Plate Loaded");
}

void handleRemovePlate() {
  showOnOLED("Remove Plate");
  stepperPL.moveTo(0); stepperYL.moveTo(8000);
  while (stepperPL.distanceToGo() || stepperYL.distanceToGo()) { stepperPL.run(); stepperYL.run(); }
  stepperXL.moveTo(20);
  while (stepperXL.distanceToGo()) stepperXL.run();
  stepperXL.moveTo(700);
  while (stepperXL.distanceToGo()) stepperXL.run();
  server.send(200, "text/plain", "Plate Removed");
}

void handleStatus() {
  String pos = "PL: " + String(stepperPL.currentPosition()) +
               " | YL: " + String(stepperYL.currentPosition()) +
               " | XL: " + String(stepperXL.currentPosition());
  server.send(200, "text/plain", pos);
}

void handlePlateNumber() {
  server.send(200, "text/plain", String(plateNumber));
}

void handleEndstops() {
  String s = "PL: " + String(digitalRead(ENDSTOP_PL) ? "OPEN" : "CLOSED");
  s += " | YL: " + String(digitalRead(ENDSTOP_YL) ? "OPEN" : "CLOSED");
  s += " | XL: " + String(digitalRead(ENDSTOP_XL) ? "OPEN" : "CLOSED");
  server.send(200, "text/plain", s);
}

void handleDistance() {
  server.send(200, "text/plain", String(currentDistance));
}

// === NEW FUNCTION ===
#include <AccelStepper.h>
#include <math.h>

#define PI 3.14159265

void handleOpenChamber() {
  showOnOLED("Opening Chamber");

  // === 1. Move to initial safe position ===
  stepperYL.moveTo(3600);     // Y = more right
  stepperXL.moveTo(14300);    // X = forward
  while (stepperYL.distanceToGo() || stepperXL.distanceToGo()) {
    stepperYL.run();
    stepperXL.run();
  }

  // === 2. Move Y (left) to 4000 ===
  stepperYL.moveTo(4000);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // === 3. ARC MOVE to back-left (2000, 10700) ===
  const float centerX = 14300;
  const float centerY = 10700;
  const float radiusX = 12300;  // from 14300 to 2000
  const float radiusY = 6700;   // from 10700 to 4000

  const float startAngle = -PI / 2.0;  // -90 deg
  const float endAngle = -PI;          // -180 deg
  const int segments = 60;             // smoothness
  const float angleStep = (endAngle - startAngle) / segments;

  for (int i = 1; i <= segments; i++) {
    float theta = startAngle + i * angleStep;
    long targetX = centerX + radiusX * cos(theta); // XL = front/back
    long targetY = centerY + radiusY * sin(theta); // YL = left/right

    stepperXL.moveTo(targetX);
    stepperYL.moveTo(targetY);
    while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
      stepperXL.run();
      stepperYL.run();
    }
  }

  server.send(200, "text/plain", "Chamber Opened");
}


void handleCloseChamber() {
  showOnOLED("Closing Chamber");
 // 1. Move X to 300
  stepperXL.moveTo(300);
  while (stepperXL.distanceToGo()) stepperXL.run();

  // 2. Move Y to 10900
  stepperYL.moveTo(10900);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // 3. Move X to 1800
  stepperXL.moveTo(1800);
  while (stepperXL.distanceToGo()) stepperXL.run();

  const float centerX = 15000;
  const float centerY = 10500;
  const float radiusX = 13000.0;   // 15000 - 2000 (previous open start)
  const float radiusY = 6550.0;    // 10500 - 3950

  // Angle from left to actual destination
  const float startAngle = -PI;                       // -180°
  const float endAngle = atan2(3950 - 10500, 14300 - 15000);  // to (14300, 3950)

  const int segments = 60;
  const float angleStep = (endAngle - startAngle) / segments;

  for (int i = 1; i <= segments; i++) {
    float theta = startAngle + i * angleStep;

    long targetX = round(centerX + radiusX * cos(theta));
    long targetY = round(centerY + radiusY * sin(theta));

    // Guarantee final point
    if (i == segments) {
      targetX = 14300;
      targetY = 3950;
    }

    stepperXL.moveTo(targetX);
    stepperYL.moveTo(targetY);
    while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
      stepperXL.run();
      stepperYL.run();
    }
  }

  // Hard guarantee end point
  stepperXL.moveTo(14300);
  stepperYL.moveTo(3950);
  while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
    stepperXL.run();
    stepperYL.run();
  }
 stepperXL.moveTo(300);
  while (stepperXL.distanceToGo()) stepperXL.run();
  server.send(200, "text/plain", "Chamber Closed");
}


void handleLoadToPrinter() {
  showOnOLED("Load to Printer");
   stepperYL.moveTo(10400);
  while (stepperYL.distanceToGo()) stepperYL.run();
  stepperXL.moveTo(16450);
  while (stepperXL.distanceToGo()) stepperXL.run();
 stepperXL.moveTo(16100);
stepperYL.moveTo(10430);
  while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
    stepperXL.run();
    stepperYL.run();
  }
  delay(20000);
    stepperXL.moveTo(400);
  while (stepperXL.distanceToGo()) stepperXL.run();
}

void handleUnloadFromPrinter() {
  showOnOLED("Unload from Printer");
  stepperYL.moveTo(10200);
  while (stepperYL.distanceToGo()) stepperYL.run();

  stepperXL.moveTo(16150);
  while (stepperXL.distanceToGo()) stepperXL.run();
   stepperYL.moveTo(10400);
  while (stepperYL.distanceToGo()) stepperYL.run();

  delay(20000);
  stepperXL.moveTo(500);
  while (stepperXL.distanceToGo()) stepperXL.run();
}
void handleRoot() {
  String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Control</title></head><body>";
  page += "<h2>Stepper Control</h2>";

  page += "<button onclick=\"fetch('/home')\">Home All</button><br>";
  page += "<button onclick=\"fetch('/loadplate')\">Load Plate</button>";
  page += "<button onclick=\"fetch('/removeplate')\">Remove Plate</button><br>";

  page += "<button onclick=\"fetch('/openchamber')\">Open Chamber</button>";
  page += "<button onclick=\"fetch('/closechamber')\">Close Chamber</button><br>";
  page += "<button onclick=\"fetch('/loadtoprinter')\">Load to Printer</button>";
  page += "<button onclick=\"fetch('/unloadfromprinter')\">Unload from Printer</button><br>";

  page += "PL: <input id='pl' type='number'><br>";
  page += "YL: <input id='yl' type='number'><br>";
  page += "XL: <input id='xl' type='number'><br>";
  page += "<button onclick='moveMotors()'>Move Motors</button><br>";

  page += "Speed: <input id='speed' value='" + String(motorSpeed) + "'><br>";
  page += "Accel: <input id='accel' value='" + String(motorAccel) + "'><br>";
  page += "<button onclick='updateParams()'>Update Params</button><br>";

  page += "<div id='plate'></div><div id='endstops'></div><div id='distance'></div>";
  page += "<script>";
  page += "setInterval(async()=>{document.getElementById('plate').innerHTML=await (await fetch('/plate')).text();";
  page += "document.getElementById('endstops').innerHTML=await (await fetch('/endstops')).text();";
  page += "document.getElementById('distance').innerHTML=await (await fetch('/distance')).text() + ' mm';},1000);";
  page += "function moveMotors(){fetch(`/move?pl=${pl.value}&yl=${yl.value}&xl=${xl.value}`);}";
  page += "function updateParams(){fetch(`/updateparams?speed=${speed.value}&accel=${accel.value}`);}";
  page += "</script></body></html>";
  server.send(200, "text/html", page);
}
