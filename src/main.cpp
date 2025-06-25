#include "N_util.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 1 // Digital pin connected to the DHT sensor
// Uncomment the type of sensor in use:
#define DHTTYPE DHT11 // DHT 11
// #define DHTTYPE    DHT22     // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT_Unified dht(DHTPIN, DHTTYPE);
static unsigned long DTH11_CHECK_INTERVAL_MS = 1 * 1000UL;

enum KeyNum
{
  KN_NONE,
  KN_UP,
  KN_DOWN,
  KN_LEFT,
  KN_RIGHT
};

enum SettingMode
{
  SM_NONE,
  SM_BRIGHT_LEVEL,
  SM_LOWBAT_THRESHOLD
};
static SettingMode settingMode = SM_NONE;

#define BRIGHT_LVL_INIT 30
#define BRIGHT_LVL_MAX 255
#define BRIGHT_LVL_MIN 0
#define LOWBAT_THRESHOLD_INIT 10
#define LOWBAT_THRESHOLD_MAX 95
#define LOWBAT_THRESHOLD_MIN 5
static uint8_t BRIGHT_LVL;       // 0 - 255 : LCD bright level
static uint8_t LOWBAT_THRESHOLD; // 5 - 95% : LOW BATTERY Threshold level
const char *NVM_BRIGHT_TITLE = "brt";
const char *NVM_LOWBAT_TITLE = "lbat";

void setup();
void loop();
bool keyCheck();
void settings();
bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step);
void changeSettings(SettingMode mode, KeyNum keyNo);
void dth11_sensor();
void dispInit();
void settingsInit();
void updateFloatValueDisplay(float currentValue, float &previousValue, uint8_t lineIndex, uint8_t column, uint8_t widthChars);
void prtTempValue(float temp_val);
void prtHumiValue(float humi_val);
void batteryState();
void prtBatLvl(uint8_t batLvl);
void prtSetting(const char *msg, uint8_t data);
void changeBright(KeyNum keyNo);
void changeLowBatThr(KeyNum keyNo);
void lowBatteryCheck(uint8_t batLvl);

void setup()
{
  m5stack_begin();

  if (SD_ENABLE)
  { // M5stack-SD-Updater lobby
    SDU_lobby();
    SD.end();
  }

  // Initialize sensor
  dht.begin();
  sensor_t sensor;
#ifdef DEBUG
  // Print temperature sensor details.
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("°C"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("°C"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("%"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("%"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("%"));
  Serial.println("min_delay: " + String(sensor.min_delay) + " usec");
  Serial.println(F("------------------------------------"));
#endif
  DTH11_CHECK_INTERVAL_MS = sensor.min_delay / 1000UL;

  settingsInit();
  dispInit();
}

void loop()
{
  dth11_sensor();
  batteryState();

  if (keyCheck())
    settings();

  vTaskDelay(1);
}

bool keyCheck()
{
  M5Cardputer.update(); // update Cardputer key input

  if (M5Cardputer.Keyboard.isChange())
  {
    if (M5Cardputer.Keyboard.isPressed())
      return true;
  }
  return false;
}

void settings()
{
  // Setting Mode Check
  bool setting_do = false;
  KeyNum keyNum = KN_NONE;

  if (M5Cardputer.Keyboard.isKeyPressed('0')) // clear
  {
    settingMode = SM_NONE;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('1')) // lcd brightness
  {
    settingMode = SM_BRIGHT_LEVEL;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('2')) // lowBattery threshold
  {
    settingMode = SM_LOWBAT_THRESHOLD;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(';')) // up
  {
    keyNum = KN_UP;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('.')) // down
  {
    keyNum = KN_DOWN;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(',')) // left
  {
    keyNum = KN_LEFT;
    setting_do = true;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed('/')) // right
  {
    keyNum = KN_RIGHT;
    setting_do = true;
  }

  if (setting_do)
  {
    changeSettings(settingMode, keyNum);
    return;
  }
}

void changeSettings(SettingMode mode, KeyNum keyNo)
{
  if (mode == SM_NONE)
  {
    // clear L1 : setting display line
    M5Cardputer.Display.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK);
  }

  else if (mode == SM_BRIGHT_LEVEL)
    changeBright(keyNo);

  else if (mode == SM_LOWBAT_THRESHOLD)
    changeLowBatThr(keyNo);
}

bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step)
{
  int tempValue = value;
  bool changeKey = false;

  switch (keyNo)
  {
  case KN_UP:
    tempValue += big_step;
    changeKey = true;
    break;
  case KN_DOWN:
    tempValue -= big_step;
    changeKey = true;
    break;
  case KN_RIGHT:
    tempValue += step;
    changeKey = true;
    break;
  case KN_LEFT:
    tempValue -= step;
    changeKey = true;
    break;
  default:
    break;
  }

  if (changeKey)
  {
    if (tempValue > max)
      tempValue = max;
    if (tempValue < min)
      tempValue = min;

    if (value != (uint8_t)tempValue)
    {
      value = (uint8_t)tempValue;
      return true; // Value changed
    }
  }
  return false; // No change
}

void prtSetting(const char *msg, uint8_t data)
{
  // Line1 : setting display
  const int DISP_POS = 2; // display start position
  char msgBuf[30];        // message buffer
  snprintf(msgBuf, sizeof(msgBuf), "%s%3u", msg, data);
  dbPrtln(msgBuf);

  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK); // clear L1
  M5Cardputer.Display.drawString(msgBuf, W_CHR * DISP_POS, SC_LINES[1], &fonts::Font2);
}

