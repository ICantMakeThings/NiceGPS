#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int RXPin = PIN_008, TXPin = PIN_006;
static const uint32_t GPSBaud = 38400;

#define BUTTON_UP_PIN PIN_031
#define BUTTON_DOWN_PIN PIN_029
unsigned long lastActivityTime = 0;
unsigned long buttonHoldStart = 0;
bool isHolding = false;

const unsigned long INACTIVITY_TIMEOUT = 60000;
const unsigned long SLEEP_HOLD_DURATION = 5000;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
int currentPage = 0;
const unsigned long debounceDelay = 200;
unsigned long lastDebounce = 0;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

double maxSpeed = 0.0;
double totalDistance = 0.0;
double lastLat = 0.0, lastLon = 0.0;
bool firstFix = true;

float readBatteryVoltage()
{
  volatile uint32_t raw_value = 0;
  NRF_SAADC->ENABLE = 1;
  NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;
  NRF_SAADC->CH[0].CONFIG =
      (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
      (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) |
      (SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos);
  NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDDHDIV5;
  NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;
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
  double step2 = ((double)raw_value * 2.4) / 4095.0;
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
    int animPhase = (millis() / 150) % (iconWidth - 4 + 1);
    display.fillRect(iconX + 2, iconY + 2, animPhase, iconHeight - 4, SSD1306_WHITE);
  }
  else
  {
    int fillWidth = (int)((voltage - minV) / (maxV - minV) * (iconWidth - 4));
    fillWidth = constrain(fillWidth, 0, iconWidth - 4);
    if (fillWidth > 0)
      display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4, SSD1306_WHITE);
  }
}

void enterUltraSleep()
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("ZzZz");
  display.display();
  delay(1000);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  digitalWrite(PIN_013, LOW);
  NRF_POWER->SYSTEMOFF = 1;
  while (true)
  {
  }
}

void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

void checkButtons()
{
  static bool lastUp = HIGH, lastDown = HIGH;
  bool up = digitalRead(BUTTON_UP_PIN);
  bool down = digitalRead(BUTTON_DOWN_PIN);

  if ((up == LOW || down == LOW) && !isHolding)
  {
    buttonHoldStart = millis();
    isHolding = true;
  }
  if (isHolding && (up == HIGH && down == HIGH))
  {
    isHolding = false;
  }
  if (isHolding && millis() - buttonHoldStart >= SLEEP_HOLD_DURATION)
  {
    enterUltraSleep();
  }

  if ((up == LOW && lastUp == HIGH) || (down == LOW && lastDown == HIGH))
  {
    lastActivityTime = millis();
    if (millis() - lastDebounce > debounceDelay)
    {
      if (up == LOW)
        currentPage = (currentPage + 1) % 5;
      if (down == LOW)
        currentPage = (currentPage + 4) % 5;
      lastDebounce = millis();
    }
  }
  lastUp = up;
  lastDown = down;
}

void updateStats()
{
  if (gps.location.isValid())
  {
    double currentLat = gps.location.lat();
    double currentLon = gps.location.lng();
    if (!firstFix)
    {
      double dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, currentLat, currentLon);
      totalDistance += dist / 1000.0;
    }
    else
    {
      firstFix = false;
    }
    lastLat = currentLat;
    lastLon = currentLon;
  }
  if (gps.speed.isValid())
  {
    double spd = gps.speed.kmph();
    if (spd > maxSpeed)
      maxSpeed = spd;
  }
}

void drawSpeedLargePage()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.drawLine(0, 10, 128, 10, WHITE);
  display.println("Speed | km/h");

  display.setTextSize(3);
  display.setCursor(10, 25);
  display.print(gps.speed.kmph(), 1);

  drawBatteryIcon(readBatteryVoltage());
  display.display();
}

void drawMainPage()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Main GPS Info");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 14);
  display.print("Fix: ");
  display.println(gps.location.isValid() ? "Yes" : "No");

  display.print("Sats: ");
  display.println(gps.satellites.isValid() ? gps.satellites.value() : 0);

  display.print("Lat: ");
  display.println(gps.location.lat(), 6);

  display.print("Lng: ");
  display.println(gps.location.lng(), 6);

  drawBatteryIcon(readBatteryVoltage());
  display.display();
}

void drawAdvancedPage()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Advanced GPS Data");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 14);
  display.print("Alt: ");
  display.println(gps.altitude.meters(), 1);

  display.print("HDOP: ");
  display.println(gps.hdop.hdop(), 1);

  display.print("Heading: ");
  display.println(gps.course.deg(), 1);

  display.print("Fix Age: ");
  display.println(gps.location.age());

  drawBatteryIcon(readBatteryVoltage());
  display.display();
}

void drawSpeedPage()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Speed & Distance");
  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 14);
  display.print("Speed: ");
  display.println(gps.speed.kmph(), 2);

  display.print("Max: ");
  display.println(maxSpeed, 2);

  display.print("Dist (km): ");
  display.println(totalDistance, 2);

  drawBatteryIcon(readBatteryVoltage());
  display.display();
}

void drawTimePage()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("UTC Time / Date");
  display.drawLine(0, 10, 128, 10, WHITE);

  if (gps.time.isValid())
  {
    display.setTextSize(2);
    display.setCursor(10, 14);
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
    display.println(buf);
  }
  else
  {
    display.setTextSize(2);
    display.setCursor(10, 14);
    display.println("No Time");
  }

  display.setTextSize(1);
  display.setCursor(0, 44);
  if (gps.date.isValid())
  {
    char dateBuf[16];
    sprintf(dateBuf, "%02d/%02d/%04d", gps.date.day(), gps.date.month(), gps.date.year());
    display.println(dateBuf);
  }
  else
  {
    display.println("Date: No Fix");
  }

  drawBatteryIcon(readBatteryVoltage());
  display.display();
}

void configureWakeupButtons()
{
  nrf_gpio_cfg_sense_input(BUTTON_UP_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  nrf_gpio_cfg_sense_input(BUTTON_DOWN_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
}

void setup()
{
  Serial.begin(115200);
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
  display.println("NiceGPS\nStarting...");
  display.display();

  configureWakeupButtons();
  ss.begin(GPSBaud);

  lastActivityTime = millis();
}

void loop()
{
  smartDelay(100);
  checkButtons();
  updateStats();

  switch (currentPage)
  {
  case 0:
    drawMainPage();
    break;
  case 1:
    drawAdvancedPage();
    break;
  case 2:
    drawSpeedPage();
    break;
  case 3:
    drawTimePage();
    break;
  case 4:
    drawSpeedLargePage();
    break;
  }

  if (millis() - lastActivityTime > INACTIVITY_TIMEOUT)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  if (digitalRead(BUTTON_UP_PIN) == LOW || digitalRead(BUTTON_DOWN_PIN) == LOW)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
  }
}
