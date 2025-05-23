#include <ctype.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Core2.h>
#include <M5UnitENV.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <lvgl.h>
#include <lwip/ip.h>
#include "esp_log.h"

#include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>

#include "lv_port_fs_sd.hpp"
#include "time.h"

#define TINY_GSM_MODEM_SIM7080
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#include <TinyGSM.h>

#include "sim7080g_client.hpp"

#define JST 3600 * 9

static const char * TAG = "main";

HardwareSerial portASerial(2);

// Preference
Preferences preferences;

// SystemBar
static const char *systemBarFormat = "%s %s %s %d%%";
char systemBarText[16];

// Status
static const char *booting = "Booting...";
static const char *stopped = "Stopped...";
static const char *running = "Running...";

char dashboardTempratureBuffer[16];
char dashboardHumidityBuffer[16];

// WiFi
static const char *ssidKey = "ssid";
static const char *passKey = "pass";
static const int macAddressLength = 12;

char ssid[33] = {0};
char pass[65] = {0};
String ssids = "";

static const IPAddress googleDNS(8, 8, 8, 8);
static const IPAddress googleDNS2(8, 8, 4, 4);
char macAddress[macAddressLength + 1] = {0};

WiFiClientSecure wifiClientSecure = WiFiClientSecure();

bool wifiReady = false;

// Mobile
static const char *gsmPortKey = "apnPort";
static const char *apnKey = "apn";
static const char *apnUserKey = "apnUser";
static const char *apnPassKey = "apnPass";

static const int gsmPortNone = 0;
static const int gsmPortA = 1;

int gsmPort = gsmPortNone;
char apn[32] = {0};
char apnUser[32] = {0};
char apnPass[32] = {0};

static const char *modemRootCaFileName = "rootCa.pem";
static const char *modemCertFileName = "cert.pem";
static const char *modemPrivateKeyFileName = "key.pem";

//
// SIMCOM 7080G/7090GはAmazonRootCaを使えない？？？
//
static const char *awsClass2RootFileName = "awsClass2Root.crt";
PROGMEM static const char *awsClass2Root = 
"-----BEGIN CERTIFICATE-----" \
"MIIEDzCCAvegAwIBAgIBADANBgkqhkiG9w0BAQUFADBoMQswCQYDVQQGEwJVUzEl" \
"MCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMp" \
"U3RhcmZpZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMDQw" \
"NjI5MTczOTE2WhcNMzQwNjI5MTczOTE2WjBoMQswCQYDVQQGEwJVUzElMCMGA1UE" \
"ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMpU3RhcmZp" \
"ZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggEgMA0GCSqGSIb3" \
"DQEBAQUAA4IBDQAwggEIAoIBAQC3Msj+6XGmBIWtDBFk385N78gDGIc/oav7PKaf" \
"8MOh2tTYbitTkPskpD6E8J7oX+zlJ0T1KKY/e97gKvDIr1MvnsoFAZMej2YcOadN" \
"+lq2cwQlZut3f+dZxkqZJRRU6ybH838Z1TBwj6+wRir/resp7defqgSHo9T5iaU0" \
"X9tDkYI22WY8sbi5gv2cOj4QyDvvBmVmepsZGD3/cVE8MC5fvj13c7JdBmzDI1aa" \
"K4UmkhynArPkPw2vCHmCuDY96pzTNbO8acr1zJ3o/WSNF4Azbl5KXZnJHoe0nRrA" \
"1W4TNSNe35tfPe/W93bC6j67eA0cQmdrBNj41tpvi/JEoAGrAgEDo4HFMIHCMB0G" \
"A1UdDgQWBBS/X7fRzt0fhvRbVazc1xDCDqmI5zCBkgYDVR0jBIGKMIGHgBS/X7fR" \
"zt0fhvRbVazc1xDCDqmI56FspGowaDELMAkGA1UEBhMCVVMxJTAjBgNVBAoTHFN0" \
"YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4xMjAwBgNVBAsTKVN0YXJmaWVsZCBD" \
"bGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8w" \
"DQYJKoZIhvcNAQEFBQADggEBAAWdP4id0ckaVaGsafPzWdqbAYcaT1epoXkJKtv3" \
"L7IezMdeatiDh6GX70k1PncGQVhiv45YuApnP+yz3SFmH8lU+nLMPUxA2IGvd56D" \
"eruix/U0F47ZEUD0/CwqTRV/p2JdLiXTAAsgGh1o+Re49L2L7ShZ3U0WixeDyLJl" \
"xy16paq8U4Zt3VekyvggQQto8PT7dL5WXXp59fkdheMtlb71cZBDzI0fmgAKhynp" \
"VSJYACPq4xJDKVtHCN2MQWplBqjlIapBtJUhlbl90TSrE9atvNziPTnNvT51cKEY" \
"WQPJIrSPnNVeKtelttQKbfi3QBFGmh95DmK/D5fs4C8fF5Q=" \
"-----END CERTIFICATE-----";

SIM7080GClient sim7080gClient = SIM7080GClient();
bool gsmReady = false;
bool gsmGPSReady = false;

// MQTT
static const char *urlKey = "url";
static const char *portKey = "port";
static const char *clientIdKey = "clientId";
static const char *tlsKey = "tls";
static const char *topicKey = "topic";