#define STEP_SHORT 1
#define STEP_BIG 10
void changeBright(KeyNum keyNo)
{
  if (updateSettingValue(BRIGHT_LVL, keyNo, BRIGHT_LVL_MIN, BRIGHT_LVL_MAX, STEP_SHORT, STEP_BIG))
  {
    M5Cardputer.Display.setBrightness(BRIGHT_LVL);
    wrtNVS(NVM_BRIGHT_TITLE, BRIGHT_LVL);
  }
  prtSetting("bright = ", BRIGHT_LVL);
}

void changeLowBatThr(KeyNum keyNo)
{
  if (updateSettingValue(LOWBAT_THRESHOLD, keyNo, LOWBAT_THRESHOLD_MIN, LOWBAT_THRESHOLD_MAX, STEP_SHORT, STEP_BIG))
  {
    wrtNVS(NVM_LOWBAT_TITLE, LOWBAT_THRESHOLD);
  }
  prtSetting("lowBattery threshold = ", LOWBAT_THRESHOLD);
}

static unsigned long PREV_DTH11_TM = 0L;
void dth11_sensor()
{
  unsigned long currentTime = millis(); // Get current time once

  if (currentTime - PREV_DTH11_TM < DTH11_CHECK_INTERVAL_MS)
  {
    return;
  }
  PREV_DTH11_TM = currentTime;

  // Get temperature event and print its value.
  sensors_event_t event;

  dht.temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    dbPrtln("Error reading temperature!");
    prtTempValue(NAN);
  }
  else
  {
    prtTempValue(event.temperature);
    dbPrtln("Temperature: " + String(event.temperature) + "°C");
  }

  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    dbPrtln("Error reading humidity!");
    prtHumiValue(NAN);
  }
  else
  {
    prtHumiValue(event.relative_humidity);
    dbPrtln("Humidity: " + String(event.relative_humidity) + "%");
  }
}

void dispInit()
{
  // ---012345678901234567890123456789----
  // L0:-- DTH11 Sensor --    bat.---%
  // L1:                   bright ---
  // L2:
  // L3:
  // L4:
  // L5:
  // L6:
  // L7:   気温　℃         湿度　％
  // ---012345678901234567890123456789----

  const int TEMP_COL = 3;
  const int HUMI_COL = 19;

  M5Cardputer.Display.fillScreen(TFT_BLACK); // 画面塗り潰し
  M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_16);
  M5Cardputer.Display.setTextSize(1);

  // temperater
  M5Cardputer.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * TEMP_COL, SC_LINES[7]);
  //--------------------------"012345678901234567890123456789"--;
  M5Cardputer.Display.print(F("気温　℃"));

  // humidity
  M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * HUMI_COL, SC_LINES[7]);
  //----------"012345678901234567890123456789"--;
  M5Cardputer.Display.print(F("湿度　％"));

  //--L0 : title--------------
  M5Cardputer.Display.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  M5Cardputer.Display.drawString(F("-- DTH11 Sensor --"), 0, SC_LINES[0], &fonts::Font2);

  // L0 :Battery Level -----
  const int BATVAL_POS = 22; // Battery value display start position
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.drawString(F("bat.---%"), W_CHR * BATVAL_POS, SC_LINES[0], &fonts::Font2);
}

void settingsInit()
{
  // LCD brightness setup
  uint8_t nvmData;
  if (!rdNVS(NVM_BRIGHT_TITLE, nvmData))
  {
    BRIGHT_LVL = BRIGHT_LVL_INIT;         // 0 - 255 : LCD bright level
    wrtNVS(NVM_BRIGHT_TITLE, BRIGHT_LVL); // Write only if using default
  }
  else
  {
    BRIGHT_LVL = nvmData;
  }
  M5Cardputer.Display.setBrightness(BRIGHT_LVL);

  // Low Battery Threshold setup
  if (!rdNVS(NVM_LOWBAT_TITLE, nvmData))
  {
    LOWBAT_THRESHOLD = LOWBAT_THRESHOLD_INIT; // % : low battery threshold lvl
    wrtNVS(NVM_LOWBAT_TITLE, LOWBAT_THRESHOLD);
  }
  else
  {
    if (nvmData < LOWBAT_THRESHOLD_MIN || nvmData > LOWBAT_THRESHOLD_MAX)
    {
      nvmData = LOWBAT_THRESHOLD_INIT;
      wrtNVS(NVM_LOWBAT_TITLE, nvmData);
    }
    LOWBAT_THRESHOLD = nvmData;
  }
}

