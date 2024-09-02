#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Core2.h>
#include <M5UnitENV.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <lvgl.h>
#include <lwip/ip.h>

#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

#include "lv_port_fs_sd.hpp"
#include "time.h"

#define JST 3600 * 9

// Preference
Preferences preferences;

// SystemBar
static const char *systemBarFormat = "%s %s %d%%";
char systemBarMessage[16];

// Status
bool ready = false;
static const char *booting = "Booting...";
static const char *stopped = "Stopped...";
static const char *running = "Running...";

char dashboardTempratureBuffer[8];
char dashboardHumidityBuffer[8];

// WiFi
static const char *ssidKey = "ssid";
static const char *passKey = "pass";

char ssid[33] = {0};
char pass[65] = {0};
String ssids = "";

static const IPAddress googleDNS(8, 8, 8, 8);
static const IPAddress googleDNS2(8, 8, 4, 4);

char macAddress[13] = {0};

// NTP
static const char *ntpKey = "ntp";
static const char *nictNTP = "ntp.nict.jp";
static const char *mfeedNTP = "ntp.jst.mfeed.ad.jp";

// MQTT
static const char *urlKey = "url";
static const char *portKey = "port";
static const char *topicKey = "topic";

char url[64];
int port = 0;

WiFiClientSecure wifiClientSecure = WiFiClientSecure();
PubSubClient mqttClient = PubSubClient(wifiClientSecure);

char clientId[16];
char topic[32];
static const char *notificationTopic = "notify";
char message[256];
JsonDocument messageJson;

// Cert
const char *rootCAKey = "rootCA";
const char *certKey = "cert";
const char *keyKey = "key";

char *rootCA;
char *cert;
char *key;

// BLE
const char *adv = "ADV";
const char *srp = "SRP";
struct Beacon {
  char type[4] = {0};
  char address[20] = {0};
  char payload[100] = {0};
  int rssi;
  long time;
};
QueueHandle_t queue;
const char *activeScanKey = "activeScan";
const char *rssiThresholdKey = "rssiThreshold";
NimBLEScan *bleScan;
const int scanTime = 3;
const int scanInterval = scanTime * 1000;
const int scanWindow = scanInterval - 100;
bool activeScan = false;
int rssiThreshold = -100;

// GPS
const char *gnssKey = "gnss";
bool gnssEnable;
double latitude = -1.0;
double longitude = -1.0;
SoftwareSerial softwareSerial;
TinyGPSPlus gps = TinyGPSPlus();

// Temprature / Humidity
const char *temperatureKey = "temperature";
bool temperatureEnable = false;
float temperature = 0.0;
const char *humidityKey = "humidity";
bool humidityEnable = false;
float humidity = 0.0;
SHT4X sht;

// Air pressure
const char *pressuerKey = "pressuer";
bool pressuerEnable = false;
float pressuer = 0.0;
BMP280 bmp;

// LVGL
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static const uint16_t tabWidth = 50;
static const uint16_t padding = 10;

static const char *wifiText = "WiFi";
static const char *ssidText = "SSID";
static const char *passText = "PASS";
static const char *mqttText = "MQTT";
static const char *urlText = "URL";
static const char *portText = "PORT";
static const char *topicText = "TOPIC";
static const char *certText = "Cert";
static const char *rootCAText = "Root";
static const char *keyText = "Key";
static const char *ntpText = "NTP";
static const char *bluetoothText = "Bluetooth";
static const char *activeScanText = "Active";
static const char *rssiText = "RSSI";
static const char *gpsText = "GPS";
static const char *temperatureText = "Temperature";
static const char *humidityText = "Humidity";
static const char *pressureText = "Air pressure";
static const char *enableText = "Enable";
static const char *saveText = "Save";
static const char *okText = "OK";
static const char *cancelText = "Cancel";

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 3];
static LGFX lcd;

static lv_obj_t *rootScreen;
static lv_obj_t *systemBar;
static lv_obj_t *status;
static lv_obj_t *dashboardTempertature;
static lv_obj_t *dashboardHumidity;
static lv_obj_t *keyboard;
lv_obj_t *messageBox;