char url[64];
int port = 0;
bool tls;

char clientId[32];
char topic[32];
static const char *notificationTopic = "notify";
char message[1024];
JsonDocument messageJson;
static const int sourceTypeBeacon = 0;
static const int sourceTypeTimer = 1;
int retry = 0;

PubSubClient mqttClient = PubSubClient(wifiClientSecure);

// Cert
static const char *rootCAKey = "rootCA";
static const char *certKey = "cert";
static const char *keyKey = "key";

char *rootCA;
char *cert;
char *key;

// NTP
static const char *ntpKey = "ntp";
static const char *nictNTP = "ntp.nict.jp";
static const char *mfeedNTP = "ntp.jst.mfeed.ad.jp";

// BLE
static const char *scanEnableKey = "scanEnable";
static const char *activeScanKey = "activeScan";
static const char *rssiThresholdKey = "rssiThreshold";
static const int bluetoothAddressLength = macAddressLength;
static const int advertisingPayloadLength = 31 * 2;
static const int scanResponsePayloadLength = advertisingPayloadLength;
bool scanEnable = true;

struct Beacon {
  int source;
  char address[bluetoothAddressLength + 1] = {0};
  char payload[advertisingPayloadLength + scanResponsePayloadLength + 1] = {0};
  int rssi;
  long timestamp;
};
QueueHandle_t queue;
NimBLEScan *bleScan;
static const int scanTime = 3;
static const int scanInterval = scanTime * 1000;
static const int scanWindow = scanInterval - 100;
bool activeScan = false;
int rssiThreshold = -100;

// Port
static const char *supportedUnits = "None\nGPS\nENV IV Unit";
static const int none = 0;
static const int gpsUnit = 1;
static const int env4Unit = 2;
static const int catMGNSSUnit = 3;

struct Port {
  int type = none;
  bool ready;

  // GPS
  // https://docs.m5stack.com/ja/unit/gps
  TinyGPSPlus gps = TinyGPSPlus();

  // ENV IV Unit
  // https://docs.m5stack.com/ja/unit/ENV%E2%85%A3%20Unit
  SHT4X sht = SHT4X();
  BMP280 bmp = BMP280();
};

// Port A
static const char *portAKey = "portA";
Port portA;

// Timer
static const char *timerIntervalKey = "timerInterval";
static const char *timerIntervals ="None\n10 min\n30 min\n60 min";

static const uint64_t sec = 1000000;
static const uint64_t timerIntervalNone = 0;
static const uint64_t timerInterval10min = 10 * 60 * sec;
static const uint64_t timerInterval30min = 30 * 60 * sec;
static const uint64_t timerInterval60min = 60 * 60 * sec;

uint64_t timerInterval = timerInterval10min;
hw_timer_t* timer;

// LVGL
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static const uint16_t tabWidth = 50;
static const uint16_t padding = 10;

static const char *wifiText = "WiFi";
static const char *ssidText = "SSID";
static const char *passText = "PASS";
static const char *gsmText = "Mobile";
static const char *apnText = "APN";
static const char *userText = "USER";
static const char *mqttText = "MQTT";
static const char *urlText = "URL";
static const char *portText = "PORT";
static const char *tlsText = "TLS";
static const char *clientIdText = "ID";
static const char *topicText = "TOPIC";
static const char *certText = "Cert";
static const char *rootCAText = "Root";
static const char *keyText = "Key";
static const char *ntpText = "NTP";
static const char *bluetoothText = "Bluetooth";
static const char *scanText = "Scan";
static const char *activeScanText = "Active";
static const char *rssiText = "RSSI";
static const char *sensorsText = "Sensors";
static const char *timerText = "Timer";
static const char *portAText = "Port A";
static const char *unitText = "Type";
static const char *gpsText = "GPS";
static const char *enableText = "Enable";
static const char *saveText = "Save";
static const char *okText = "OK";
static const char *cancelText = "Cancel";

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 3];
static LGFX lcd;

static lv_obj_t *rootScreen;
static lv_obj_t *systemBar;
static lv_obj_t *connectionStatus;
static lv_obj_t *dashboardTempertature;
static lv_obj_t *dashboardHumidity;
static lv_obj_t *keyboard;
lv_obj_t *messageBox;

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;
  lcd.setAddrWindow(area->x1, area->y1, width, height);
  lcd.pushPixels((uint16_t *)color_p, width * height, true);

  lv_disp_flush_ready(disp);
}

static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (M5.Touch.ispressed()) {
    data->point.x = M5.Touch.getPressPoint().x;
    data->point.y = M5.Touch.getPressPoint().y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void open_keyboard() {
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

static int getBatLevel() {
  float batVoltage = M5.Axp.GetBatVoltage();
  float batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;

  return (int)batPercentage;
}

static unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return (0);
  }
  time(&now);
  return now;
}

static void getWiFiMac() {
  String wifiMac = WiFi.macAddress();
  wifiMac.replace(":", "");
  wifiMac.toUpperCase();
  sprintf(macAddress, "%s", wifiMac.c_str());
}

