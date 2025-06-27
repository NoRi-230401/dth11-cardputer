// --------------------------------------------------------
//  *** dth11-cardputer ***     by NoRi
//  DTH11 Senseor software for Cardputer
// (Temperature and Humidity)
//    2025-06-28  v105
// https://github.com/NoRi-230401/dth11-cardputer
//  MIT License
// --------------------------------------------------------
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
  SM_ESC,
  SM_BRIGHT_LEVEL,
  SM_LOWBAT_THRESHOLD,
  SM_LANG
};
static SettingMode settingMode = SM_ESC;

#define BRIGHT_LVL_INIT 30
#define BRIGHT_LVL_MAX 255
#define BRIGHT_LVL_MIN 0
#define BATLVL_MAX 100
#define LOWBAT_THRESHOLD_INIT 10
#define LOWBAT_THRESHOLD_MAX 95
#define LOWBAT_THRESHOLD_MIN 5
#define LANG_INIT 0 // 0:English 1:Japanese
#define LANG_MAX 1
#define BATLVL_ITEM_POS 22 // 'bat.' or '電池'
#define BATLVL_ITEM_LEN 4
#define BATLVL_VALUE_POS 26 // xxx
#define BATLVL_VALUE_LEN 3
#define BATLVL_PERCENT_POS 29 // %
#define SETTING_DISP_POS 2    // setting display start position

// --- Key mapping constants ---
const char KEY_SETTING_ESCAPE = '`';
const char KEY_SETTING_BRIGHTNESS = '1';
const char KEY_SETTING_LOWBAT = '2';
const char KEY_SETTING_LANG = '3';
const char KEY_UP = ';';
const char KEY_DOWN = '.';
const char KEY_LEFT = ',';
const char KEY_RIGHT = '/';

const char *BATLVL_TITLE[] = {"bat.", "電池"};
static uint8_t BRIGHT_LVL;       // 0 - 255 : LCD bright level
static uint8_t LOWBAT_THRESHOLD; // 5 - 95% : LOW BATTERY Threshold level
const char *NVM_BRIGHT_TITLE = "brt";

const char *NVM_LOWBAT_TITLE = "lbat";
const char *NVM_LANG_TITLE = "lang";
const char *LANG[] = {"English", "日本語"};
static uint8_t LANG_INDEX = 0;
const char *meas_items[][2] = {{"Temp. ℃", "気温　℃"},
                               {"Humidity %", "湿度　％"}};

void setup();
void loop();
bool keyCheck();
void settings();
void changeSettings(SettingMode mode, KeyNum keyNo);
void changeLang(KeyNum keyNo);
bool updateLang(KeyNum keyNo);
void dispBatItem();
void dispMeasItem();
bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step);
void prtSetting(const char *msg, uint8_t data);
void prtSetting(const char *msg, const char *data);
void changeBright(KeyNum keyNo);
void changeLowBatThr(KeyNum keyNo);
void dth11_sensor();
void dispInit();
static void loadSetting(const char *nvs_key, uint8_t &setting_variable, uint8_t default_value, uint8_t min_val, uint8_t max_val);
void settingsInit();
void updateFloatValueDisplay(float currentValue, float &previousValue, uint8_t lineIndex, uint8_t column, uint8_t widthChars);
void prtTempValue(float temp_val);
void prtHumiValue(float humi_val);
void batteryState();
void prtBatLvl(uint8_t batLvl);
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
  KeyNum keyNum = KN_NONE;

  // key 1 - 3 : special setting mode
  // key `     : clear and escape special mode
  if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_ESCAPE)) // clear and escape
  {
    if (settingMode == SM_ESC)
      return;
    settingMode = SM_ESC;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_BRIGHTNESS)) // lcd brightness
  {
    if (settingMode == SM_BRIGHT_LEVEL)
      return;
    settingMode = SM_BRIGHT_LEVEL;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_LOWBAT)) // lowBattery threshold
  {
    if (settingMode == SM_LOWBAT_THRESHOLD)
      return;
    settingMode = SM_LOWBAT_THRESHOLD;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_SETTING_LANG)) // select language
  {
    if (settingMode == SM_LANG)
      return;
    settingMode = SM_LANG;
  }
  // -----------------------------------------------------------------------
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_UP)) // up
  {
    keyNum = KN_UP;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_DOWN)) // down
  {
    keyNum = KN_DOWN;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT)) // left
  {
    keyNum = KN_LEFT;
  }
  else if (M5Cardputer.Keyboard.isKeyPressed(KEY_RIGHT)) // right
  {
    keyNum = KN_RIGHT;
  }
  else
  {
    return; // No relevant key pressed
  }

  changeSettings(settingMode, keyNum);
}

