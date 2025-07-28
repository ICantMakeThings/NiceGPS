#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int RXPin = PIN_008, TXPin = PIN_006;
static const uint32_t GPSBaud = 38400;

#define BUTTON_UP_PIN PIN_029
#define BUTTON_DOWN_PIN PIN_031
unsigned long lastActivityTime = 0;
unsigned long lastPinInputTime = 0;

const unsigned long INACTIVITY_TIMEOUT = 60000;
const unsigned long PIN_SETUP_TIMEOUT = 10000;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int inputIndex = 0;

TinyGPSPlus gps;

SoftwareSerial ss(RXPin, TXPin);

int currentPage = 0;
const unsigned long debounceDelay = 200;
unsigned long lastDebounce = 0;

double maxSpeed = 0.0;
double totalDistance = 0.0;
double lastLat = 0.0, lastLon = 0.0;
bool firstFix = true;


/*  https://github.com/ICantMakeThings/Nicenano-NRF52-Supermini-PlatformIO-Support/blob/main/Platformio%20Example%20code/Read%20Batt%20voltage/main.cpp  */
float readBatteryVoltage()
{
  // The volatile keyword is a type qualifier in C/C++ that tells the compiler a variable's value might change in ways that the compiler cannot detect from the code alone.
  // Essentially, it says: "Don't optimize access to this variable because its value might change unexpectedly."
  volatile uint32_t raw_value = 0;
  // Configure SAADC
  NRF_SAADC->ENABLE = 1;
  NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;

  NRF_SAADC->CH[0].CONFIG =
      (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
      (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) |
      (SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos);

  NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDDHDIV5;
  NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;

  // Sample
  NRF_SAADC->RESULT.PTR = (uint32_t)&raw_value;
  NRF_SAADC->RESULT.MAXCNT = 1;
  NRF_SAADC->TASKS_START = 1;
  while (!NRF_SAADC->EVENTS_STARTED)
    ;
  NRF_SAADC->EVENTS_STARTED = 0;
  NRF_SAADC->TASKS_SAMPLE = 1;
  while (!NRF_SAADC->EVENTS_END)
    ;
  NRF_SAADC->EVENTS_END = 0;
  NRF_SAADC->TASKS_STOP = 1;
  while (!NRF_SAADC->EVENTS_STOPPED)
    ;
  NRF_SAADC->EVENTS_STOPPED = 0;
  NRF_SAADC->ENABLE = 0;

  // Force explicit double-precision calculations
  double raw_double = (double)raw_value;
  double step1 = raw_double * 2.4;
  double step2 = step1 / 4095.0;
  double vddh = 5.0 * step2;

  return (float)vddh;
}

void drawBatteryIcon(float voltage)
{
  const int iconX = 128 - 18;
  const int iconY = 0;
  const int iconWidth = 16;
  const int iconHeight = 6;
  const int terminalWidth = 2;

  display.drawRect(iconX, iconY, iconWidth, iconHeight, SSD1306_WHITE);
  display.fillRect(iconX + iconWidth, iconY + 2, terminalWidth, iconHeight - 4, SSD1306_WHITE);

  float minV = 3.0;
  float maxV = 4.2;
  bool isCharging = (voltage >= 4.3);

  if (isCharging)
  {
    const int fillMax = iconWidth - 4;
    int animPhase = (millis() / 150) % (fillMax + 1);

    display.fillRect(iconX + 2, iconY + 2, animPhase, iconHeight - 4, SSD1306_WHITE);
  }
  else
  {
    int fillWidth = (int)((voltage - minV) / (maxV - minV) * (iconWidth - 4));
    if (fillWidth < 0)
      fillWidth = 0;
    if (fillWidth > iconWidth - 4)
      fillWidth = iconWidth - 4;

    if (fillWidth > 0)
    {
      display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4, SSD1306_WHITE);
    }
  }
}

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available()) gps.encode(ss.read());
  } while (millis() - start < ms);
}