static void updateSystemBar() {
  if (systemBar != NULL) {
    bool gsmConnected = gsmReady;
    bool wifiConnected = WiFi.isConnected();
    int battLevel = getBatLevel();
    sprintf(systemBarText, systemBarFormat, ((portA.type == gpsUnit) && portA.ready) ? LV_SYMBOL_GPS : " ", gsmConnected ? LV_SYMBOL_CALL : " ", wifiConnected ? LV_SYMBOL_WIFI : " ", battLevel);
    lv_label_set_text(systemBar, systemBarText);
  }
}

static void updateStatus() {
  if (connectionStatus != NULL) {
    if ((WiFi.isConnected() && mqttClient.connected()) || gsmReady) {
      lv_label_set_text(connectionStatus, running);
    } else {
      lv_label_set_text(connectionStatus, stopped);
    }
  }
}

static void updateDashboard() {
  if (portA.type == env4Unit && portA.ready) {
    sprintf(dashboardTempratureBuffer, "%.1f °C", portA.sht.cTemp);
    lv_label_set_text(dashboardTempertature, dashboardTempratureBuffer);
    sprintf(dashboardHumidityBuffer, "%.1f %%", portA.sht.humidity);
    lv_label_set_text(dashboardHumidity, dashboardHumidityBuffer);
  }
}

static void mqttCallback(const char *topic, byte *payload, unsigned int length) {
  ESP_LOGD(TAG, "topic : %s\n", topic);
  ESP_LOGD(TAG, "payload : %s\n", payload);
}

static void IRAM_ATTR onTimer() {
  ESP_LOGD(TAG, "onTimer");
  if (queue != NULL) {
    struct Beacon beacon;

    beacon.source = sourceTypeTimer;
    //TODO: 修正
    //beacon.timestamp = getTime(); //ここでgetTimeを呼ぶとクラッシュする？？？

    xQueueSend(queue, &beacon, portMAX_DELAY);
  }
}

class MyNimBLEAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    int rssi = advertisedDevice->getRSSI();
    if (rssi >= rssiThreshold) {
      if (mqttClient.connected() || gsmReady) {
        struct Beacon beacon;

        beacon.source = sourceTypeBeacon;

        char bluetoothAddress[bluetoothAddressLength + 1] = {0};
        int index = 0;
        for (int i = 5; i >= 0; i--) {
          index += sprintf(&bluetoothAddress[index], "%02X", advertisedDevice->getAddress().getNative()[i]);
        }
        sprintf(beacon.address, "%s", bluetoothAddress);

        char payload[advertisingPayloadLength + scanResponsePayloadLength + 1] = {0};
        if (advertisedDevice->getPayloadLength() < (advertisingPayloadLength + scanResponsePayloadLength)) {
          int index = 0;
          for (int i = 0; i < advertisedDevice->getPayloadLength(); i++) {
            index += sprintf(&payload[index], "%02X", advertisedDevice->getPayload()[i]);
          }
          sprintf(beacon.payload, "%s", payload);
        }

        beacon.rssi = rssi;
        beacon.timestamp = getTime();

        if (queue != NULL) {
          xQueueSend(queue, &beacon, portMAX_DELAY);
        }
      }
    }
    delay(1);
  }
};

MyNimBLEAdvertisedDeviceCallbacks callbacks = MyNimBLEAdvertisedDeviceCallbacks();