void changeSettings(SettingMode mode, KeyNum keyNo)
{
  if (mode == SM_ESC)
  {
    // clear L1 : setting display line
    M5Cardputer.Display.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK);
  }

  else if (mode == SM_BRIGHT_LEVEL)
    changeBright(keyNo);

  else if (mode == SM_LOWBAT_THRESHOLD)
    changeLowBatThr(keyNo);

  else if (mode == SM_LANG)
    changeLang(keyNo);
}

void changeLang(KeyNum keyNo)
{
  if (updateLang(keyNo))
  {
    wrtNVS(NVM_LANG_TITLE, LANG_INDEX);
    dispMeasItem();
    dispBatItem();
  }
  prtSetting("lang = ", LANG[LANG_INDEX]);
}

bool updateLang(KeyNum keyNo)
{
  switch (keyNo)
  {
  case KN_UP:
  case KN_DOWN:
  case KN_RIGHT:
  case KN_LEFT:
    LANG_INDEX = (LANG_INDEX + 1) % (LANG_MAX + 1);
    return true; // Value changed
  default:
    break;
  }
  return false; // No change
}

void dispBatItem()
{
  M5Cardputer.Display.fillRect(W_CHR * BATLVL_ITEM_POS, SC_LINES[0], W_CHR * BATLVL_ITEM_LEN, H_CHR, TFT_BLACK);
  M5Cardputer.Display.setFont(&fonts::lgfxJapanMincho_16);
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.drawString(BATLVL_TITLE[LANG_INDEX], W_CHR * BATLVL_ITEM_POS, SC_LINES[0]);
}

void dispMeasItem()
{
  // clear
  M5Cardputer.Display.fillRect(0, SC_LINES[7], X_WIDTH, H_CHR, TFT_BLACK);

  M5Cardputer.Display.setFont(&fonts::lgfxJapanMinchoP_16);
  M5Cardputer.Display.setTextSize(1);

  // temperature
  M5Cardputer.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5Cardputer.Display.drawCentreString(meas_items[0][LANG_INDEX], X_WIDTH * 1 / 4, SC_LINES[7]);

  // humidity
  M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5Cardputer.Display.drawCentreString(meas_items[1][LANG_INDEX], X_WIDTH * 3 / 4, SC_LINES[7]);
}

bool updateSettingValue(uint8_t &value, KeyNum keyNo, uint8_t min, uint8_t max, uint8_t step, uint8_t big_step)
{
  int tempValue = value;

  switch (keyNo)
  {
  case KN_UP:
    tempValue += big_step;
    break;
  case KN_DOWN:
    tempValue -= big_step;
    break;
  case KN_RIGHT:
    tempValue += step;
    break;
  case KN_LEFT:
    tempValue -= step;
    break;
  default:
    return false; // Not a value-changing key
  }

  // Clamp the value to the allowed range
  if (tempValue > max)
    tempValue = max;
  if (tempValue < min)
    tempValue = min;

  if (value != (uint8_t)tempValue)
  {
    value = (uint8_t)tempValue;
    return true; // Value changed
  }
  return false; // No change in value
}

void prtSetting(const char *msg, uint8_t data)
{
  char datBuf[4]; // message buffer
  snprintf(datBuf, sizeof(datBuf), "%3u", data);
  prtSetting(msg, datBuf);
}

void prtSetting(const char *msg, const char *data)
{
  // Line1 : setting display
  char msgBuf[31]; // message buffer
  snprintf(msgBuf, sizeof(msgBuf), "%s%s", msg, data);
  dbPrtln(msgBuf);

  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setFont(&fonts::lgfxJapanMincho_12);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillRect(0, SC_LINES[1], X_WIDTH, H_CHR, TFT_BLACK); // clear L1
  M5Cardputer.Display.drawString(msgBuf, W_CHR * SETTING_DISP_POS, SC_LINES[1]);
}

void changeBright(KeyNum keyNo)
{
  const uint8_t step_short = 1;
  const uint8_t step_big = 10;
  if (updateSettingValue(BRIGHT_LVL, keyNo, BRIGHT_LVL_MIN, BRIGHT_LVL_MAX, step_short, step_big))
  {
    M5Cardputer.Display.setBrightness(BRIGHT_LVL);
    wrtNVS(NVM_BRIGHT_TITLE, BRIGHT_LVL);
  }
  prtSetting("bright = ", BRIGHT_LVL);
}

