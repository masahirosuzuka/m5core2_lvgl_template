#include <M5Core2.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include "lv_port_fs_sd.hpp"

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;
static const uint16_t tabWidth = 50;
static const uint16_t padding = 10;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];
static LGFX lcd;

void disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
    int32_t width = area->x2 - area->x1 + 1;
    int32_t height = area->y2 - area->y1 + 1;
    lcd.setAddrWindow(area->x1, area->y1,
                      width, height);
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
}

void setup()
{
  M5.begin(true, true, true, true);

  Serial.begin( 115200 );

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

  static lv_obj_t* rootScreen = lv_scr_act();

  lv_obj_t* tabView = lv_tabview_create(rootScreen, LV_DIR_LEFT, tabWidth);

  // ホームタブ
  lv_obj_t* homeTab = lv_tabview_add_tab(tabView, LV_SYMBOL_HOME);
  lv_obj_t* homeTabContainer = lv_obj_create(homeTab);
  lv_gridnav_add(homeTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(homeTabContainer, lv_pct(100), lv_pct(100));

  lv_obj_t* statusLabel = lv_label_create(homeTab);
  char batteryStatus[16];
  sprintf(batteryStatus, "%d%%%s", 99, LV_SYMBOL_BATTERY_FULL);
  lv_label_set_text(statusLabel, batteryStatus);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_RIGHT, -15, 10); // -15はlv_obj_get_widthで幅を取得するべきか？

  // WiFi/LTE/MQTT/証明書タブ
  lv_obj_t* connectionTab = lv_tabview_add_tab(tabView, LV_SYMBOL_WIFI);
  lv_obj_t* connectionTabContainer = lv_obj_create(connectionTab);
  lv_gridnav_add(connectionTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(connectionTabContainer, lv_pct(100), lv_pct(300));
  
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &mplus1_light);

  lv_obj_t* wifiLabel = lv_label_create(connectionTabContainer);
  lv_obj_add_style(wifiLabel, &style, 0);
  lv_label_set_text(wifiLabel, "WiFi");
  lv_obj_set_pos(wifiLabel, 0, 0);

  lv_obj_t* ssidLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ssidLabel, "SSID");
  lv_obj_set_pos(ssidLabel, 0, 40);

  lv_obj_t* ssidDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ssidDropdown, "SSID1\nSSID2");
  lv_obj_set_width(ssidDropdown, 140);
  lv_obj_set_pos(ssidDropdown, 50, 30);

  lv_obj_t* passwordLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(passwordLabel, "PASS");
  lv_obj_set_pos(passwordLabel, 0, 80);

  lv_obj_t* passwordTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(passwordTextarea, "");
  lv_textarea_set_password_mode(passwordTextarea, true);
  lv_textarea_set_one_line(passwordTextarea, true);
  lv_obj_set_width(passwordTextarea, 140);
  lv_obj_set_pos(passwordTextarea, 50, 70);

  lv_obj_t* mqttLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(mqttLabel, "MQTT");
  lv_obj_set_pos(mqttLabel, 0, 120);

  lv_obj_t* urlLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(urlLabel, "URL");
  lv_obj_set_pos(urlLabel, 0, 160);

  lv_obj_t* urlTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(urlTextarea, "");
  lv_textarea_set_one_line(urlTextarea, true);
  lv_obj_set_width(urlTextarea, 140);
  lv_obj_set_pos(urlTextarea, 50, 150);

  lv_obj_t* portLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(portLabel, "PORT");
  lv_obj_set_pos(portLabel, 0, 200);

  lv_obj_t* portTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(portTextarea, "");
  lv_textarea_set_one_line(portTextarea, true);
  lv_obj_set_width(portTextarea, 140);
  lv_obj_set_pos(portTextarea, 50, 190);

  lv_obj_t* certLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(certLabel, "Certification");
  lv_obj_set_pos(certLabel, 0, 240);

  lv_obj_t* rootCaLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(rootCaLabel, "Root");
  lv_obj_set_pos(rootCaLabel, 0, 280);

  lv_obj_t* rootCaDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(rootCaDropdown, "AmazonCA1");
  lv_obj_set_width(rootCaDropdown, 140);
  lv_obj_set_pos(rootCaDropdown, 50, 270);

  lv_obj_t* clientCertLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(clientCertLabel, "Cert");
  lv_obj_set_pos(clientCertLabel, 0, 320);

  lv_obj_t* clientCertDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(clientCertDropdown, "Cert1");
  lv_obj_set_width(clientCertDropdown, 140);
  lv_obj_set_pos(clientCertDropdown, 50, 310);

  lv_obj_t* keyLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(keyLabel, "Key");
  lv_obj_set_pos(keyLabel, 0, 360);

  lv_obj_t* keyDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(keyDropdown, "Key1");
  lv_obj_set_width(keyDropdown, 140);
  lv_obj_set_pos(keyDropdown, 50, 350);

  lv_obj_t* ntpLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpLabel, "NTP");
  lv_obj_set_pos(ntpLabel, 0, 400);

  lv_obj_t* ntpServerLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpServerLabel, "NTP");
  lv_obj_set_pos(ntpServerLabel, 0, 440);

  lv_obj_t* ntpDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ntpDropdown, "ntp.nict.jp");
  lv_obj_set_width(ntpDropdown, 140);
  lv_obj_set_pos(ntpDropdown, 50, 430);

  lv_obj_t* ntpSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t* ntpSaveButtonLabel = lv_label_create(ntpSaveButton);
  lv_label_set_text(ntpSaveButtonLabel, "Save");
  lv_obj_set_pos(ntpSaveButton, 50, 480);

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
  lv_obj_set_pos(activeScanEnableScan, 0, 40);

  lv_obj_t* activeScanSwitch = lv_switch_create(bluetoothTabContainer);
  lv_obj_add_state(activeScanSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(activeScanSwitch, 50, 30);

  lv_obj_t* rssiLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(rssiLabel, "RSSI");
  lv_obj_set_pos(rssiLabel, 0, 80);

  lv_obj_t* rssiSlider = lv_slider_create(bluetoothTabContainer);
  lv_obj_set_width(rssiSlider, 130);
  lv_obj_set_pos(rssiSlider, 60, 80);

  lv_obj_t* activeScanSaveButton = lv_btn_create(bluetoothTabContainer);
  lv_obj_t* activeScanSaveButtonLabel = lv_label_create(activeScanSaveButton);
  lv_label_set_text(activeScanSaveButtonLabel, "Save");
  lv_obj_set_pos(activeScanSaveButton, 50, 120);

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
  lv_obj_set_pos(gnssEnableLabel, 0, 40);

  lv_obj_t* gnssSwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(gnssSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(gnssSwitch, 50, 30);

  lv_obj_t* temperatureLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureLabel, "Temperature");
  lv_obj_set_pos(temperatureLabel, 0, 80);

  lv_obj_t* temperatureEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureEnableLabel, "ON");
  lv_obj_set_pos(temperatureEnableLabel, 0, 120);
    
  lv_obj_t* temperatureSwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(temperatureSwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(temperatureSwitch, 50, 110);

  lv_obj_t* humidityLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityLabel, "Humidity");
  lv_obj_set_pos(humidityLabel, 0, 160);

  lv_obj_t* humidityEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityEnableLabel, "ON");
  lv_obj_set_pos(humidityEnableLabel, 0, 200);

  lv_obj_t* humiditySwitch = lv_switch_create(sensorTabContainer);
  lv_obj_add_state(humiditySwitch, LV_STATE_CHECKED);
  lv_obj_set_pos(humiditySwitch, 50, 190);

  lv_obj_t* humiditySaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t* humiditySaveButtonLabel = lv_label_create(humiditySaveButton);
  lv_label_set_text(humiditySaveButtonLabel, "Save");
  lv_obj_set_pos(humiditySaveButton, 50, 240);

  Serial.println( "Setup done" );
}

void loop()
{
  M5.update();
  lv_tick_inc(5);
  lv_task_handler();
  delay(5);
}
