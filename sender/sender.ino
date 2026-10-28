#include <DHT.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32_NOW.h>
#include "my_mac_addresses.h"

#define DEBUG
#ifdef DEBUG
   #define DEBUG_BEGIN(x) Serial.begin(x)
   #define DEBUG(x)       Serial.print(x)
   #define DEBUGLN(x)     Serial.println(x)
#else
   #define DEBUG_BEGIN(x)
   #define DEBUG(x)
   #define DEBUGLN(x)
#endif

#define NODE_ID 1
#define DHT_DATA_PIN 2
#define DHT_POWER_PIN 4
#define DHTTYPE DHT22
#define DHT_WAIT 2000
#define DHT_READ_RETRY 40

#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_WIFI_MODE WIFI_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA

// Sleep duration: 1 hour in microseconds (1 hour * 60 min * 60 sec * 1,000,000 us)
#define SLEEP_DURATION_US  (1ULL * 60ULL * 60ULL * 1000000ULL)
//#define SLEEP_DURATION_US  (60ULL * 1000000ULL)

struct SensorData {
  uint8_t nodeID;
  float temperature;
  float humidity;
  float batteryVoltage;
  uint8_t readRetries;
};

const uint8_t peer_mac[] = MAC_ADDRESS_RECEIVER_NODE;

class ReceiverPeer : public ESP_NOW_Peer {
public:
  ReceiverPeer(const uint8_t* mac, uint8_t channel, wifi_interface_t interface)
  : ESP_NOW_Peer(mac, channel, interface)
  {    
  }

  ~ReceiverPeer() {
    remove();
  } 

  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      DEBUGLN("Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    return true;
  }

  bool send_message(const uint8_t* data, size_t len)
  {
    if (!send(data, len)) {
      DEBUGLN("failed to send data.");
      return false;
    }
    DEBUGLN("send_message succeeded.");
    return true;
  }

  void onSent(bool success) override {
    if (success) {
      Serial.print("transmission success!");
    } else {
      Serial.print("transmission failed!");
    }
  }
};

ReceiverPeer receiverPeer(peer_mac, ESPNOW_WIFI_CHANNEL, ESPNOW_WIFI_IF);

DHT dht(DHT_DATA_PIN, DHTTYPE);

void setup() {
  // Configure and immediately power up the DHT22 sensor via GPIO4
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);

  DEBUG_BEGIN(115200);
  DEBUGLN("Starting DHT22");
  dht.begin();

  // CRITICAL: Wait ~2 seconds for the DHT22 to boot up and stabilize
  delay(DHT_WAIT);
}

void loop() {
  SensorData data;
  data.nodeID = NODE_ID;
  bool readFailed = true;
  for (int i = 0; i < DHT_READ_RETRY; ++i) {
    data.temperature = dht.readTemperature();
    data.humidity = dht.readHumidity();
    if (!isnan(data.temperature) && !isnan(data.humidity)) {
      data.readRetries = i;
      readFailed = false;
      break;
    }
    delay(DHT_WAIT);
  }
  if (readFailed) {
    data.readRetries = DHT_READ_RETRY;
  }

  DEBUG(" Humidity: ");
  if (isnan(data.humidity)) {
    DEBUG("Nan");
  } else {
    DEBUG(data.humidity);
  }
  DEBUG("% | Temperature: ");
  if (isnan(data.temperature)) {
    DEBUG("Nan");
  } else {
    DEBUG(data.temperature);
  }
  DEBUG("deg C, retries "); DEBUGLN(data.readRetries);

  // WiFi Start
  DEBUGLN("Setting up WiFi...");
  WiFi.mode(ESPNOW_WIFI_MODE);
  esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_LR);
  WiFi.disconnect();
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  int channel = WiFi.channel();
  DEBUG("connected to WiFi channel "); DEBUGLN(channel);
  uint8_t primary_channel;
  wifi_second_chan_t second_channel;
  esp_wifi_get_channel(&primary_channel, &second_channel);
  DEBUG("The radio is ACTUALLY on channel: ");
  DEBUGLN(primary_channel);

  while (!WiFi.STA.started()) {
    delay(100);
  }
  DEBUGLN("WiFi station mode started");
  DEBUG("MAC Address: "); DEBUGLN(WiFi.macAddress());

  DEBUGLN("ESP-NOW communication starting...");
  if (!receiverPeer.begin()) {
    DEBUGLN("Failed to initialize ESP-NOW, rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
  DEBUGLN("ESP-NOW started.");

  bool sendStatus = receiverPeer.send_message((uint8_t *)&data, sizeof(data));
  DEBUGLN("data sent to the ESP-NOW peer.");
  DEBUGLN(sendStatus);

  // Cut the power to DHT22 and isolate the pin by setting it input without pull-up.
  DEBUGLN("cutting power to DHT22");
  digitalWrite(DHT_POWER_PIN, LOW);
  pinMode(DHT_POWER_PIN, INPUT);

  delay(300);

  // Deep sleep
  DEBUG("entering deep sleep for "); DEBUG(SLEEP_DURATION_US); DEBUGLN(" usec.");
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
  esp_deep_sleep_start();
}
