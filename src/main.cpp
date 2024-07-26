#include <M5Core2.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include "time.h"
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "lv_port_fs_sd.hpp"

#define JST 3600*9

// Preference
Preferences preferences;

// Status
static const char * statusBarFormat = "YYYY/MM/DD 00:00 %s %d%%";
char status[64] = { 0 };

// WiFi
static const char * ssidKey = "ssid";
static const char * passKey = "pass";

char ssid[33] = { 0 };
char pass[65] = { 0 };
String ssids = "";

// Certification

// MQTT

// BLE
BLEScan* bleScan;

// LVGL
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static const uint16_t tabWidth = 50;
static const uint16_t padding = 10;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];
static LGFX lcd;

static lv_obj_t* rootScreen;
static lv_obj_t* statusBar;
static lv_obj_t* keyboard;

void disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;
  lcd.setAddrWindow(area->x1, area->y1, width, height);
  lcd.pushPixels((uint16_t*)color_p, width * height, true);

  lv_disp_flush_ready(disp);
}

void touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
  if (M5.Touch.ispressed()) {
    data->point.x = M5.Touch.getPressPoint().x;
    data->point.y = M5.Touch.getPressPoint().y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void open_keyboard()
{
  if (keyboard == NULL) {
    keyboard = lv_keyboard_create(rootScreen);
  }
}

static void textarea_event_cb( lv_event_t * event )
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t* textarea = lv_event_get_target(event);
  if(code == LV_EVENT_FOCUSED) {
    open_keyboard();
    lv_keyboard_set_textarea(keyboard, textarea);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }

  if(code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

int getBattLevel() {
  float batVoltage = M5.Axp.GetBatVoltage();
  float batPercentage = ( batVoltage < 3.2 ) ? 0 : ( batVoltage - 3.2 ) * 100;

  return (int)batPercentage;
}

void updateStatusBar() {
  if (statusBar != NULL) {
    bool connected = WiFi.isConnected();
    int battLevel = getBattLevel();
    sprintf(status, statusBarFormat, connected ? LV_SYMBOL_WIFI : " ", battLevel);
    lv_label_set_text(statusBar, status);
  }
}

void setup()
{
  M5.begin(true, true, true, true);
  Serial.begin( 115200 );

  preferences.begin("m5core2_app", false);

  // Restore preferences
  sprintf(ssid, "%s", preferences.getString(ssidKey).c_str());
  sprintf(pass, "%s", preferences.getString(passKey).c_str());
  Serial.printf("ssid : %s  pass : %s\n", ssid, pass);

  // Setup WiFi
  if (strlen(ssid) > 0) {
    WiFi.begin(ssid, pass);
  } else {
    WiFi.begin();
  }
  delay(500);

  bool connected = WiFi.isConnected();
  int battLevel = getBattLevel();
  sprintf(status, statusBarFormat, connected ? LV_SYMBOL_WIFI : " ", battLevel);

  if (connected) {
    configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  }

  // Setup bluetooth
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();

  // Setup LVGL
  String LVGL_Arduino = "Hello Arduino!!!!";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println( LVGL_Arduino );
  Serial.println( "I am LVGL_Arduino" );

  lcd.begin();
  lcd.setBrightness(128);
  lcd.setColorDepth(24);

  /* Initialize the display */
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * 10 );
  lv_init();
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /* Initialize the input device driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register( &indev_drv );

  /* Initialize the filesystem driver */
  lv_port_fs_sd_init();

  //static lv_obj_t* loginScreen = lv_scr_act();
  //lv_obj_t* loginPage = lv_obj_create(loginScreen);

  rootScreen = lv_scr_act();

  lv_obj_t* tabView = lv_tabview_create(rootScreen, LV_DIR_LEFT, tabWidth);

  // ホームタブ
  lv_obj_t* homeTab = lv_tabview_add_tab(tabView, LV_SYMBOL_HOME);
  lv_obj_t* homeTabContainer = lv_obj_create(homeTab);
  lv_gridnav_add(homeTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(homeTabContainer, lv_pct(100), lv_pct(100));

  statusBar = lv_label_create(homeTab);
  lv_label_set_text(statusBar, status);
  lv_obj_align(statusBar, LV_ALIGN_TOP_RIGHT, -15, 10); // -15はlv_obj_get_widthで幅を取得するべきか？

  // WiFi/LTE/MQTT/証明書タブ
  lv_obj_t* connectionTab = lv_tabview_add_tab(tabView, LV_SYMBOL_WIFI);
  lv_obj_t* connectionTabContainer = lv_obj_create(connectionTab);
  lv_gridnav_add(connectionTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(connectionTabContainer, lv_pct(100), lv_pct(300));
  
  static lv_style_t jp_style;
  lv_style_init(&jp_style);
  lv_style_set_text_font(&jp_style, &mplus1_light_14);

  lv_obj_t* wifiLabel = lv_label_create(connectionTabContainer);
  lv_obj_add_style(wifiLabel, &jp_style, 0);
  lv_label_set_text(wifiLabel, "WiFi");
  lv_obj_set_pos(wifiLabel, 0, 0);

  lv_obj_t* ssidLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ssidLabel, "SSID");
  lv_obj_set_pos(ssidLabel, 0, 50);

  static lv_obj_t* ssidDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ssidDropdown, ssid);
  lv_obj_set_width(ssidDropdown, 140);
  lv_obj_set_pos(ssidDropdown, 50, 40);
  lv_obj_add_event_cb(ssidDropdown, [](lv_event_t * event){
    Serial.println("ssidDropdown");
    ssids = "";
    lv_dropdown_clear_options(ssidDropdown);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int networks = WiFi.scanNetworks();
    for (int i = 0; i < networks; i++) {
      Serial.println(WiFi.SSID(i) + " " + WiFi.channel(i) + " " + WiFi.RSSI(i));
      ssids += WiFi.SSID(i) + "\n";
    }
    if (networks > 0) {
      lv_dropdown_set_options(ssidDropdown, ssids.c_str());
    }
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t* passwordLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(passwordLabel, "PASS");
  lv_obj_set_pos(passwordLabel, 0, 100);

  static lv_obj_t* passwordTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(passwordTextarea, "");
  lv_textarea_set_password_mode(passwordTextarea, true);
  lv_textarea_set_one_line(passwordTextarea, true);
  lv_obj_set_width(passwordTextarea, 140);
  lv_obj_set_pos(passwordTextarea, 50, 90);
  lv_obj_add_event_cb(passwordTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t* wifiSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t* wifiSaveButtonLabel = lv_label_create(wifiSaveButton);
  lv_label_set_text(wifiSaveButtonLabel, "Save");
  lv_obj_set_pos(wifiSaveButton, 50, 140);
  lv_obj_add_event_cb(wifiSaveButton, [](lv_event_t * event) {
    lv_dropdown_get_selected_str(ssidDropdown, ssid, 33);
    sprintf(pass, "%s\0", lv_textarea_get_text(passwordTextarea), 65);
    Serial.printf("ssid : %s  pass : %s\n", ssid, pass);
    WiFi.begin(ssid, pass);
    delay(500);
    Serial.println("WiFi : " + WiFi.isConnected() ? "WiFi connect OK" : "WiFi connect FAIL");
    if (WiFi.isConnected()) {
      preferences.putString(ssidKey, ssid);
      preferences.putString(passKey, pass);
    }
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t* mqttLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(mqttLabel, "MQTT");
  lv_obj_set_pos(mqttLabel, 0, 200);

  lv_obj_t* urlLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(urlLabel, "URL");
  lv_obj_set_pos(urlLabel, 0, 250);

  lv_obj_t* urlTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(urlTextarea, "");
  lv_textarea_set_one_line(urlTextarea, true);
  lv_obj_set_width(urlTextarea, 140);
  lv_obj_set_pos(urlTextarea, 50, 240);
  lv_obj_add_event_cb(urlTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t* portLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(portLabel, "PORT");
  lv_obj_set_pos(portLabel, 0, 300);

  lv_obj_t* portTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(portTextarea, "");
  lv_textarea_set_one_line(portTextarea, true);
  lv_obj_set_width(portTextarea, 140);
  lv_obj_set_pos(portTextarea, 50, 290);
  lv_obj_add_event_cb(portTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t* mqttSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t* mqttSaveButtonLabel = lv_label_create(mqttSaveButton);
  lv_label_set_text(mqttSaveButtonLabel, "Save");
  lv_obj_set_pos(mqttSaveButton, 50, 340);

  lv_obj_t* certLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(certLabel, "Certification");
  lv_obj_set_pos(certLabel, 0, 400);

  lv_obj_t* rootCaLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(rootCaLabel, "Root");
  lv_obj_set_pos(rootCaLabel, 0, 450);

  lv_obj_t* rootCaDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(rootCaDropdown, "AmazonCA1");
  lv_obj_set_width(rootCaDropdown, 140);
  lv_obj_set_pos(rootCaDropdown, 50, 440);

  lv_obj_t* clientCertLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(clientCertLabel, "Cert");
  lv_obj_set_pos(clientCertLabel, 0, 500);

  lv_obj_t* clientCertDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(clientCertDropdown, "Cert1");
  lv_obj_set_width(clientCertDropdown, 140);
  lv_obj_set_pos(clientCertDropdown, 50, 490);

  lv_obj_t* keyLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(keyLabel, "Key");
  lv_obj_set_pos(keyLabel, 0, 550);

  lv_obj_t* keyDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(keyDropdown, "Key1");
  lv_obj_set_width(keyDropdown, 140);
  lv_obj_set_pos(keyDropdown, 50, 540);

  lv_obj_t* certSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t* certSaveButtonLabel = lv_label_create(certSaveButton);
  lv_label_set_text(certSaveButtonLabel, "Save");
  lv_obj_set_pos(certSaveButton, 50, 590);

  lv_obj_t* ntpLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpLabel, "NTP");
  lv_obj_set_pos(ntpLabel, 0, 650);

  lv_obj_t* ntpServerLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpServerLabel, "NTP");
  lv_obj_set_pos(ntpServerLabel, 0, 700);

  lv_obj_t* ntpDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ntpDropdown, "ntp.nict.jp");
  lv_obj_set_width(ntpDropdown, 140);
  lv_obj_set_pos(ntpDropdown, 50, 690);

  lv_obj_t* ntpSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t* ntpSaveButtonLabel = lv_label_create(ntpSaveButton);
  lv_label_set_text(ntpSaveButtonLabel, "Save");
  lv_obj_set_pos(ntpSaveButton, 50, 740);

  // Bluetoothタブ
  lv_obj_t* bluetoothTab = lv_tabview_add_tab(tabView, LV_SYMBOL_BLUETOOTH);
  lv_obj_t* bluetoothTabContainer = lv_obj_create(bluetoothTab);
  lv_gridnav_add(bluetoothTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(bluetoothTabContainer, lv_pct(100), lv_pct(100));

  lv_obj_t* activeScanLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(activeScanLabel, "Active scan");
  lv_obj_set_pos(activeScanLabel, 0, 0);

  lv_obj_t* activeScanEnableScan = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(activeScanEnableScan, "ON");
  lv_obj_set_pos(activeScanEnableScan, 0, 50);

  lv_obj_t* activeScanSwitch = lv_switch_create(bluetoothTabContainer);
  lv_obj_add_state(activeScanSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(activeScanSwitch, 50, 40);

  lv_obj_t* rssiLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(rssiLabel, "RSSI");
  lv_obj_set_pos(rssiLabel, 0, 100);

  lv_obj_t* rssiSlider = lv_slider_create(bluetoothTabContainer);
  lv_obj_set_width(rssiSlider, 130);
  lv_obj_set_pos(rssiSlider, 60, 100);

  lv_obj_t* activeScanSaveButton = lv_btn_create(bluetoothTabContainer);
  lv_obj_t* activeScanSaveButtonLabel = lv_label_create(activeScanSaveButton);
  lv_label_set_text(activeScanSaveButtonLabel, "Save");
  lv_obj_set_pos(activeScanSaveButton, 50, 140);

  // GPS/内蔵センサタブ
  lv_obj_t* sensorTab = lv_tabview_add_tab(tabView, LV_SYMBOL_GPS);
  lv_obj_t* sensorTabContainer = lv_obj_create(sensorTab);
  lv_gridnav_add(sensorTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(sensorTabContainer, lv_pct(100), lv_pct(200));

  lv_obj_t* gnssLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(gnssLabel, "GPS");
  lv_obj_set_pos(gnssLabel, 0, 0);

  lv_obj_t* gnssEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(gnssEnableLabel, "ON");
  lv_obj_set_pos(gnssEnableLabel, 0, 50);

  lv_obj_t* gnssSwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(gnssSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(gnssSwitch, 50, 40);

  lv_obj_t* temperatureLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureLabel, "Temperature");
  lv_obj_set_pos(temperatureLabel, 0, 100);

  lv_obj_t* temperatureEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureEnableLabel, "ON");
  lv_obj_set_pos(temperatureEnableLabel, 0, 150);
    
  lv_obj_t* temperatureSwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(temperatureSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(temperatureSwitch, 50, 140);

  lv_obj_t* humidityLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityLabel, "Humidity");
  lv_obj_set_pos(humidityLabel, 0, 200);

  lv_obj_t* humidityEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityEnableLabel, "ON");
  lv_obj_set_pos(humidityEnableLabel, 0, 250);

  lv_obj_t* humiditySwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(humiditySwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(humiditySwitch, 50, 240);

  lv_obj_t* humiditySaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t* humiditySaveButtonLabel = lv_label_create(humiditySaveButton);
  lv_label_set_text(humiditySaveButtonLabel, "Save");
  lv_obj_set_pos(humiditySaveButton, 50, 300);

  Serial.println( "Setup done" );
}

void loop()
{
  M5.update();
  lv_tick_inc(5);
  lv_task_handler();
  delay(5);
  updateStatusBar();
}