void setup() {
  M5.begin(true, true, true, true);
  Serial.begin(115200);

  getWiFiMac();

  // Restore preferences
  preferences.begin("m5core2_app", false);
  sprintf(ssid, "%s", preferences.getString(ssidKey, "").c_str());
  sprintf(pass, "%s", preferences.getString(passKey, "").c_str());
  ESP_LOGD(TAG, "ssid : %s  pass : %s\n", ssid, pass);

  gsmPort = preferences.getInt(gsmPortKey, gsmPortNone);
  sprintf(apn, "%s", preferences.getString(apnKey, "").c_str());
  sprintf(apnUser, "%s", preferences.getString(apnUserKey, "").c_str());
  sprintf(apnPass, "%s", preferences.getString(apnPassKey, "").c_str());
  ESP_LOGD(TAG, "port : %d apn : %s user : %s pass : %s\n", gsmPort, apn, apnUser, apnPass);

  sprintf(url, "%s", preferences.getString(urlKey, "").c_str());
  port = preferences.getInt(portKey, port);
  tls = preferences.getBool(tlsKey, true);
  char defaultClientId[32] = {0};
  sprintf(defaultClientId, "m5stack-%s", macAddress);
  sprintf(clientId, "%s", preferences.getString(clientIdKey, defaultClientId));
  sprintf(topic, "%s", preferences.getString(topicKey).c_str(), "topic");
  ESP_LOGD(TAG, "mqttUrl : %s port : %d clientId : %s topic : %s\n", url, port, clientId, topic);

  int rootCASize = preferences.getString(rootCAKey, "").length();
  if (rootCASize > 0) {
    rootCA = (char *)ps_malloc(rootCASize + 1);
    sprintf(rootCA, preferences.getString(rootCAKey, "").c_str());
    rootCA[rootCASize + 1] = '\0';
    ESP_LOGD(TAG, "rootCA : %s\n", rootCA);
  }

  int certSize = preferences.getString(certKey, "").length();
  if (certSize > 0) {
    cert = (char *)ps_malloc(certSize + 1);
    sprintf(cert, preferences.getString(certKey, "").c_str());
    ESP_LOGD(TAG, "cert : %s\n", cert);
  }

  int keySize = preferences.getString(keyKey, "").length();
  if (keySize > 0) {
    key = (char *)ps_malloc(keySize + 1);
    sprintf(key, preferences.getString(keyKey, "").c_str());
    ESP_LOGD(TAG, "key : %s\n", key);
  }

  scanEnable = preferences.getBool(scanEnableKey, true);
  activeScan = preferences.getBool(activeScanKey, false);
  rssiThreshold = preferences.getInt(rssiThresholdKey, rssiThreshold);

  timerInterval = preferences.getInt(timerIntervalKey, timerIntervalNone);
  portA.type = preferences.getInt(portAKey);

  preferences.end();

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, googleDNS, googleDNS2);
  WiFi.begin(ssid, pass);

  WiFi.waitForConnectResult();

  if (WiFi.isConnected()) {
    ESP_LOGD(TAG, "WiFi connect OK\n");

    configTime(JST, 0, nictNTP);  // 時間を同期

    if (tls) {
      wifiClientSecure.setCACert(rootCA);
      wifiClientSecure.setCertificate(cert);
      wifiClientSecure.setPrivateKey(key);
    } else {
      wifiClientSecure.setInsecure();
    }

    mqttClient.setServer(url, port);
    mqttClient.connect(clientId);
    delay(1000);
    if (mqttClient.connected()) {
      wifiReady = true;
    } else {
      wifiReady = false;
    }
    mqttClient.disconnect();
  }

  // Setup Mobile Network
  if (gsmPort > gsmPortNone) {
    ESP_LOGD(TAG, "Setup GSM");
    portASerial.begin(115200, SERIAL_8N1, 33, 32, false);
    TinyGsmAutoBaud(portASerial, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX); //これを入れると何故か動くようになる
    ESP_LOGD(TAG, "portASerial baud : %d", portASerial.baudRate());
    if (portASerial.baudRate() == 9600) {
      ESP_LOGD(TAG, "Couldn't find GSM module");
      gsmReady = false;
      portASerial.end();
      goto finish_gsm_setup;
    }

    // 動作確認
    if (!sim7080gClient.deviceConnected(portASerial)) {
      gsmReady = false;
      portASerial.end();
      goto finish_gsm_setup;
    }

    if ((strlen(apn) > 0) && (strlen(apnUser) > 0) && (strlen(apnPass) > 0)) {
      if (!sim7080gClient.SIMReady(portASerial)) {
        gsmReady = false;
        portASerial.end();
        goto finish_gsm_setup;
      }

      sim7080gClient.connectAPN(portASerial, apn, apnUser, apnPass);
      if (!sim7080gClient.isOnline(portASerial)) {
        gsmReady = false;
        portASerial.end();
        goto finish_gsm_setup;
      }

      if (tls) {
        sim7080gClient.setCaCert(portASerial, awsClass2RootFileName, awsClass2Root, strlen(awsClass2Root));
        sim7080gClient.setCert(portASerial, modemCertFileName, cert, certSize);
        sim7080gClient.setKey(portASerial, modemPrivateKeyFileName, key, keySize);
        sim7080gClient.useTLS(portASerial, awsClass2RootFileName, modemCertFileName, modemPrivateKeyFileName);
        sim7080gClient.setSSLVersion(portASerial, 3);
      }

      sim7080gClient.setServer(portASerial, url, port);
      sim7080gClient.setCleanness(portASerial, true);
      sim7080gClient.setKeeptime(portASerial, 1200);
      sim7080gClient.connect(portASerial, clientId);

      if (sim7080gClient.connected(portASerial)) {
        gsmReady = true;
        //portASerial.end(); //TODO: あとで消す
      } else {
        gsmReady = false;
        portASerial.end();
        goto finish_gsm_setup;
      }
    }
  }
  ESP_LOGD(TAG, "gsm setup done");

  ESP_LOGD(TAG, "gsm gps test");
  sim7080gClient.gpsPowerOn(portASerial);
  sim7080gClient.updateLatLng(portASerial);

