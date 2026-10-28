#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

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

#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_WIFI_MODE WIFI_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA

struct SensorData {
  uint8_t nodeID;
  float temperature;
  float humidity;
  float batteryVoltage;
  uint8_t readRetries;
};

void on_data_recv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incoming_data, int data_len) {
  if (data_len != sizeof(SensorData)) {
    log_e("MSG: Received packet of unexpected size!");
    return;
  }

  SensorData data;
  memcpy(&data, incoming_data, sizeof(SensorData));

  // Print formatted output to Serial for the Raspberry Pi
  // Format: DATA,NODE_ID,TEMP,HUMIDITY,BATTERY
  Serial.print("DATA: {\"nodeID\":");
  Serial.print(data.nodeID);
  Serial.print(", \"temperature\":");
  Serial.print(data.temperature);
  Serial.print(", \"humidity\":");
  Serial.print(data.humidity);
  Serial.print(", \"readRetries\":");
  Serial.print(data.readRetries);
  Serial.println("}");
}

void setup() {
  delay(10000);
  Serial.begin(115200);
  Serial.println("MSG: Setting up WiFi...");
  WiFi.mode(ESPNOW_WIFI_MODE);
  esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_LR);
  WiFi.disconnect();
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  int wifiChannel = WiFi.channel();
  Serial.print("MSG: Connected to Wi-Fi on Channel: ");
  Serial.println(wifiChannel);

  uint8_t primary_channel;
  wifi_second_chan_t second_channel;
  esp_wifi_get_channel(&primary_channel, &second_channel);
  Serial.print("The radio is ACTUALLY on channel: ");
  Serial.println(primary_channel);


  while (!WiFi.STA.started()) {
    delay(100);
  }
  Serial.println("MSG: WiFi station mode started");
  Serial.print("MSG: MAC Address: ");
  Serial.println(WiFi.macAddress());

  Serial.println("MSG: ESP-NOW communication starting...");
  if (esp_now_init() != ESP_OK) {
    Serial.println("MSG: Failed to initialize ESP-NOW");
    Serial.println("MSG: Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
  uint32_t esp_version;
  esp_now_get_version(&esp_version);
  Serial.printf("MSG: ESP-NOW version: %d\n", esp_version);

  if (esp_now_register_recv_cb(on_data_recv) != ESP_OK) {
    log_e("MSG: receive callback registration failed.");
    return;
  }
  Serial.print("MSG: Receive callback registered");
}

void loop() {
}
