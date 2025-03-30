#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <VL53L1X.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssid = "ssid";
const char* password = "pass";

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

#define EN_PIN_YL   14
#define STEP_PIN_YL 27
#define DIR_PIN_YL  26

#define EN_PIN_XL   15
#define STEP_PIN_XL 13
#define DIR_PIN_XL  12

// Endstop pins
#define ENDSTOP_PL 18
#define ENDSTOP_YL 19
#define ENDSTOP_XL 5

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
  homeMotor(stepperYL, ENDSTOP_YL);
  homeMotor(stepperXL, ENDSTOP_XL);
  plateNumber = 0;
  server.send(200, "text/plain", "Homing Done");
}

void handleLoadPlate() {
  showOnOLED("Load Plate");
  stepperYL.moveTo(310); stepperXL.moveTo(150);
  while (stepperYL.distanceToGo() || stepperXL.distanceToGo()) { stepperYL.run(); stepperXL.run(); }
  stepperPL.moveTo(2600 + plateNumber * 260);
  while (stepperPL.distanceToGo()) stepperPL.run();
  stepperXL.moveTo(6000);
  while (stepperXL.distanceToGo()) stepperXL.run();
  stepperYL.moveTo(700);
  while (stepperYL.distanceToGo()) stepperYL.run();
stepperYL.setMaxSpeed(100000);
  stepperYL.setAcceleration(20000);
   stepperYL.moveTo(500);
  while (stepperYL.distanceToGo()) stepperYL.run();
  stepperYL.setMaxSpeed(motorSpeed);
  stepperYL.setAcceleration(motorAccel);
  stepperPL.moveTo(0);
  while (stepperPL.distanceToGo()) stepperPL.run();
  plateNumber++;
  server.send(200, "text/plain", "Plate Loaded");
}

void handleRemovePlate() {
  showOnOLED("Remove Plate");
  stepperPL.moveTo(0); stepperXL.moveTo(8000);
  while (stepperPL.distanceToGo() || stepperXL.distanceToGo()) { stepperPL.run(); stepperXL.run(); }
  stepperYL.moveTo(10);
  while (stepperYL.distanceToGo()) stepperYL.run();
  stepperYL.moveTo(700);
  while (stepperYL.distanceToGo()) stepperYL.run();
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
void handleOpenChamber() {
  showOnOLED("Opening Chamber");

  // 1. Move X to 3500
  stepperXL.moveTo(3500);
  while (stepperXL.distanceToGo()) stepperXL.run();

  // 2. Move Y to 8000
  stepperYL.moveTo(8000);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // 3. Move X to 3600
  stepperXL.moveTo(3600);
  while (stepperXL.distanceToGo()) stepperXL.run();

  // 4. Move Y to 7500
  stepperYL.moveTo(7500);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // 5. Ćwiartka koła: do X = 10300, Y = 1100
  int centerX = 10300;
  int centerY = 7500;
  int radius = abs(centerY - 1100); // czyli 6400
stepperYL.setMaxSpeed(6000);
  stepperYL.setAcceleration(5000);
  stepperXL.setMaxSpeed(6000);
  stepperXL.setAcceleration(5000);
  for (int angle = 0; angle <= 90; angle++) {
    float rad = angle * PI / 180.0;
    int x = centerX - int(radius * cos(rad)); // zakrzywienie w lewo
    int y = centerY - int(radius * sin(rad)); // zakrzywienie w górę
    stepperXL.moveTo(x);
    stepperYL.moveTo(y);
    while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
      stepperXL.run();
      stepperYL.run();
    }
  }
stepperYL.setMaxSpeed(motorSpeed);
  stepperYL.setAcceleration(motorAccel);
  stepperXL.setMaxSpeed(motorSpeed);
  stepperXL.setAcceleration(motorAccel);
  server.send(200, "text/plain", "Chamber Opened");
}



void handleCloseChamber() {
  showOnOLED("Closing Chamber");

  // 1. Move Y to 500
  stepperYL.moveTo(500);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // 2. Move X to 10400
  stepperXL.moveTo(10400);
  while (stepperXL.distanceToGo()) stepperXL.run();

  // 3. Move Y to 1100
  stepperYL.moveTo(1100);
  while (stepperYL.distanceToGo()) stepperYL.run();

  // 4. Ćwiartka koła: do X=3700, Y=7800
  int centerX = 10400;
  int centerY = 7800;
  int radius = abs(centerY - 1100); // 6700

  stepperYL.setMaxSpeed(6000);
  stepperYL.setAcceleration(5000);
  stepperXL.setMaxSpeed(6000);
  stepperXL.setAcceleration(5000);

  for (int angle = 0; angle <= 90; angle++) {
    float rad = angle * PI / 180.0;
    int x = centerX - int(radius * sin(rad)); // z prawej do lewej
    int y = centerY - int(radius * cos(rad)); // z góry w dół
    stepperXL.moveTo(x);
    stepperYL.moveTo(y);
    while (stepperXL.distanceToGo() || stepperYL.distanceToGo()) {
      stepperXL.run();
      stepperYL.run();
    }
  }

  // 5. Y to 600
  stepperYL.setMaxSpeed(motorSpeed);
  stepperYL.setAcceleration(motorAccel);
  stepperXL.setMaxSpeed(motorSpeed);
  stepperXL.setAcceleration(motorAccel);
 stepperYL.moveTo(7980);
  while (stepperYL.distanceToGo()) stepperYL.run();
  stepperYL.moveTo(600);
  while (stepperYL.distanceToGo()) stepperYL.run();

  server.send(200, "text/plain", "Chamber Closed");
}



void handleLoadToPrinter() {
  showOnOLED("Load to Printer");
  server.send(200, "text/plain", "Not implemented");
}

void handleUnloadFromPrinter() {
  showOnOLED("Unload from Printer");
  server.send(200, "text/plain", "Not implemented");
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