finish_gsm_setup:

  // Setup PortA
  if (gsmPort != gsmPortA) {
    ESP_LOGD(TAG, "portA %d\n", portA.type);
    if (portA.type == gpsUnit) {
      // ポートをスキャンし、GPSユニットが接続されているか確認する
      portASerial.begin(9600, SERIAL_8N1, 33, 32, false);
      delay(500);
      if (portASerial.available() == 0) {
        // GPSが接続されていない
        ESP_LOGE(TAG, "Couldn't find GPS\n");
        portA.ready = false;
        portASerial.end();
      } else {
        portA.ready = true;
      }
    } else if (portA.type == env4Unit) {
      if ((!portA.sht.begin(&Wire, SHT40_I2C_ADDR_44, 32, 33, 400000U)) || (!portA.bmp.begin(&Wire, BMP280_I2C_ADDR, 32, 33, 400000U))) {
        ESP_LOGE(TAG, "Couldn't find Env4\n");
        portA.ready = false;
      } else {
        portA.bmp.setSampling(BMP280::MODE_NORMAL, BMP280::SAMPLING_X2, BMP280::SAMPLING_X16, BMP280::FILTER_X16, BMP280::STANDBY_MS_500);
        portA.ready = true;
      }
    }
  }

  int battLevel = getBatLevel();
  sprintf(systemBarText, systemBarFormat, ((portA.type == gpsUnit) && portA.ready) ? LV_SYMBOL_GPS : " ", gsmReady ? LV_SYMBOL_CALL : " " , WiFi.isConnected() ? LV_SYMBOL_WIFI : " ", battLevel);

  // Setup bluetooth
  NimBLEDevice::init("");
  bleScan = NimBLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(&callbacks);

  queue = xQueueCreate(10, sizeof(Beacon));

  if (queue) {
    if (timerInterval > 0) {
      timer = timerBegin(0, 80, true);
      timerAttachInterrupt(timer, &onTimer, true);
      timerAlarmWrite(timer, timerInterval, true);
      timerAlarmEnable(timer);
    }
  }

  // Setup LVGL
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
  lv_label_set_text(systemBar, systemBarText);
  lv_obj_align(systemBar, LV_ALIGN_TOP_RIGHT, 0, 0);

  connectionStatus = lv_label_create(homeTabContainer);
  lv_label_set_text(connectionStatus, booting);
  lv_obj_set_pos(connectionStatus, 0, 0);

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

  // WiFi/MQTT/証明書タブ
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
        ESP_LOGD(TAG, "ssidDropdown\n");
        ssids = "";
        lv_dropdown_clear_options(ssidDropdown);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        int networks = WiFi.scanNetworks();
        for (int i = 0; i < networks; i++) {
          ESP_LOGD(TAG, "%s\n", WiFi.SSID(i) + " " + WiFi.channel(i) + " " + WiFi.RSSI(i));
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
        wifiReady = false;

        lv_dropdown_get_selected_str(ssidDropdown, ssid, 33);
        sprintf(pass, "%s\0", lv_textarea_get_text(passwordTextarea), 65);
        ESP_LOGD(TAG, "ssid : %s  pass : %s\n", ssid, pass);

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
    
  lv_obj_t *gsmLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(gsmLabel, gsmText);
  lv_obj_set_pos(gsmLabel, 0, 200);

  lv_obj_t *gsmPortLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(gsmPortLabel, portText);
  lv_obj_set_pos(gsmPortLabel, 0, 250);

  static lv_obj_t *gsmPortDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(gsmPortDropdown, "None\nPortA");
  lv_dropdown_set_selected(gsmPortDropdown, gsmPort);
  lv_obj_set_pos(gsmPortDropdown, 50, 240);
  
  lv_obj_t *apnLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(apnLabel, apnText);
  lv_obj_set_pos(apnLabel, 0, 300);

  static lv_obj_t *apnTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(apnTextarea, apn);
  lv_textarea_set_one_line(apnTextarea, true);
  lv_textarea_set_max_length(apnTextarea, 63);
  lv_obj_set_width(apnTextarea, 140);
  lv_obj_set_pos(apnTextarea, 50, 290);
  lv_obj_add_event_cb(apnTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *apnUserLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(apnUserLabel, userText);
  lv_obj_set_pos(apnUserLabel, 0, 350);

  static lv_obj_t *apnUserTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(apnUserTextarea, apnUser);
  lv_textarea_set_one_line(apnUserTextarea, true);
  lv_textarea_set_max_length(apnUserTextarea, 63);
  lv_obj_set_width(apnUserTextarea, 140);
  lv_obj_set_pos(apnUserTextarea, 50, 340);
  lv_obj_add_event_cb(apnUserTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *apnPassLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(apnPassLabel, passText);
  lv_obj_set_pos(apnPassLabel, 0, 400);

  static lv_obj_t *apnPassTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(apnPassTextarea, apnPass);
  lv_textarea_set_one_line(apnPassTextarea, true);
  lv_textarea_set_max_length(apnPassTextarea, 63);
  lv_obj_set_width(apnPassTextarea, 140);
  lv_obj_set_pos(apnPassTextarea, 50, 390);
  lv_obj_add_event_cb(apnPassTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *gsmSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *gsmSaveButtonLabel = lv_label_create(gsmSaveButton);
  lv_label_set_text(gsmSaveButtonLabel, saveText);
  lv_obj_set_pos(gsmSaveButton, 50, 450);
  lv_obj_add_event_cb(
    gsmSaveButton,
    [](lv_event_t *event) {
      gsmPort = lv_dropdown_get_selected(gsmPortDropdown);
      sprintf(apn, "%s", lv_textarea_get_text(apnTextarea));
      sprintf(apnUser, "%s", lv_textarea_get_text(apnUserTextarea));
      sprintf(apnPass, "%s", lv_textarea_get_text(apnPassTextarea));
      ESP_LOGD(TAG, "apn : %s, user : %s, pass : %s", apn, apnUser, apnPass);
      static const char *buttons[] = {okText, cancelText, ""};
      messageBox = lv_msgbox_create(NULL, saveText, "GSM settings", buttons, true);
      lv_obj_center(messageBox);
      lv_obj_add_event_cb(
        messageBox,
        [](lv_event_t *event) {
          lv_obj_t *obj = lv_event_get_current_target(event);
          const char *buttonText = lv_msgbox_get_active_btn_text(obj);
          if (strcmp(buttonText, okText) == 0) {
            preferences.begin("m5core2_app", false);
            preferences.putInt(gsmPortKey, gsmPort);
            preferences.putString(apnKey, apn);
            preferences.putString(apnUserKey, apnUser);
            preferences.putString(apnPassKey, apnPass);
            preferences.end();
          }
          lv_msgbox_close(messageBox);
        },
        LV_EVENT_VALUE_CHANGED, NULL);
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *mqttLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(mqttLabel, mqttText);
  lv_obj_set_pos(mqttLabel, 0, 500);

  lv_obj_t *urlLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(urlLabel, urlText);
  lv_obj_set_pos(urlLabel, 0, 550);

  static lv_obj_t *urlTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(urlTextarea, url);
  lv_textarea_set_one_line(urlTextarea, true);
  lv_textarea_set_max_length(urlTextarea, 63);
  lv_obj_set_width(urlTextarea, 140);
  lv_obj_set_pos(urlTextarea, 50, 540);
  lv_obj_add_event_cb(urlTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *portLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(portLabel, portText);
  lv_obj_set_pos(portLabel, 0, 600);

  static lv_obj_t *portTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(portTextarea, String(port).c_str());
  lv_textarea_set_one_line(portTextarea, true);
  lv_textarea_set_accepted_chars(portTextarea, "0123456789");
  lv_textarea_set_max_length(portTextarea, 4);
  lv_obj_set_width(portTextarea, 140);
  lv_obj_set_pos(portTextarea, 50, 590);
  lv_obj_add_event_cb(portTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *tlsLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(tlsLabel, tlsText);
  lv_obj_set_pos(tlsLabel, 0, 650);

  static lv_obj_t *tlsSwitch = lv_switch_create(connectionTabContainer);
  lv_obj_set_pos(tlsSwitch, 50, 640);
  if (tls) {
    lv_obj_add_state(tlsSwitch, tls);
  } else {
    lv_obj_clear_state(tlsSwitch, tls);
  }

  lv_obj_t *clientIdLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(clientIdLabel, clientIdText);
  lv_obj_set_pos(clientIdLabel, 0, 700);

  static lv_obj_t *clientIdTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(clientIdTextarea, clientId);
  lv_textarea_set_one_line(clientIdTextarea, true);
  lv_textarea_set_max_length(clientIdTextarea, 31);
  lv_obj_set_width(clientIdTextarea, 140);
  lv_obj_set_pos(clientIdTextarea, 50, 690);
  lv_obj_add_event_cb(clientIdTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *topicLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(topicLabel, topicText);
  lv_obj_set_pos(topicLabel, 0, 750);

  static lv_obj_t *topicTextarea = lv_textarea_create(connectionTabContainer);
  lv_textarea_set_text(topicTextarea, topic);
  lv_textarea_set_one_line(topicTextarea, true);
  lv_obj_set_width(topicTextarea, 140);
  lv_obj_set_pos(topicTextarea, 50, 740);
  lv_obj_add_event_cb(topicTextarea, textarea_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *mqttSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *mqttSaveButtonLabel = lv_label_create(mqttSaveButton);
  lv_label_set_text(mqttSaveButtonLabel, saveText);
  lv_obj_set_pos(mqttSaveButton, 50, 800);
  lv_obj_add_event_cb(
      mqttSaveButton,
      [](lv_event_t *event) {
        ESP_LOGD(TAG, "mqttSaveButton\n");
        wifiReady = false;

        sprintf(url, "%s", lv_textarea_get_text(urlTextarea));
        port = atoi(lv_textarea_get_text(portTextarea));
        tls = lv_obj_get_state(tlsSwitch);
        sprintf(clientId, "%s", lv_textarea_get_text(clientIdTextarea));
        sprintf(topic, "%s", lv_textarea_get_text(topicTextarea));

        ESP_LOGD(TAG, "%s, %d, %s\n", url, port, topic);

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
                preferences.putBool(tlsKey, tls);
                preferences.putString(clientIdKey, clientId);
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
  lv_obj_set_pos(certLabel, 0, 850);

  lv_obj_t *rootCaLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(rootCaLabel, rootCAText);
  lv_obj_set_pos(rootCaLabel, 0, 900);

  static lv_obj_t *rootCaDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(rootCaDropdown, files.c_str());
  lv_obj_set_width(rootCaDropdown, 140);
  lv_obj_set_pos(rootCaDropdown, 50, 890);

  lv_obj_t *clientCertLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(clientCertLabel, certText);
  lv_obj_set_pos(clientCertLabel, 0, 950);

  static lv_obj_t *clientCertDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(clientCertDropdown, files.c_str());
  lv_obj_set_width(clientCertDropdown, 140);
  lv_obj_set_pos(clientCertDropdown, 50, 940);

  lv_obj_t *keyLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(keyLabel, keyText);
  lv_obj_set_pos(keyLabel, 0, 1000);

  static lv_obj_t *keyDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(keyDropdown, files.c_str());
  lv_obj_set_width(keyDropdown, 140);
  lv_obj_set_pos(keyDropdown, 50, 990);

  lv_obj_t *certSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *certSaveButtonLabel = lv_label_create(certSaveButton);
  lv_label_set_text(certSaveButtonLabel, saveText);
  lv_obj_set_pos(certSaveButton, 50, 1050);
  lv_obj_add_event_cb(
      certSaveButton,
      [](lv_event_t *event) {
        wifiReady = false;

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
  lv_obj_set_pos(ntpLabel, 0, 1100);

  lv_obj_t *ntpServerLabel = lv_label_create(connectionTabContainer);
  lv_label_set_text(ntpServerLabel, urlText);
  lv_obj_set_pos(ntpServerLabel, 0, 1150);

  lv_obj_t *ntpDropdown = lv_dropdown_create(connectionTabContainer);
  lv_dropdown_set_options(ntpDropdown, nictNTP);
  lv_obj_set_width(ntpDropdown, 140);
  lv_obj_set_pos(ntpDropdown, 50, 1140);

  lv_obj_t *ntpSaveButton = lv_btn_create(connectionTabContainer);
  lv_obj_t *ntpSaveButtonLabel = lv_label_create(ntpSaveButton);
  lv_label_set_text(ntpSaveButtonLabel, saveText);
  lv_obj_set_pos(ntpSaveButton, 50, 1200);
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

  lv_obj_t *bluetoothLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(bluetoothLabel, bluetoothText);
  lv_obj_set_pos(bluetoothLabel, 0, 0);

  lv_obj_t *scanEnableLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(scanEnableLabel, scanText);
  lv_obj_set_pos(scanEnableLabel, 0, 50);

  static lv_obj_t *scanEnableSwitch = lv_switch_create(bluetoothTabContainer);
  lv_obj_set_pos(scanEnableSwitch, 50, 40);
  if (scanEnable) {
    lv_obj_add_state(scanEnableSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(scanEnableSwitch, LV_STATE_CHECKED);
  }

  lv_obj_t *activeScanEnableLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(activeScanEnableLabel, activeScanText);
  lv_obj_set_pos(activeScanEnableLabel, 0, 100);

  static lv_obj_t *activeScanSwitch = lv_switch_create(bluetoothTabContainer);
  if (activeScan) {
    lv_obj_add_state(activeScanSwitch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(activeScanSwitch, LV_STATE_CHECKED);
  }
  lv_obj_set_pos(activeScanSwitch, 50, 90);

  lv_obj_t *rssiLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text(rssiLabel, rssiText);
  lv_obj_set_pos(rssiLabel, 0, 150);

  static lv_obj_t *rssiThresholdLabel = lv_label_create(bluetoothTabContainer);
  lv_label_set_text_fmt(rssiThresholdLabel, "%d", rssiThreshold);
  lv_obj_set_pos(rssiThresholdLabel, 210, 150);

  static lv_obj_t *rssiSlider = lv_slider_create(bluetoothTabContainer);
  lv_obj_set_width(rssiSlider, 130);
  lv_slider_set_range(rssiSlider, -120, 0);
  lv_slider_set_value(rssiSlider, rssiThreshold, LV_ANIM_OFF);
  lv_obj_set_pos(rssiSlider, 60, 150);
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
  lv_obj_set_pos(bluetoothSaveButton, 50, 200);
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
                preferences.putBool(scanEnableKey, lv_obj_get_state(scanEnableSwitch));
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
  lv_obj_t *sensorsTab = lv_tabview_add_tab(tabView, LV_SYMBOL_PLUS);
  lv_obj_t *sensorsTabContainer = lv_obj_create(sensorsTab);
  lv_gridnav_add(sensorsTabContainer, LV_GRIDNAV_CTRL_NONE);
  lv_obj_set_size(sensorsTabContainer, lv_pct(100), lv_pct(450));

  lv_obj_t *sensorsLabel = lv_label_create(sensorsTabContainer);
  lv_label_set_text(sensorsLabel, sensorsText);
  lv_obj_set_pos(sensorsLabel, 0, 0);

  lv_obj_t *timerEnableLabel = lv_label_create(sensorsTabContainer);
  lv_label_set_text(timerEnableLabel, timerText);
  lv_obj_set_pos(timerEnableLabel, 0, 50);

  static lv_obj_t* timerIntervalDropdown = lv_dropdown_create(sensorsTabContainer);
  lv_dropdown_set_options(timerIntervalDropdown, timerIntervals);
  lv_obj_set_width(timerIntervalDropdown, 140);
  lv_obj_set_pos(timerIntervalDropdown, 50, 40);
  if (timerInterval == timerIntervalNone) {
    lv_dropdown_set_selected(timerIntervalDropdown, 0);
  } else if (timerInterval == timerInterval10min) {
    lv_dropdown_set_selected(timerIntervalDropdown, 1);
  } else if (timerInterval == timerInterval30min) {
    lv_dropdown_set_selected(timerIntervalDropdown, 2);
  } else if (timerInterval == timerInterval60min) {
    lv_dropdown_set_selected(timerIntervalDropdown, 3);
  }

  lv_obj_t *portALabel = lv_label_create(sensorsTabContainer);
  lv_label_set_text(portALabel, portAText);
  lv_obj_set_pos(portALabel, 0, 100);

  static lv_obj_t *portADropdown = lv_dropdown_create(sensorsTabContainer);
  lv_dropdown_set_options(portADropdown, supportedUnits);
  lv_obj_set_width(portADropdown, 140);
  lv_dropdown_set_selected(portADropdown, portA.type);
  lv_obj_set_pos(portADropdown, 50, 90);
  if (gsmPort == gsmPortA) {
    lv_dropdown_set_selected(portADropdown, none);
    lv_obj_add_state(portADropdown, LV_STATE_DISABLED);
  }

  lv_obj_t *sensorsSaveButton = lv_btn_create(sensorsTabContainer);
  lv_obj_t *sensorsSaveButtonLabel = lv_label_create(sensorsSaveButton);
  lv_label_set_text(sensorsSaveButtonLabel, saveText);
  lv_obj_set_pos(sensorsSaveButton, 50, 150);
  lv_obj_add_event_cb(
      sensorsSaveButton,
      [](lv_event_t *event) {
        int selected = lv_dropdown_get_selected(timerIntervalDropdown);
        if (selected == 0) {
          timerInterval = timerIntervalNone;
        } else if (selected == 1) {
          timerInterval = timerInterval10min;
        } else if (selected == 2) {
          timerInterval = timerInterval30min;
        } else if (selected == 3) {
          timerInterval = timerInterval60min;
        }

        static const char *buttons[] = {okText, cancelText, ""};
        messageBox = lv_msgbox_create(NULL, saveText, "Sensors settings", buttons, true);
        lv_obj_center(messageBox);
        lv_obj_add_event_cb(
            messageBox,
            [](lv_event_t *event) {
              lv_obj_t *obj = lv_event_get_current_target(event);
              const char *buttonText = lv_msgbox_get_active_btn_text(obj);
              int portA = lv_dropdown_get_selected(portADropdown);
              if (strcmp(buttonText, okText) == 0) {
                preferences.begin("m5core2_app", false);
                preferences.putInt(timerIntervalKey, timerInterval);
                preferences.putInt(portAKey, portA);
                preferences.end();
              }
              lv_msgbox_close(messageBox);
            },
            LV_EVENT_VALUE_CHANGED, NULL);
      },
      LV_EVENT_CLICKED, NULL);

  ESP_LOGD(TAG, "Setup done\n");
}

void loop() {
  M5.update();
  lv_tick_inc(5);
  lv_task_handler();

  if (wifiReady || gsmReady) {
    if (WiFi.isConnected() || gsmReady) {
      if (mqttClient.connected() || gsmReady) {
        if (queue != NULL) {
          while (uxQueueMessagesWaiting(queue)) {
            struct Beacon beacon;
            if (xQueueReceive(queue, &beacon, portMAX_DELAY) == pdPASS) {
              messageJson["source"] = beacon.source;
              messageJson["gateway"] = macAddress;
              messageJson["address"] = beacon.address;
              messageJson["payload"] = beacon.payload;
              messageJson["rssi"] = beacon.rssi;

              if (gsmPort != gsmPortA) {
                if (portA.ready) {
                  JsonObject portAJson = messageJson["porta"].to<JsonObject>();

                  if (portA.type == gpsUnit) {
                    JsonObject gpsJson = portAJson["gps"].to<JsonObject>();
                    gpsJson["latitude"] = portA.gps.location.lat();
                    gpsJson["longitude"] = portA.gps.location.lng();
                    gpsJson["fixed_time"] = portA.gps.time.value();
                  }

                  if (portA.type == env4Unit) {
                    JsonObject env4Json = portAJson["env4"].to<JsonObject>();
                    env4Json["temperature"] = portA.sht.cTemp;
                    env4Json["humidity"] = portA.sht.humidity;
                    env4Json["airpressuer"] = portA.bmp.pressure;
                  }
                }
              }

              messageJson["battery"] = getBatLevel();
              messageJson["timestamp"] = beacon.timestamp;

              int messageLength = serializeJson(messageJson, message);
              ESP_LOGD(TAG, "%s\n", message);
              if (gsmReady) {
                sim7080gClient.updateLatLng(portASerial);
                sim7080gClient.publish(portASerial, topic, message, messageLength, 0, 0);
              } else if (mqttClient.connected()) {
                mqttClient.publish(topic, message);
              }
              delay(1);
            }
          }
        }

        mqttClient.loop();

        if (scanEnable && !bleScan->isScanning()) {
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
        if (!mqttClient.connected()) {
          mqttClient.disconnect();
          mqttClient.setServer(url, port);
          mqttClient.setCallback(mqttCallback);
          if (mqttClient.connect(clientId)) {
            mqttClient.subscribe(notificationTopic);
          } else {
            if (retry++ > 100) {
              ESP_LOGE(TAG,"Reboot\n");
              ESP.restart();
            }
          }
          delay(500);
        }
      }
    } else {
      if (!WiFi.isConnected()) {
        WiFi.begin(ssid, pass);
        WiFi.waitForConnectResult();
      }
    }

    if (gsmPort != gsmPortA) {
      if ((portA.type == gpsUnit) && portA.ready) {
        while (portASerial.available() > 0) {
          int ch = portASerial.read();
          if (portA.gps.encode(ch)) {
            if (portA.gps.location.isValid() && portA.gps.location.isUpdated()) {
              break;
            }
          }
        }
        delay(1);
      }

      if ((portA.type == env4Unit) && portA.ready) {
        portA.sht.update();
        portA.bmp.update();
        delay(1);
      }
    }
  }

  updateSystemBar();
  updateStatus();
  updateDashboard();

  delay(1);
}
