#include "N_util.h"

void setup();
void loop();
void dth11_sensor();
void dispInit();
void prtTempValue(float temp_val);
void prtHumiValue(float humi_val);
void battery_check();
void batDisp(uint8_t batLvl);
void lowBatteryCheck(uint8_t batLvl);

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

void setup()
{
  m5stack_begin();

  if (SD_ENABLE)
  { // M5stack-SD-Updater lobby
    SDU_lobby();
    SD.end();
  }

  // Initialize device.
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
  dispInit();
}

void loop()
{
  dth11_sensor();
  battery_check();
  vTaskDelay(1);
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

#define BATLVL_COL 22
#define TEMP_COL 3
#define HUMI_COL 19

void dispInit()
{
  // ---012345678901234567890123456789----
  // L0:-- DTH11 Sensor --    bat.---%
  // L1:
  // L2:
  // L3:
  // L4:
  // L5:
  // L6:
  // L7:   気温　℃         湿度　％
  // ---012345678901234567890123456789----

  M5Cardputer.Display.fillScreen(TFT_BLACK); // 画面塗り潰し
  M5Cardputer.Display.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  M5Cardputer.Display.setCursor(0, 0);
  //--------------------------"012345678901234567890123456789"------------------;
  M5Cardputer.Display.print(F("-- DTH11 Sensor --"));

  //  L0 :Battery Level ---------------------------------------
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * BATLVL_COL, SC_LINES[0]);
  M5Cardputer.Display.print(F("bat.---%"));

  M5Cardputer.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * TEMP_COL, SC_LINES[7]);
  //--------------------------"012345678901234567890123456789"--;
  M5Cardputer.Display.print(F("気温　℃"));

  M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * HUMI_COL, SC_LINES[7]);
  //----------"012345678901234567890123456789"--;
  M5Cardputer.Display.print(F("湿度　％"));
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  M5Cardputer.Display.setFont(&fonts::Font6);
  M5Cardputer.Display.setTextSize(1);
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

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillRect(W_CHR * column, SC_LINES[lineIndex], W_CHR * widthChars, H_CHR * 4, TFT_BLACK);
  M5Cardputer.Display.setCursor(W_CHR * column, SC_LINES[lineIndex]);
  M5Cardputer.Display.print(buf);
}

static float prev_temp = 0.0;
void prtTempValue(float temp_val)
{
  updateFloatValueDisplay(temp_val, prev_temp, 3, 1, 13);
}

static float prev_humi = 0.0;
void prtHumiValue(float humi_val)
{
  updateFloatValueDisplay(humi_val, prev_humi, 3, 17, 13);
}

static unsigned long PREV_BATCHK_TM = 0L;
static uint8_t PREV_BATLVL = 0;
static bool batChk_first = true;

void battery_check()
{
  const unsigned long BATCHK_INTVAL = 1993UL;
  unsigned long currentTime = millis(); // Get current time once

  if (currentTime - PREV_BATCHK_TM < BATCHK_INTVAL)
    return;

  // This will update consecutiveLowBatteryCount
  PREV_BATCHK_TM = currentTime;
  uint8_t batLvl = (uint8_t)M5.Power.getBatteryLevel(); // Get battery level
  dbPrtln("batLvl: " + String(batLvl));
  if (batLvl > 100)
    batLvl = 100;

  lowBatteryCheck(batLvl);

  if (batChk_first)
    batChk_first = false;
  else if( (batLvl == PREV_BATLVL) || (abs(batLvl - PREV_BATLVL) > 5))
    return;

  PREV_BATLVL = batLvl;
  batDisp(batLvl);
}

void batDisp(uint8_t batLvl)
{
  // Line0 : battery level display
  //---- 012345678901234567890123456789---
  // L0_"                      bat.xxx%"--
  const int BATVAL_POS = 26; // Battery value display start position
  const int BATVAL_LEN = 3;  // Battery value display length

  // clear
  M5Cardputer.Display.fillRect(W_CHR * BATVAL_POS, SC_LINES[0], W_CHR * BATVAL_LEN, H_CHR, TFT_BLACK);

  M5Cardputer.Display.setTextSize(0.3);
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  char batLvlBuf[4]; // Buffer for "XXX" + null terminator
  M5Cardputer.Display.setCursor(W_CHR * BATVAL_POS, SC_LINES[0] + 3);
  snprintf(batLvlBuf, sizeof(batLvlBuf), "%3u", batLvl);
  M5Cardputer.Display.print(batLvlBuf);
}

static uint8_t consecutiveLowBatteryCount = 0;
void lowBatteryCheck(uint8_t batLvl)
{
  const uint8_t LOWBAT_LVL_THRESHOLD = 10; // % : define LOW BATTERY lvl
  const uint8_t LOWBAT_CONSECUTIVE_READINGS = 5;
  const int LOWBAT_DISP_COL = 45;
  
  // Update consecutive low battery count
  if (batLvl < LOWBAT_LVL_THRESHOLD)
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
    M5Cardputer.Display.setCursor( LOWBAT_DISP_COL , SC_LINES[3]);
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.print(F("Low Battery !!"));
    POWER_OFF();
    // never return
  }
}
