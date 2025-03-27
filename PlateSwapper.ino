#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>

const char* ssid = "Kontyner";
const char* password = "Kancelaria2012";

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

int plateNumber = 0; // Tracks the current plate number
float motorSpeed = 1000;
float motorAccel = 500;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  pinMode(EN_PIN_PL, OUTPUT);
  pinMode(EN_PIN_YL, OUTPUT);
  pinMode(EN_PIN_XL, OUTPUT);
  digitalWrite(EN_PIN_PL, LOW);
  digitalWrite(EN_PIN_YL, LOW);
  digitalWrite(EN_PIN_XL, LOW);

  pinMode(ENDSTOP_PL, INPUT_PULLUP);
  pinMode(ENDSTOP_YL, INPUT_PULLUP);
  pinMode(ENDSTOP_XL, INPUT_PULLUP);

  setStepperParams();

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/endstops", handleEndstops);
  server.on("/home", handleHome);
  server.on("/loadplate", handleLoadPlate);
  server.on("/removeplate", handleRemovePlate);
  server.on("/status", handleStatus);
  server.on("/updateparams", handleUpdateParams);
  server.on("/plate", handlePlateNumber);
  server.on("/openchamber", handleOpenChamber);
  server.on("/unloadplatform", handleUnloadPlatform);
  server.on("/loadplatform", handleLoadPlatformToPrinter);

  server.begin();
}

void loop() {
  server.handleClient();
  stepperPL.run();
  stepperYL.run();
  stepperXL.run();
}

void setStepperParams() {
  stepperPL.setMaxSpeed(motorSpeed);
  stepperPL.setAcceleration(motorAccel);
  stepperYL.setMaxSpeed(motorSpeed);
  stepperYL.setAcceleration(motorAccel);
  stepperXL.setMaxSpeed(motorSpeed);
  stepperXL.setAcceleration(motorAccel);
}

void handleUpdateParams() {
  if (server.hasArg("speed")) motorSpeed = server.arg("speed").toFloat();
  if (server.hasArg("accel")) motorAccel = server.arg("accel").toFloat();
  setStepperParams();
  server.send(200, "text/plain", "Speed and Acceleration Updated");
}

void handleRoot() {
  String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Stepper Control</title></head><body>";
  page += "<h2>Stepper Motor Control Panel</h2>";

  page += "<h3>Automated Actions</h3>";
  page += "<button onclick=\"fetch('/home')\">🏠 Home All Motors</button><br><br>";
  page += "<button onclick=\"fetch('/loadplate')\">📦 Load Plate</button><br><br>";
  page += "<button onclick=\"fetch('/removeplate')\">🗑️ Remove Plate</button><br><br>";
  page += "<button onclick=\"fetch('/openchamber')\">🔓 Open Chamber</button><br><br>";
  page += "<button onclick=\"fetch('/unloadplatform')\">📤 Unload Platform from Printer</button><br><br>";
  page += "<button onclick=\"fetch('/loadplatform')\">📥 Load Platform to Printer</button><br><br>";

  page += "<h3>Manual Positioning</h3>";
  page += "PL: <input type='number' id='pl'><br>";
  page += "YL: <input type='number' id='yl'><br>";
  page += "XL: <input type='number' id='xl'><br>";
  page += "<button onclick='moveMotors()'>Move Motors</button><br><br>";

  page += "<h3>Speed and Acceleration</h3>";
  page += "Speed: <input type='number' id='speed' value='" + String(motorSpeed) + "'><br>";
  page += "Acceleration: <input type='number' id='accel' value='" + String(motorAccel) + "'><br>";
  page += "<button onclick='updateParams()'>Update Params</button><br><br>";

  page += "<h3>Plate Number:</h3><div id='plateNumber'>Loading...</div>";
  page += "<h3>Motor Positions:</h3><div id='positions'>Loading...</div>";
  page += "<h3>Endstop Status:</h3><div id='endstops'>Loading...</div>";

  page += "<script>";
  page += "setInterval(async()=>{";
  page += "document.getElementById('positions').innerHTML=await (await fetch('/status')).text();";
  page += "document.getElementById('plateNumber').innerHTML=await (await fetch('/plate')).text();";
  page += "document.getElementById('endstops').innerHTML=await (await fetch('/endstops')).text();";
  page += "}, 500);";
  page += "function moveMotors(){";
  page += "let pl=document.getElementById('pl').value;";
  page += "let yl=document.getElementById('yl').value;";
  page += "let xl=document.getElementById('xl').value;";
  page += "fetch(`/move?pl=${pl}&yl=${yl}&xl=${xl}`);";
  page += "}";
  page += "function updateParams(){";
  page += "let speed=document.getElementById('speed').value;";
  page += "let accel=document.getElementById('accel').value;";
  page += "fetch(`/updateparams?speed=${speed}&accel=${accel}`);";
  page += "}";
  page += "</script>";

  page += "</body></html>";
  server.send(200, "text/html", page);
}

