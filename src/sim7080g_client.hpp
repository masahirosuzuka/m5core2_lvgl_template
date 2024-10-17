#include <string.h>
#include <M5Core2.h>

//static const char * TAG;
#define BUFFER_SIZE 64

typedef void (*Callback)(const char *, const char *);

class SIM7080GClient {
private:
  const char * TAG = "sim7080g_client";
  char command[BUFFER_SIZE];
  char response[BUFFER_SIZE];
  int result = 0;
  int pdpContext = 1;
  char * subscribeTopic;
  Callback subscribeCallback;
  int sendATCommand(Stream& serial, const char * message, char * response, int responseSize, int wait);
  int sendFile(Stream& serial, const char * file, char * response, int responseSize, int wait);
public:
  bool deviceConnected(Stream& serial);
  bool SIMReady(Stream& serial);
  void connectAPN(Stream& serial, const char * apn, const char * user, const char * pass);
  bool isOnline(Stream& serial);
  void setServer(Stream& serial, const char * url, const int port);
  void setKeeptime(Stream& serial, const int keeptime);
  void setCleanness(Stream& serial, bool cleanness);
  void setCaCert(Stream& serial, const char *fileName, const char * caCert, const int caCertSize);
  void setCert(Stream& serial, const char *fileName, const char * cert, const int certSize);
  void setKey(Stream& serial, const char *fileName, const char * key, const int keySize);
  void useTLS(Stream& serial, const char * caCertName, const char * certName, const char * keyName);
  void setSSLVersion(Stream& serial, int version);
  void connect(Stream& serial, const char * clientId);
  bool connected(Stream& serial);
  void publish(Stream& serial, const char * topic, const char * message, const int messageLength, int qos, int retain);
  void subscribe(Stream& serial, char * topic, Callback onMessage);
  void mqttLoop(Stream& serial);
  void disconnect(Stream& serial);
};

int SIM7080GClient::sendATCommand(Stream& serial, const char * message, char * response, int responseSize, int wait) {
  serial.println(message);
  delay(wait);
  int index = 0;
  while (serial.available() && index < responseSize) {
    response[index++] = (char)serial.read();
  }
  response[index] = '\0';

  return index;
};

int SIM7080GClient::sendFile(Stream& serial, const char * file, char * response, int responseSize, int wait) {
  serial.write(file);
  delay(wait);
  int index = 0;
  while (serial.available() && index < responseSize) {
    response[index++] = (char)serial.read();
  }
  response[index] = '\0';

  return index;
};

bool SIM7080GClient::deviceConnected(Stream& serial) {
  sprintf(command, "ATE0");
  int result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  if (strstr(response, "OK")) {
    return true;
  }

  return false;
}

bool SIM7080GClient::SIMReady(Stream& serial) {
  sprintf(command, "AT+CPIN?");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  if (strstr(response, "+CPIN: READY")) {
    return true;
  }

  return false;
}