void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;
  lcd.setAddrWindow(area->x1, area->y1, width, height);
  lcd.pushPixels((uint16_t *)color_p, width * height, true);

  lv_disp_flush_ready(disp);
}

void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (M5.Touch.ispressed()) {
    data->point.x = M5.Touch.getPressPoint().x;
    data->point.y = M5.Touch.getPressPoint().y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void open_keyboard() {
  if (keyboard == NULL) {
    keyboard = lv_keyboard_create(rootScreen);
  }
}

static void textarea_event_cb(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *textarea = lv_event_get_target(event);
  if (code == LV_EVENT_FOCUSED) {
    open_keyboard();
    lv_keyboard_set_textarea(keyboard, textarea);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }

  if (code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(keyboard, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

int getBattLevel() {
  float batVoltage = M5.Axp.GetBatVoltage();
  float batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;

  return (int)batPercentage;
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return (0);
  }
  time(&now);
  return now;
}

void updateSystemBar() {
  if (systemBar != NULL) {
    bool connected = WiFi.isConnected();
    int battLevel = getBattLevel();
    sprintf(systemBarMessage, systemBarFormat, gnssEnable ? LV_SYMBOL_GPS : " ", connected ? LV_SYMBOL_WIFI : " ", battLevel);
    lv_label_set_text(systemBar, systemBarMessage);
  }
}

void updateStatus() {
  if (status != NULL) {
    if (WiFi.isConnected() && mqttClient.connected()) {
      lv_label_set_text(status, running);
    } else {
      lv_label_set_text(status, stopped);
    }
  }
}

void updateDashboard() {
  if (temperatureEnable) {
    sprintf(dashboardTempratureBuffer, "%.1f °C", temperature);
    lv_label_set_text(dashboardTempertature, dashboardTempratureBuffer);
  }

  if (humidityEnable) {
    sprintf(dashboardHumidityBuffer, "%.1f %%", humidity);
    lv_label_set_text(dashboardHumidity, dashboardHumidityBuffer);
  }
}

void mqttCallback(const char *topic, byte *payload, unsigned int length) {
  Serial.print(topic);
  Serial.print(" : ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.print("\n");
}

class MyNimBLEAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    int rssi = advertisedDevice->getRSSI();
    if (rssi >= rssiThreshold) {
      if (mqttClient.connected()) {
        struct Beacon beacon;

        String bluetoothAddress = "";
        for (int i = 0; i < advertisedDevice->getAddress().toString().length(); i++) {
          if (advertisedDevice->getAddress().toString()[i] != ':') {
            bluetoothAddress += advertisedDevice->getAddress().toString()[i];
          }
        }
        bluetoothAddress.trim();
        bluetoothAddress.toUpperCase();
        sprintf(beacon.address, "%s", bluetoothAddress.c_str());

        String payload = "";
        for (int i = 0; i < advertisedDevice->getPayloadLength(); i++) {
          payload += String(advertisedDevice->getPayload()[i], HEX);
        }
        payload.trim();
        payload.toUpperCase();
        sprintf(beacon.payload, "%s", payload.c_str());

        beacon.rssi = rssi;
        beacon.time = getTime();

        if (queue != NULL) {
          xQueueSend(queue, &beacon, 10);
        }
      }
    }
    delay(1);
  }
};

void setup() {
  M5.begin(true, true, true, true);
  Serial.begin(115200);

  // Restore preferences
  preferences.begin("m5core2_app", false);
  sprintf(ssid, "%s", preferences.getString(ssidKey).c_str());
  sprintf(pass, "%s", preferences.getString(passKey).c_str());
  Serial.printf("ssid : %s  pass : %s\n", ssid, pass);

  sprintf(url, "%s", preferences.getString(urlKey).c_str());
  port = preferences.getInt(portKey, port);
  sprintf(topic, "%s", preferences.getString(topicKey).c_str());
  Serial.printf("mqttUrl : %s  port : %d topic : %s\n", url, port, topic);

  int rootCASize = preferences.getString(rootCAKey).length();
  if (rootCASize > 0) {
    rootCA = (char *)ps_malloc(rootCASize + 1);
    sprintf(rootCA, preferences.getString(rootCAKey).c_str());
    rootCA[rootCASize + 1] = '\0';
    Serial.printf("rootCA : %s\n", rootCA);
  }

  int certSize = preferences.getString(certKey).length();
  if (certSize > 0) {
    cert = (char *)ps_malloc(certSize);
    sprintf(cert, preferences.getString(certKey).c_str());
    Serial.printf("cert : %s\n", cert);
  }

  int keySize = preferences.getString(keyKey).length();
  if (keySize > 0) {
    key = (char *)ps_malloc(keySize);
    sprintf(key, preferences.getString(keyKey).c_str());
    Serial.printf("key : %s\n", key);
  }

  activeScan = preferences.getBool(activeScanKey);
  rssiThreshold = preferences.getInt(rssiThresholdKey);

  gnssEnable = preferences.getBool(gnssKey);
  if (gnssEnable) {
    // ポートをスキャンし、GPSユニットが接続されているか確認する
    softwareSerial.begin(9600, SWSERIAL_8N1, 33, 32, false);
    delay(500);
    if (softwareSerial.available() == 0) {
      // GPSが接続されていない
      Serial.println("Couldn't find GPS");
      gnssEnable = false;
      softwareSerial.end();
    }
  }

  temperatureEnable = preferences.getBool(temperatureKey, false);
  humidityEnable = preferences.getBool(humidityKey, false);
  if (temperatureEnable || humidityEnable) {
    if (!sht.begin(&Wire, SHT40_I2C_ADDR_44, 32, 33, 400000U)) {
      Serial.println("Couldn't find SHT40");
      temperatureEnable = false;
      humidityEnable = false;
    }
  }

  pressuerEnable = preferences.getBool(pressuerKey, false);
  if (pressuerEnable) {
    if (!bmp.begin(&Wire, BMP280_I2C_ADDR, 32, 33, 400000U)) {
      Serial.println("Couldn't find BMP280");
      pressuerEnable = false;
    } else {
      bmp.setSampling(BMP280::MODE_NORMAL, 
                      BMP280::SAMPLING_X2,
                      BMP280::SAMPLING_X16,
                      BMP280::FILTER_X16,
                      BMP280::STANDBY_MS_500);
    }
  }

  preferences.end();

  // Setup WiFi
  String wifiMac = WiFi.macAddress();
  wifiMac.replace(":", "");
  sprintf(macAddress, "%s", wifiMac.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, googleDNS, googleDNS2);
  WiFi.begin(ssid, pass);

  WiFi.waitForConnectResult();

  if (WiFi.isConnected()) {
    Serial.println("wifi connect OK");

    configTime(JST, 0, nictNTP); // 時間を同期

    sprintf(clientId, "m5stack-%s", wifiMac); // MQTTクライアントIDを設定

    wifiClientSecure.setCACert(rootCA);
    wifiClientSecure.setCertificate(cert);
    wifiClientSecure.setPrivateKey(key);

    mqttClient.setServer(url, port);
    mqttClient.connect(clientId);
    delay(1000);
    if (mqttClient.connected()) {
      ready = true;
    } else {
      ready = false;
    }
    mqttClient.disconnect();
  }

  int battLevel = getBattLevel();
  sprintf(systemBarMessage, systemBarFormat,
          gnssEnable ? LV_SYMBOL_GPS : " ", WiFi.isConnected() ? LV_SYMBOL_WIFI : " ", battLevel);

  // Setup bluetooth
  NimBLEDevice::init("");
  bleScan = NimBLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyNimBLEAdvertisedDeviceCallbacks());

  queue = xQueueCreate(5, sizeof(Beacon));

  // Setup LVGL
  String LVGL_Arduino = "Hello Arduino!!!!";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(LVGL_Arduino);
  Serial.println("I am LVGL_Arduino");

  lcd.begin();
  lcd.setBrightness(128);
  lcd.setColorDepth(24);

  /* Initialize the display */
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 3);
  lv_init();
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Initialize the input device driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  /* Initialize the filesystem driver */
  lv_port_fs_sd_init();

  // static lv_obj_t* loginScreen = lv_scr_act();
  // lv_obj_t* loginPage = lv_obj_create(loginScreen);

  rootScreen = lv_scr_act();

  lv_obj_t *tabView = lv_tabview_create(rootScreen, LV_DIR_BOTTOM, tabWidth);

  // ホームタブ
  lv_obj_t *homeTab = lv_tabview_add_tab(tabView, LV_SYMBOL_HOME);
  lv_obj_t *homeTabContainer = lv_obj_create(homeTab);
  lv_gridnav_add(homeTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(homeTabContainer, lv_pct(100), lv_pct(100));

  systemBar = lv_label_create(homeTabContainer);
  lv_label_set_text(systemBar, systemBarMessage);
  lv_obj_align(systemBar, LV_ALIGN_TOP_RIGHT, 0, 0);

  status = lv_label_create(homeTabContainer);
  lv_label_set_text(status, booting);
  lv_obj_set_pos(status, 0, 0);

  // ダッシュボード
  static lv_style_t dashboardLabelStyle;
  lv_style_init(&dashboardLabelStyle);
  lv_style_set_text_font(&dashboardLabelStyle, &lv_font_montserrat_34);

  dashboardTempertature = lv_label_create(homeTabContainer);
  lv_label_set_text(dashboardTempertature, "--.- °C");
  lv_obj_set_pos(dashboardTempertature, 15, 60);
  lv_obj_add_style(dashboardTempertature, &dashboardLabelStyle, 0);

  dashboardHumidity = lv_label_create(homeTabContainer);
  lv_label_set_text(dashboardHumidity, "--.- %");
  lv_obj_set_pos(dashboardHumidity, 150, 60);
  lv_obj_add_style(dashboardHumidity, &dashboardLabelStyle, 0);

  // WiFi/LTE/MQTT/証明書タブ
  lv_obj_t *connectionTab = lv_tabview_add_tab(tabView, LV_SYMBOL_WIFI);
  lv_obj_t *connectionTabContainer = lv_obj_create(connectionTab);
  lv_gridnav_add(connectionTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(connectionTabContainer, lv_pct(100), lv_pct(300));

  // static lv_style_t jp_style;
  // lv_style_init(&jp_style);
  // lv_style_set_text_font(&jp_style, &mplus1_light_14);

  lv_obj_t *wifiLabel = lv_label_create(connectionTabContainer);
  // lv_obj_add_style(wifiLabel, &jp_style, 0);
  lv_label_set_text(wifiLabel, wifiText);
  lv_obj_set_pos(wifiLabel, 0, 0);

  lv_obj_t *ssidLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ssidLabel, ssidText);
  lv_obj_set_pos(ssidLabel, 0, 50);

  static lv_obj_t *ssidDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ssidDropdown, ssid);
  lv_obj_set_width(ssidDropdown, 140);
  lv_obj_set_pos(ssidDropdown, 50, 40);
  lv_obj_add_event_cb(
      ssidDropdown,
      [](lv_event_t *event) {
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
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *passwordLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(passwordLabel, passText);
  lv_obj_set_pos(passwordLabel, 0, 100);

  static lv_obj_t *passwordTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(passwordTextarea, pass);
  lv_textarea_set_password_mode(passwordTextarea, true);
  lv_textarea_set_one_line(passwordTextarea, true);
  lv_textarea_set_max_length(passwordTextarea, 64);
  lv_obj_set_width(passwordTextarea, 140);
  lv_obj_set_pos(passwordTextarea, 50, 90);
  lv_obj_add_event_cb(passwordTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *wifiSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *wifiSaveButtonLabel = lv_label_create(wifiSaveButton);
  lv_label_set_text(wifiSaveButtonLabel, saveText);
  lv_obj_set_pos(wifiSaveButton, 50, 140);
  lv_obj_add_event_cb(
      wifiSaveButton,
      [](lv_event_t *event) {
        ready = false;

        lv_dropdown_get_selected_str(ssidDropdown, ssid, 33);
        sprintf(pass, "%s\0", lv_textarea_get_text(passwordTextarea), 65);
        Serial.printf("ssid : %s  pass : %s\n", ssid, pass);

        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "SSID and Password", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putString(ssidKey, ssid);
                preferences.putString(passKey, pass);
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *mqttLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(mqttLabel, mqttText);
  lv_obj_set_pos(mqttLabel, 0, 200);

  lv_obj_t *urlLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(urlLabel, urlText);
  lv_obj_set_pos(urlLabel, 0, 250);

  static lv_obj_t *urlTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(urlTextarea, url);
  lv_textarea_set_one_line(urlTextarea, true);
  lv_textarea_set_max_length(urlTextarea, 63);
  lv_obj_set_width(urlTextarea, 140);
  lv_obj_set_pos(urlTextarea, 50, 240);
  lv_obj_add_event_cb(urlTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *portLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(portLabel, "PORT");
  lv_obj_set_pos(portLabel, 0, 300);

  static lv_obj_t *portTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(portTextarea, String(port).c_str());
  lv_textarea_set_one_line(portTextarea, true);
  lv_textarea_set_accepted_chars(portTextarea, "0123456789");
  lv_textarea_set_max_length(portTextarea, 4);
  lv_obj_set_width(portTextarea, 140);
  lv_obj_set_pos(portTextarea, 50, 290);
  lv_obj_add_event_cb(portTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *topicLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(topicLabel, topicText);
  lv_obj_set_pos(topicLabel, 0, 350);

  static lv_obj_t *topicTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(topicTextarea, topic);
  lv_textarea_set_one_line(topicTextarea, true);
  lv_obj_set_width(topicTextarea, 140);
  lv_obj_set_pos(topicTextarea, 50, 340);
  lv_obj_add_event_cb(topicTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *mqttSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *mqttSaveButtonLabel = lv_label_create(mqttSaveButton);
  lv_label_set_text(mqttSaveButtonLabel, saveText);
  lv_obj_set_pos(mqttSaveButton, 50, 390);
  lv_obj_add_event_cb(
      mqttSaveButton,
      [](lv_event_t *event) {
        Serial.println("mqttSaveButton");
        ready = false;

        sprintf(url, "%s", lv_textarea_get_text(urlTextarea));
        port = atoi(lv_textarea_get_text(portTextarea));
        sprintf(topic, "%s", lv_textarea_get_text(topicTextarea));

        Serial.printf("%s, %d, %s\n", url, port, topic);

        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "MQTT settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putString(urlKey, url);
                preferences.putInt(portKey, port);
                preferences.putString(topicKey, topic);
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  String files = "";
  // SDカードからファイル一覧を取得する
  File root = SD.open("/");
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      if (strlen(file.name()) > 0) {
        if (file.name()[0] != '.') {  // 隠しファイルではないこと
          files += "/";
          files += file.name();
          files += "\n";
        }
      }
    }
    file = root.openNextFile();
  }
  file.close();

  lv_obj_t *certLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(certLabel, certText);
  lv_obj_set_pos(certLabel, 0, 450);

  lv_obj_t *rootCaLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(rootCaLabel, rootCAText);
  lv_obj_set_pos(rootCaLabel, 0, 500);

  static lv_obj_t *rootCaDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(rootCaDropdown, files.c_str());
  lv_obj_set_width(rootCaDropdown, 140);
  lv_obj_set_pos(rootCaDropdown, 50, 490);

  lv_obj_t *clientCertLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(clientCertLabel, certText);
  lv_obj_set_pos(clientCertLabel, 0, 550);

  static lv_obj_t *clientCertDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(clientCertDropdown, files.c_str());
  lv_obj_set_width(clientCertDropdown, 140);
  lv_obj_set_pos(clientCertDropdown, 50, 540);

  lv_obj_t *keyLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(keyLabel, keyText);
  lv_obj_set_pos(keyLabel, 0, 600);

  static lv_obj_t *keyDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(keyDropdown, files.c_str());
  lv_obj_set_width(keyDropdown, 140);
  lv_obj_set_pos(keyDropdown, 50, 590);

  lv_obj_t *certSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *certSaveButtonLabel = lv_label_create(certSaveButton);
  lv_label_set_text(certSaveButtonLabel, saveText);
  lv_obj_set_pos(certSaveButton, 50, 640);
  lv_obj_add_event_cb(
      certSaveButton,
      [](lv_event_t *event) {
        ready = false;

        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Certification files", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);

                char buf[32];
                File file;

                lv_dropdown_get_selected_str(rootCaDropdown, buf, 32);
                file = SD.open(buf, "r");
                rootCA = (char *)ps_malloc(file.size());
                file.readBytes(rootCA, file.size());
                preferences.putString(rootCAKey, rootCA);

                lv_dropdown_get_selected_str(clientCertDropdown, buf, 32);
                file = SD.open(buf, "r");
                cert = (char *)ps_malloc(file.size());
                file.readBytes(cert, file.size());
                preferences.putString(certKey, cert);

                lv_dropdown_get_selected_str(keyDropdown, buf, 32);
                file = SD.open(buf, "r");
                key = (char *)ps_malloc(file.size());
                file.readBytes(key, file.size());
                preferences.putString(keyKey, key);

                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *ntpLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpLabel, ntpText);
  lv_obj_set_pos(ntpLabel, 0, 700);

  lv_obj_t *ntpServerLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpServerLabel, urlText);
  lv_obj_set_pos(ntpServerLabel, 0, 750);

  lv_obj_t *ntpDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ntpDropdown, nictNTP);
  lv_obj_set_width(ntpDropdown, 140);
  lv_obj_set_pos(ntpDropdown, 50, 740);

  lv_obj_t *ntpSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *ntpSaveButtonLabel = lv_label_create(ntpSaveButton);
  lv_label_set_text(ntpSaveButtonLabel, saveText);
  lv_obj_set_pos(ntpSaveButton, 50, 790);
  lv_obj_add_event_cb(
      ntpSaveButton,
      [](lv_event_t *event) {
        // NTPの変更は未実装
      },
      LV_EVENT_CLICKED, NULL);

  // Bluetoothタブ
  lv_obj_t *bluetoothTab = lv_tabview_add_tab(tabView, LV_SYMBOL_BLUETOOTH);
  lv_obj_t *bluetoothTabContainer = lv_obj_create(bluetoothTab);
  lv_gridnav_add(bluetoothTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(bluetoothTabContainer, lv_pct(100), lv_pct(100));

  lv_obj_t *activeScanLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(activeScanLabel, bluetoothText);
  lv_obj_set_pos(activeScanLabel, 0, 0);

  lv_obj_t *activeScanEnableLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(activeScanEnableLabel, activeScanText);
  lv_obj_set_pos(activeScanEnableLabel, 0, 50);

  static lv_obj_t *activeScanSwitch = lv_switch_create(bluetoothTabContainer);
  if (activeScan) {
    lv_obj_add_state(activeScanSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(activeScanSwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(activeScanSwitch, 50, 40);

  lv_obj_t *rssiLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(rssiLabel, rssiText);
  lv_obj_set_pos(rssiLabel, 0, 100);

  static lv_obj_t *rssiThresholdLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text_fmt(rssiThresholdLabel, "%d", rssiThreshold);
  lv_obj_set_pos(rssiThresholdLabel, 210, 100);

  static lv_obj_t *rssiSlider = lv_slider_create(bluetoothTabContainer);
  lv_obj_set_width(rssiSlider, 130);
  lv_slider_set_range(rssiSlider, -120, 0);
  lv_slider_set_value(rssiSlider, rssiThreshold, LV_ANIM_OFF);
  lv_obj_set_pos(rssiSlider, 60, 100);
  lv_obj_add_event_cb(
      rssiSlider,
      [](lv_event_t *event) {
        lv_obj_t *slider = lv_event_get_target(event);
        int value = lv_slider_get_value(slider);
        lv_label_set_text_fmt(rssiThresholdLabel, "%d", value);
      },
      LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *bluetoothSaveButton = lv_btn_create(bluetoothTabContainer);
  lv_obj_t *bluetoothSaveButtonLabel = lv_label_create(bluetoothSaveButton);
  lv_label_set_text(bluetoothSaveButtonLabel, saveText);
  lv_obj_set_pos(bluetoothSaveButton, 50, 140);
  lv_obj_add_event_cb(
      bluetoothSaveButton,
      [](lv_event_t *event) {
        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Bluetooth settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putBool(activeScanKey, lv_obj_get_state(activeScanSwitch));
                preferences.putInt(rssiThresholdKey, lv_slider_get_value(rssiSlider));
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  // GPS/内蔵センサタブ
  lv_obj_t *sensorTab = lv_tabview_add_tab(tabView, LV_SYMBOL_PLUS);
  lv_obj_t *sensorTabContainer = lv_obj_create(sensorTab);
  lv_gridnav_add(sensorTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(sensorTabContainer, lv_pct(100), lv_pct(450));

  lv_obj_t *gnssLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(gnssLabel, gpsText);
  lv_obj_set_pos(gnssLabel, 0, 0);

  lv_obj_t *gnssEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(gnssEnableLabel, enableText);
  lv_obj_set_pos(gnssEnableLabel, 0, 50);

  static lv_obj_t *gnssSwitch = lv_switch_create(sensorTabContainer);
  if (gnssEnable) {
    lv_obj_add_state(gnssSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(gnssSwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(gnssSwitch, 50, 40);

  lv_obj_t *gnssSaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t *gnssSaveButtonLabel = lv_label_create(gnssSaveButton);
  lv_label_set_text(gnssSaveButtonLabel, saveText);
  lv_obj_set_pos(gnssSaveButton, 50, 100);
  lv_obj_add_event_cb(
      gnssSaveButton,
      [](lv_event_t *event) {
        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "GPS settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putBool(gnssKey, lv_obj_get_state(gnssSwitch));
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *temperatureLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureLabel, temperatureText);
  lv_obj_set_pos(temperatureLabel, 0, 150);

  lv_obj_t *temperatureEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(temperatureEnableLabel, enableText);
  lv_obj_set_pos(temperatureEnableLabel, 0, 200);

  static lv_obj_t *temperatureSwitch = lv_switch_create(sensorTabContainer);
  if (temperatureEnable) {
    lv_obj_add_state(temperatureSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(temperatureSwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(temperatureSwitch, 50, 190);

  lv_obj_t *temperatureSaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t *temperatureSaveButtonLabel = lv_label_create(temperatureSaveButton);
  lv_label_set_text(temperatureSaveButtonLabel, saveText);
  lv_obj_set_pos(temperatureSaveButton, 50, 250);
  lv_obj_add_event_cb(
      temperatureSaveButton,
      [](lv_event_t *event) {
        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Temprature settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putBool(temperatureKey, lv_obj_get_state(temperatureSwitch));
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *humidityLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityLabel, humidityText);
  lv_obj_set_pos(humidityLabel, 0, 300);

  lv_obj_t *humidityEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(humidityEnableLabel, enableText);
  lv_obj_set_pos(humidityEnableLabel, 0, 350);

  static lv_obj_t *humiditySwitch = lv_switch_create(sensorTabContainer);
  if (humidityEnable) {
    lv_obj_add_state(humiditySwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(humiditySwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(humiditySwitch, 50, 340);

  lv_obj_t *humiditySaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t *humiditySaveButtonLabel = lv_label_create(humiditySaveButton);
  lv_label_set_text(humiditySaveButtonLabel, saveText);
  lv_obj_set_pos(humiditySaveButton, 50, 400);
  lv_obj_add_event_cb(
      humiditySaveButton,
      [](lv_event_t *event) {
        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Humidity settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putBool(humidityKey, lv_obj_get_state(humiditySwitch));
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  lv_obj_t *pressureLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(pressureLabel, pressureText);
  lv_obj_set_pos(pressureLabel, 0, 450);

  lv_obj_t *pressureEnableLabel = lv_label_create(sensorTabContainer);
  lv_label_set_text(pressureEnableLabel, enableText);
  lv_obj_set_pos(pressureEnableLabel, 0, 500);

  static lv_obj_t *pressureSwitch = lv_switch_create(sensorTabContainer);
  if (pressuerEnable) {
    lv_obj_add_state(pressureSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(pressureSwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(pressureSwitch, 50, 490);

  lv_obj_t *pressureSaveButton = lv_btn_create(sensorTabContainer);
  lv_obj_t *pressureSaveButtonLabel = lv_label_create(pressureSaveButton);
  lv_label_set_text(pressureSaveButtonLabel, saveText);
  lv_obj_set_pos(pressureSaveButton, 50, 550);
  lv_obj_add_event_cb(
      pressureSaveButton,
      [](lv_event_t *event) {
        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Air pressure settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putBool(pressuerKey, lv_obj_get_state(pressureSwitch));
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  Serial.println("Setup done");
}

void loop() {
  M5.update();
  lv_tick_inc(5);
  lv_task_handler();

  if (ready) {
    if (WiFi.isConnected()) {
      if (mqttClient.connected()) {
        if (queue != NULL) {
          while (uxQueueMessagesWaiting(queue)) {
            struct Beacon beacon;
            if (xQueueReceive(queue, &beacon, portMAX_DELAY) == pdPASS) {
              messageJson["address"] = beacon.address;
              messageJson["gateway"] = macAddress;
              messageJson["payload"] = beacon.payload;
              messageJson["rssi"] = beacon.rssi;
              messageJson["time"] = beacon.time;
              messageJson["battery"] = getBattLevel();

              if (gnssEnable) {
                messageJson["latitude"] = latitude;
                messageJson["longitude"] = longitude;
              }

              if (temperatureEnable) {
                messageJson["temperature"] = temperature;
              }

              if (humidityEnable) {
                messageJson["humidity"] = humidity;
              }

              if (pressuerEnable) {
                messageJson["airpressuer"] = pressuer;
              }

              serializeJson(messageJson, message);
              Serial.println(message);
              mqttClient.publish(topic, message);
              delay(1);
            }
          }
        }

        mqttClient.loop();

        if (!bleScan->isScanning()) {
          bleScan->stop();
          bleScan->clearResults();
          bleScan->clearDuplicateCache();
          bleScan->setActiveScan(activeScan);
          bleScan->setInterval(scanInterval);
          bleScan->setWindow(scanWindow);
          bleScan->setDuplicateFilter(true);
          bleScan->start(scanTime, NULL, false);
        }
      } else {
        mqttClient.disconnect();
        mqttClient.setServer(url, port);
        mqttClient.setCallback(mqttCallback);
        if (mqttClient.connect(clientId)) {
          mqttClient.subscribe(notificationTopic);
        } else {
          // Serial.println("Reboot");
          // ESP.restart();
        }
        delay(500);
      }
    } else {
      WiFi.begin(ssid, pass);
      WiFi.waitForConnectResult();
    }

    if (gnssEnable) {
      while (softwareSerial.available() > 0) {
        int ch = softwareSerial.read();
        if (gps.encode(ch)) {
          if (gps.location.isValid() && gps.location.isUpdated()) {
            latitude = gps.location.lat();
            longitude = gps.location.lng();
            break;
          }
        }
      }
      delay(1);
    }

    if (temperatureEnable) {
      sht.update();
      temperature = sht.cTemp;
      delay(1);
    }

    if (humidityEnable) {
      sht.update();
      humidity = sht.humidity;
      delay(1);
    }

    if (pressuerEnable) {
      bmp.update();
      pressuer = bmp.pressure;
      delay(1);
    }
  }

  updateSystemBar();
  updateStatus();
  updateDashboard();

  delay(1);
}