void handleStatus() {
  String pos = "PL: " + String(stepperPL.currentPosition()) + " | YL: " + String(stepperYL.currentPosition()) + " | XL: " + String(stepperXL.currentPosition());
  server.send(200, "text/plain", pos);
}

void handleMove() {
  if (server.hasArg("pl")) stepperPL.moveTo(server.arg("pl").toInt());
  if (server.hasArg("yl")) stepperYL.moveTo(server.arg("yl").toInt());
  if (server.hasArg("xl")) stepperXL.moveTo(server.arg("xl").toInt());
  server.send(200, "text/plain", "Motors Moving");
}

void handlePlateNumber() {
  server.send(200, "text/plain", String(plateNumber));
}

void handleEndstops() {
  String states = "PL: " + String(digitalRead(ENDSTOP_PL) ? "OPEN" : "CLOSED");
  states += " | YL: " + String(digitalRead(ENDSTOP_YL) ? "OPEN" : "CLOSED");
  states += " | XL: " + String(digitalRead(ENDSTOP_XL) ? "OPEN" : "CLOSED");
  server.send(200, "text/plain", states);
}

void handleHome() {
  homeMotor(stepperPL, ENDSTOP_PL);
  homeMotor(stepperYL, ENDSTOP_YL);
  homeMotor(stepperXL, ENDSTOP_XL);
  plateNumber = 0;
  server.send(200, "text/plain", "Homing Complete");
}

void homeMotor(AccelStepper &stepper, int endstopPin) {
  stepper.moveTo(stepper.currentPosition() - 100000);
  while (digitalRead(endstopPin)) {
    stepper.run();
  }
  stepper.setCurrentPosition(0);
}

void handleLoadPlate() {
  stepperYL.moveTo(320);
  stepperXL.moveTo(150);
  while (stepperYL.distanceToGo() || stepperXL.distanceToGo()) {
    stepperYL.run();
    stepperXL.run();
  }

  stepperPL.moveTo(2600 + (plateNumber * 200));
  while (stepperPL.distanceToGo()) {
    stepperPL.run();
  }

  stepperXL.moveTo(6000);
  while (stepperXL.distanceToGo()) {
    stepperXL.run();
  }

  stepperYL.moveTo(700);
  while (stepperYL.distanceToGo()) {
    stepperYL.run();
  }

  stepperPL.moveTo(0);
  while (stepperPL.distanceToGo()) {
    stepperPL.run();
  }

  plateNumber++;
  server.send(200, "text/plain", "Plate Loaded");
}

void handleRemovePlate() {
  stepperPL.moveTo(0);
  stepperXL.moveTo(8000);
  while (stepperPL.distanceToGo() || stepperXL.distanceToGo()) {
    stepperPL.run();
    stepperXL.run();
  }
  stepperYL.moveTo(10);
  while (stepperYL.distanceToGo()) {
    stepperYL.run();
  }
  stepperYL.moveTo(800);
  while (stepperYL.distanceToGo()) {
    stepperYL.run();
  }
  server.send(200, "text/plain", "Plate Removed");
}

void handleOpenChamber() {
  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH);
  delay(1000);
  digitalWrite(17, LOW);
  server.send(200, "text/plain", "Chamber Opened");
}

void handleUnloadPlatform() {
  stepperYL.moveTo(2000);
  while (stepperYL.distanceToGo()) stepperYL.run();

  stepperPL.moveTo(1500);
  while (stepperPL.distanceToGo()) stepperPL.run();

  stepperXL.moveTo(3000);
  while (stepperXL.distanceToGo()) stepperXL.run();

  stepperPL.moveTo(0);
  while (stepperPL.distanceToGo()) stepperPL.run();

  server.send(200, "text/plain", "Platform Unloaded");
}

void handleLoadPlatformToPrinter() {
  stepperPL.moveTo(1500);
  while (stepperPL.distanceToGo()) stepperPL.run();

  stepperXL.moveTo(0);
  while (stepperXL.distanceToGo()) stepperXL.run();

  stepperYL.moveTo(2000);
  while (stepperYL.distanceToGo()) stepperYL.run();

  stepperPL.moveTo(0);
  while (stepperPL.distanceToGo()) stepperPL.run();

  server.send(200, "text/plain", "Platform Loaded To Printer");
}