void changeLowBatThr(KeyNum keyNo)
{
  const uint8_t step_short = 1;
  const uint8_t step_big = 10;
  if (updateSettingValue(LOWBAT_THRESHOLD, keyNo, LOWBAT_THRESHOLD_MIN, LOWBAT_THRESHOLD_MAX, step_short, step_big))
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
  // L1: (settings display line)
  // L2:
  // L3:
  // L4:
  // L5:
  // L6:
  // L7:   気温　℃         湿度　％
  // ---012345678901234567890123456789----

  M5Cardputer.Display.fillScreen(TFT_BLACK); // all clear
  M5Cardputer.Display.setFont(&fonts::lgfxJapanMincho_16);

  //--L0 : title--------------
  M5Cardputer.Display.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  M5Cardputer.Display.drawString(F("- DTH11 Sensor -"), 0, SC_LINES[0]);

  // L0 :Battery Level -----
  dispBatItem();
  M5Cardputer.Display.drawString(F("---"), W_CHR * BATLVL_VALUE_POS, SC_LINES[0]);
  M5Cardputer.Display.drawString(F("%"), W_CHR * BATLVL_PERCENT_POS, SC_LINES[0]);

  // L7 : Measuremnt items
  dispMeasItem();
}

static void loadSetting(const char *nvs_key, uint8_t &setting_variable, uint8_t default_value, uint8_t min_val, uint8_t max_val)
{
  uint8_t nvmData;
  bool needs_write = false;

  if (!rdNVS(nvs_key, nvmData))
  {
    nvmData = default_value;
    needs_write = true;
  }
  else if (nvmData < min_val || nvmData > max_val)
  {
    nvmData = default_value;
    needs_write = true;
  }

  setting_variable = nvmData;
  if (needs_write)
  {
    wrtNVS(nvs_key, setting_variable);
  }
}

void settingsInit()
{
  loadSetting(NVM_BRIGHT_TITLE, BRIGHT_LVL, BRIGHT_LVL_INIT, BRIGHT_LVL_MIN, BRIGHT_LVL_MAX);
  M5Cardputer.Display.setBrightness(BRIGHT_LVL);
  loadSetting(NVM_LOWBAT_TITLE, LOWBAT_THRESHOLD, LOWBAT_THRESHOLD_INIT, LOWBAT_THRESHOLD_MIN, LOWBAT_THRESHOLD_MAX);
  loadSetting(NVM_LANG_TITLE, LANG_INDEX, LANG_INIT, 0, LANG_MAX);
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

  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
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
static bool batCheck_first = true;
#define BATLVL_FLUCTUATION 5                            // fluctuation
const unsigned long BATTERY_CHECK_INTERVAL_MS = 1993UL; // Interval for battery level check
void batteryState()
{
  unsigned long currentTime = millis(); // Get current time once

  if (currentTime - PREV_BATCHK_TM < BATTERY_CHECK_INTERVAL_MS)
    return;

  // This will update consecutiveLowBatteryCount
  PREV_BATCHK_TM = currentTime;
  uint8_t batLvl = (uint8_t)M5Cardputer.Power.getBatteryLevel(); // Get battery level
  dbPrtln("batLvl: " + String(batLvl));
  if (batLvl > BATLVL_MAX)
    batLvl = BATLVL_MAX;

  lowBatteryCheck(batLvl);

  if (batCheck_first)
  {
    batCheck_first = false;
  }
  else
  { // ** stable battery level is valid **
    if (abs(batLvl - PREV_BATLVL) > BATLVL_FLUCTUATION)
    {
      PREV_BATLVL = batLvl;
      return;
    }
  }

  PREV_BATLVL = batLvl;
  prtBatLvl(batLvl);
}

static uint8_t PREV_BATLVL_DISP = 255; // Use an impossible value to force the first update
void prtBatLvl(uint8_t batLvl)
{
  // Line0 : battery level display
  //---- 012345678901234567890123456789---
  // L0_"                      bat.xxx%"--

  if (batLvl == PREV_BATLVL_DISP)
    return;
  PREV_BATLVL_DISP = batLvl;

  char msg[4] = ""; // message buffer
  snprintf(msg, sizeof(msg), "%3u", batLvl);
  dbPrtln(msg);

  M5Cardputer.Display.fillRect(W_CHR * BATLVL_VALUE_POS, SC_LINES[0], W_CHR * BATLVL_VALUE_LEN, H_CHR, TFT_BLACK); // clear
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setFont(&fonts::lgfxJapanMincho_16);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString(msg, W_CHR * BATLVL_VALUE_POS, SC_LINES[0]);
}

static uint8_t consecutiveLowBatteryCount = 0;
void lowBatteryCheck(uint8_t batLvl)
{
  const uint8_t LOWBAT_CONSECUTIVE_READINGS = 5;

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
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.drawCenterString(F("Low Battery !!"), X_WIDTH / 2, SC_LINES[3], &fonts::Font4);

    POWER_OFF();
    // never return
  }
}