void SIM7080GClient::connectAPN(Stream& serial, const char * apn, const char * user, const char * pass) {
  sprintf(command, "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 5000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CGAUTH=1,3,\"%s\",\"%s\"", pass, user);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 30000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // DOCOMOに接続試す
  sprintf(command, "AT+COPS=1,2,\"44010\"");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 30000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  /*
  // KDDIに接続試す
  sprintf(command, "AT+COPS=1,2,\"44051\"");
  result = sendATCommand(portASerial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // SoftBankに接続試す
  sprintf(command, "AT+COPS=1,2,\"44051\"");
  result = sendATCommand(portASerial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
   */

  //sprintf(command, "AT+COPS=?");
  //result = sendATCommand(serial, command, response, BUFFER_SIZE, 10000);
  //ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // SIMカードチェック
  //sprintf(command, "AT+CPIN?");
  //result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  //ESP_LOGD(TAG, "result : %d response : %s", result, response);
      
  // LTE onlyに設定
  sprintf(command, "AT+CNMP=38");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // Cat-M1 onlyに設定
  sprintf(command, "AT+CMNB=1");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // APN接続確認
  //sprintf(command, "AT+CGNAPN");
  //result = sendATCommand(serial, command, response, BUFFER_SIZE, 10000);
  //ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // Application Networkを起動
  sprintf(command, "AT+CNACT=0,1");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

bool SIM7080GClient::isOnline(Stream& serial) {
  sprintf(command, "AT+CNACT?");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  //if (strstr(response, "Online")) {
  if (strstr(response, "+CNACT: 0,1")) {
    return true;
  }

  return false;
}

void SIM7080GClient::setServer(Stream& serial, const char * url, const int port) {
  sprintf(command, "AT+SMCONF=\"URL\",%s,%d", url, port);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setKeeptime(Stream& serial, const int keeptime) {
  sprintf(command, "AT+SMCONF=\"KEEPTIME\",%d", keeptime);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setCleanness(Stream& serial, bool cleanness) {
  sprintf(command, "AT+SMCONF=\"CLEANSS\",%d", cleanness ? 1 : 0);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setCaCert(Stream& serial, const char *fileName, const char * caCert, const int caCertSize) {
  sprintf(command, "AT+CFSINIT");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSWFILE=3,\"%s\",0,%d,1000", fileName, caCertSize);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  result = sendFile(serial, caCert, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSTERM");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setCert(Stream& serial, const char *fileName, const char * cert, const int certSize) {
  sprintf(command, "AT+CFSINIT");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSWFILE=3,\"%s\",0,%d,1000", fileName, certSize);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  result = sendFile(serial, cert, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSTERM");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setKey(Stream& serial, const char *fileName, const char * key, const int keySize) {
  sprintf(command, "AT+CFSINIT");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSWFILE=3,\"%s\",0,%d,1000", fileName, keySize);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  result = sendFile(serial, key, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CFSTERM");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::useTLS(Stream& serial, const char * caCertName, const char * certName, const char * keyName) {
  //sprintf(command, "AT+CFSINIT");
  //result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  //ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CSSLCFG=\"CONVERT\",2,\"%s\"", caCertName);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+CSSLCFG=\"CONVERT\",1,\"%s\",\"%s\"", certName, keyName);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+SMSSL=1,\"%s\",\"%s\"", caCertName, certName);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  //sprintf(command, "AT+CFSTERM");
  //result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  //ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::setSSLVersion(Stream& serial, int version) {
  sprintf(command, "AT+CSSLCFG=\"sslversion\",0,%d", version);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

void SIM7080GClient::connect(Stream& serial, const char * clientId) {
  sprintf(command, "AT+SMCONF=\"CLIENTID\",\"%s\"", clientId);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 1000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  sprintf(command, "AT+SMCONN");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 10000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}

bool SIM7080GClient::connected(Stream& serial) {
  sprintf(command, "AT+SMSTATE?");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 10000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  // チェック
  if (strstr(response, "+SMSTATE: 1")) {
    return true;
  }

  return false;
}

void SIM7080GClient::publish(Stream& serial, const char * topic, const char * message, const int messageSize, int qos, int retain) {
  sprintf(command, "AT+SMPUB=\"%s\",%d,%d,%d", topic, messageSize, qos, retain);
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 100);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);

  if (strstr(response, ">")) {
    result = sendFile(serial, message, response, BUFFER_SIZE, 1);
    ESP_LOGD(TAG, "result : %d response : %s", result, response);
  }
}

void SIM7080GClient::subscribe(Stream& serial, char * topic, Callback onMessage) {
  subscribeTopic = topic;
  subscribeCallback = onMessage;
}

void SIM7080GClient::mqttLoop(Stream& serial) {
  int index = 0;
  while (serial.available() && index < BUFFER_SIZE) {
    response[index++] = (char)serial.read();
  }
  response[index] = '\0';

  char * p = strstr(response, "+SMSUB");

  if (p) {
    char topicBuffer[BUFFER_SIZE];
    char bodybuffer[BUFFER_SIZE];

    char *start;
    char *end;

    start = strchr(response, '"');
    if (start != NULL) {
        start++;
        end = strchr(start, '"');
        if (end != NULL) {
            strncpy(topicBuffer, start, end - start);
            topicBuffer[end - start] = '\0';
        }
    }

    start = strchr(end + 1, '"');
    if (start != NULL) {
        start++;
        end = strchr(start, '"');
        if (end != NULL) {
            strncpy(bodybuffer, start, end - start);
            bodybuffer[end - start] = '\0';
        }
    }

    subscribeCallback(topicBuffer, bodybuffer);
  }
}

void SIM7080GClient::disconnect(Stream& serial) {
  sprintf(command, "AT+SMDISC");
  result = sendATCommand(serial, command, response, BUFFER_SIZE, 10000);
  ESP_LOGD(TAG, "result : %d response : %s", result, response);
}
