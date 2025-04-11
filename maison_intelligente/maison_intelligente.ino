#include <AccelStepper.h>
#include <Wire.h>
#include <LCD_I2C.h>
#include <HCSR04.h>

// --- Constantes ---
#define TRIGGER_PIN 9
#define ECHO_PIN 10
#define IN_1 30
#define IN_2 31
#define IN_3 32
#define IN_4 33
#define RED_PIN 5
#define BLUE_PIN 6
#define BUZZER_PIN 7

const int stepsPerRevolution = 2048;
const int angleMin = 10;
const int angleMax = 170;
const float angleTolerance = 1.0;
const int DISTANCE_ALARME = 15;
const int DISTANCE_MAX = 400;  // Distance max par défaut si rien détecté

// --- Objets globaux ---
LCD_I2C lcd(0x27, 16, 2);
HCSR04 hc(TRIGGER_PIN, ECHO_PIN);
AccelStepper myStepper(AccelStepper::HALF4WIRE, IN_1, IN_3, IN_2, IN_4);

// --- Variables globales ---
int distance = 0;
float angle = 0;
bool alarmActive = false;
unsigned long lastDetectionTime = 0;
unsigned long currentTime = 0;

enum AppState { STOP, RUNNING };
AppState appState = RUNNING;

#pragma region Fonctions Utilitaires
int angleToSteps(float angle) {
  return (angle * stepsPerRevolution) / 360.0;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float calculerAngleDepuisDistance(int d) {
  if (d <= 30) return angleMax;
  if (d >= 60) return angleMin;
  return mapFloat(d, 30, 60, angleMax, angleMin);
}
#pragma endregion

#pragma region Tâches

void measureAndControlTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 50;
  if (ct - lastTime < rate) return;
  lastTime = ct;

  int d = hc.dist();
  if (d == 0 || d > DISTANCE_MAX) {
    distance = DISTANCE_MAX;
  } else {
    distance = d;
  }

  angle = calculerAngleDepuisDistance(distance);
  myStepper.moveTo(angleToSteps(angle));

  if (distance <= DISTANCE_ALARME) {
    lastDetectionTime = ct;
    alarmActive = true;
  } else {
    if (ct - lastDetectionTime > 3000) {
      alarmActive = false;
    }
  }
}

void alarmTask(unsigned long ct) {
  static unsigned long lastToggleTime = 0;
  static bool ledState = false;
  const unsigned long blinkRate = 250;

  if (alarmActive) {
    if (ct - lastToggleTime >= blinkRate) {
      lastToggleTime = ct;
      ledState = !ledState;
      digitalWrite(RED_PIN, ledState ? HIGH : LOW);
      digitalWrite(BLUE_PIN, ledState ? LOW : HIGH);

      tone(BUZZER_PIN, ledState ? 880 : 440); // Sirène de police
    }
  } else {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

void displayTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 100;

  if (ct - lastTime < rate) return;
  lastTime = ct;

  lcd.setCursor(0, 0);
  lcd.print("Distance:     ");
  lcd.setCursor(10, 0);
  lcd.print(distance);
  lcd.print("cm ");

  lcd.setCursor(0, 1);
  if (alarmActive) {
    lcd.print("ALARME!         ");
  } else if (distance == DISTANCE_MAX) {
    lcd.print("Hors portée     ");
  } else {
    int secondsLeft = max(0, 3 - (int)((ct - lastDetectionTime) / 1000));
    lcd.print("Repos dans ");
    lcd.print(secondsLeft);
    lcd.print("s");
  }
}

void serialTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 100;

  if (ct - lastTime < rate) return;
  lastTime = ct;

  Serial.print("etd:2411160,dist:");
  Serial.print(distance);
  Serial.print(",deg:");
  Serial.println(angle, 1);
}
#pragma endregion

#pragma region États
void runningState(unsigned long ct) {
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    return;
  }

  measureAndControlTask(ct);
  alarmTask(ct);
  displayTask(ct);
  serialTask(ct);
}
#pragma endregion

void stateManager(unsigned long ct) {
  switch (appState) {
    case STOP:
      break;
    case RUNNING:
      runningState(ct);
      break;
  }
}

#pragma region setup-loop
void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();

  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Splash screen
  unsigned long startTime = millis() + 2000;
  while (millis() < startTime) {
    lcd.setCursor(0, 0);
    lcd.print("2411160");
    lcd.setCursor(0, 1);
    lcd.print("Labo 4A");
  }
  lcd.clear();

  myStepper.setMaxSpeed(500);
  myStepper.setAcceleration(200);
  myStepper.setSpeed(200);
  myStepper.setCurrentPosition(0);
}

void loop() {
  currentTime = millis();
  myStepper.run(); // Pour AccelStepper
  stateManager(currentTime);
}
#pragma endregion