void checkButtons() {
  static bool lastUp = HIGH, lastDown = HIGH;
  bool up = digitalRead(BUTTON_UP_PIN);
  bool down = digitalRead(BUTTON_DOWN_PIN);

  if ((up == LOW && lastUp == HIGH) || (down == LOW && lastDown == HIGH)) {
    lastActivityTime = millis();
    if (millis() - lastDebounce > debounceDelay) {
      if (up == LOW) currentPage = (currentPage + 1) % 4;
      if (down == LOW) currentPage = (currentPage + 3) % 4;
      lastDebounce = millis();
    }
  }

  lastUp = up;
  lastDown = down;
}

void updateStats() {
  if (gps.location.isValid()) {
    double currentLat = gps.location.lat();
    double currentLon = gps.location.lng();

    if (!firstFix) {
      double dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, currentLat, currentLon);
      totalDistance += dist / 1000.0;
    } else {
      firstFix = false;
    }

    lastLat = currentLat;
    lastLon = currentLon;
  }

  if (gps.speed.isValid()) {
    double spd = gps.speed.kmph();
    if (spd > maxSpeed) maxSpeed = spd;
  }
}

void drawMainPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Main GPS Info");

  display.setCursor(0, 12);
  display.print("Fix: ");
  display.println(gps.location.isValid() ? "Yes" : "No");

  display.print("Sats: ");
  display.println(gps.satellites.isValid() ? gps.satellites.value() : 0);

  display.print("Lat: ");
  display.println(gps.location.lat(), 6);

  display.print("Lng: ");
  display.println(gps.location.lng(), 6);

  float batteryVoltage = readBatteryVoltage();
  drawBatteryIcon(batteryVoltage);


  display.display();
}

void drawAdvancedPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Advanced GPS Data");

  display.setCursor(0, 12);
  display.print("Alt: ");
  display.println(gps.altitude.meters(), 1);

  display.print("HDOP: ");
  display.println(gps.hdop.hdop(), 1);

  display.print("Heading: ");
  display.println(gps.course.deg(), 1);

  display.print("Fix Age: ");
  display.println(gps.location.age());

  float batteryVoltage = readBatteryVoltage();
  drawBatteryIcon(batteryVoltage);

  display.display();
}

void drawSpeedPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Speed & Distance");

  display.setCursor(0, 12);
  display.print("Speed: ");
  display.println(gps.speed.kmph(), 2);

  display.print("Max: ");
  display.println(maxSpeed, 2);

  display.print("Dist (km): ");
  display.println(totalDistance, 2);

  float batteryVoltage = readBatteryVoltage();
  drawBatteryIcon(batteryVoltage);

  display.display();
}

void drawTimePage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("UTC Time / Date");

  display.setCursor(0, 16);
  if (gps.time.isValid()) {
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
    display.println(buf);
  } else {
    display.println("Time: No Fix");
  }

  display.setCursor(0, 32);
  if (gps.date.isValid()) {
    char dateBuf[16];
    sprintf(dateBuf, "%02d/%02d/%04d", gps.date.day(), gps.date.month(), gps.date.year());
    display.println(dateBuf);
  } else {
    display.println("Date: No Fix");
  }

  float batteryVoltage = readBatteryVoltage();
  drawBatteryIcon(batteryVoltage);

  display.display();
}

void enterUltraSleep() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Sleep Mode");
  display.display();
  delay(1000);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  digitalWrite(PIN_013, LOW);
  NRF_POWER->SYSTEMOFF = 1;
  while (true) {}
}

void configureWakeupButtons()
{
  nrf_gpio_cfg_sense_input(BUTTON_UP_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  nrf_gpio_cfg_sense_input(BUTTON_DOWN_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
}

void fadeOutDisplay()
{
  for (int contrast = 200; contrast >= 0; contrast -= 15)
  {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(contrast);
    delay(60);
  }
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void setup() {


  pinMode(PIN_013, OUTPUT);
  digitalWrite(PIN_013, HIGH);
  delay(1000);

  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("eepy-s GPS");
  display.setCursor(0, 30);
  display.println("Starting...");
  display.display();
  delay(100);

  configureWakeupButtons();

  ss.begin(GPSBaud);
  lastActivityTime = millis();
}

void loop() {
  smartDelay(100);
  checkButtons();
  updateStats();

  switch (currentPage) {
    case 0: drawMainPage(); break;
    case 1: drawAdvancedPage(); break;
    case 2: drawSpeedPage(); break;
    case 3: drawTimePage(); break;
  }

  if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
    enterUltraSleep();
  }
}