void updateFloatValueDisplay(float currentValue, float &previousValue, uint8_t lineIndex, uint8_t column, uint8_t widthChars)
{
  // Do not update if the value is the same.
  // Note: (NAN == NAN) is always false, so we need isnan() for the check.
  if (previousValue == currentValue || (isnan(previousValue) && isnan(currentValue)))
  {
    return;
  }
  previousValue = currentValue;

  char buf[10];
  if (isnan(currentValue))
  {
    snprintf(buf, sizeof(buf), "--.-");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%2.1f", currentValue);
  }

  M5Cardputer.Display.setFont(&fonts::Font6);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillRect(W_CHR * column, SC_LINES[lineIndex], W_CHR * widthChars, H_CHR * 4, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * column, SC_LINES[lineIndex]);
  M5Cardputer.Display.print(buf);
}

#define TEMP_LINE_INDEX 3
#define TEMP_COL_INDEX 1
#define TEMP_DISP_WIDTH 13
static float PREV_TEMPERATURE = 0.0;
void prtTempValue(float temp_val)
{
  updateFloatValueDisplay(temp_val, PREV_TEMPERATURE, TEMP_LINE_INDEX, TEMP_COL_INDEX, TEMP_DISP_WIDTH);
}

#define HUMI_LINE_INDEX 3
#define HUMI_COL_INDEX 17
#define HUMI_DISP_WIDTH 13
static float PREV_HUMIDITY = 0.0;
void prtHumiValue(float humi_val)
{
  updateFloatValueDisplay(humi_val, PREV_HUMIDITY, HUMI_LINE_INDEX, HUMI_COL_INDEX, HUMI_DISP_WIDTH);
}

static unsigned long PREV_BATCHK_TM = 0L;
static uint8_t PREV_BATLVL = 255; // Use an impossible value to force the first update
#define BATLVL_MAX 100
void batteryState()
{
  const unsigned long BATCHK_INTVAL = 1993UL;
  unsigned long currentTime = millis(); // Get current time once

  if (currentTime - PREV_BATCHK_TM < BATCHK_INTVAL)
    return;

  // This will update consecutiveLowBatteryCount
  PREV_BATCHK_TM = currentTime;
  uint8_t batLvl = (uint8_t)M5.Power.getBatteryLevel(); // Get battery level
  dbPrtln("batLvl: " + String(batLvl));
  if (batLvl > BATLVL_MAX)
    batLvl = BATLVL_MAX;

  lowBatteryCheck(batLvl);

  if (batLvl == PREV_BATLVL) // The check now works correctly on the first run
    return;

  PREV_BATLVL = batLvl;
  prtBatLvl(batLvl);
}

void prtBatLvl(uint8_t batLvl)
{
  // Line0 : battery level display
  //---- 012345678901234567890123456789---
  // L0_"                      bat.xxx%"--
  const int BATVAL_POS = 22; // Battery value display start position
  char msg[10] = "bat.";     // message buffer
  snprintf(msg, sizeof(msg), "%s%3u%%", msg, batLvl);
  dbPrtln(msg);

  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillRect(W_CHR * BATVAL_POS, SC_LINES[0], W_CHR * sizeof(msg), H_CHR, TFT_BLACK); // clear
  M5Cardputer.Display.drawString(msg, W_CHR * BATVAL_POS, SC_LINES[0], &fonts::Font2);
}

static uint8_t consecutiveLowBatteryCount = 0;
void lowBatteryCheck(uint8_t batLvl)
{
  const uint8_t LOWBAT_CONSECUTIVE_READINGS = 5;
  const int LOWBAT_DISP_COL = 45;

  // Update consecutive low battery count
  if (batLvl < LOWBAT_THRESHOLD)
  {
    if (consecutiveLowBatteryCount < LOWBAT_CONSECUTIVE_READINGS)
    { // Avoid overflow if already at max
      consecutiveLowBatteryCount++;
    }
  }
  else
  {
    consecutiveLowBatteryCount = 0; // Reset if battery level is acceptable
    return;
  }

  if (consecutiveLowBatteryCount >= LOWBAT_CONSECUTIVE_READINGS)
  {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setFont(&fonts::Font4);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(LOWBAT_DISP_COL, SC_LINES[3]);
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.print(F("Low Battery !!"));
    POWER_OFF();
    // never return
  }
}
