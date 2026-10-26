
/*
 * ==============================================================================
 * PRAKTIKUM 7: SIMPUL SENSOR-AKTUATOR IOT UNTUK BROKER LOKAL
 * MQTT AUTHENTICATION & ACCESS CONTROL
 * ==============================================================================
 * Membaca sensor DHT11/DHT22, mengirim telemetri JSON ke broker Mosquitto,
 * serta menerima instruksi kendali LED melalui Node-RED.
 * Broker menggunakan autentikasi username dan password.
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. KREDENSIAL WI-FI
const char* ssid     = "gengster bikin gemeter";
const char* password = "rahel1709";

// 2. KONFIGURASI BROKER MQTT
const char* mqttServer = "192.168.56.1";
const int mqttPort = 1883;

// Kredensial autentikasi MQTT
const char* mqttUser = "admin";
const char* mqttPassword = "rahasia123";

// 3. IDENTITAS NODE DAN TOPIK MQTT
const char* clientId = "ESP8266_Local_Node01";
const char* topicPubTele = "kampus/labiot/telemetri";
const char* topicSubCmd = "kampus/labiot/kendali";

// 4. KONFIGURASI PERANGKAT KERAS
const byte dhtPin = 2;  // D4 / GPIO2
const byte ledPin = 5;  // D1 / GPIO5

#define DHTTYPE DHT11   // Ganti DHT22 jika menggunakan sensor DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000;

// ============================================================
// CALLBACK PESAN MASUK DARI NODE-RED
// ============================================================
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];

  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]         : %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];

    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);

    Serial.printf(
      "[AKTUATOR] Status LED: %s\n",
      stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)"
    );
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan!");
  }

  Serial.println("----------------------------------------");
}

// ============================================================
// KONEKSI WI-FI
// ============================================================
void connectToWiFi() {
  Serial.println();
  Serial.print("[Wi-Fi] Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Berhasil terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
// KONEKSI MQTT DENGAN AUTENTIKASI
// ============================================================
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan ke Broker Lokal (");
    Serial.print(mqttServer);
    Serial.print(")...");

    // Login menggunakan username dan password MQTT
    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println(" Terhubung dengan Autentikasi Sah!");

      if (client.subscribe(topicSubCmd)) {
        Serial.println("[MQTT] Berhasil subscribe topik kendali!");
      } else {
        Serial.println("[MQTT] Gagal subscribe topik kendali!");
      }

    } else {
      Serial.printf(
        " Gagal (rc=%d). Coba lagi dalam 4 detik...\n",
        client.state()
      );

      delay(4000);
    }
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

// ============================================================
// LOOP UTAMA
// ============================================================
void loop() {
  // Memulihkan koneksi Wi-Fi jika terputus
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // Memulihkan koneksi MQTT jika terputus
  if (!client.connected()) {
    reconnectMQTT();
  }

  // Memproses pesan MQTT masuk
  client.loop();

  // Mengirim telemetri setiap 4 detik
  unsigned long now = millis();

  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;

      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      bool success = client.publish(topicPubTele, teleBuffer);

      if (success) {
        Serial.printf(
          "[TELEMETRI TERKIRIM] %s\n",
          teleBuffer
        );
      } else {
        Serial.println(
          "[ERROR] Gagal mengirim telemetri ke broker MQTT!"
        );
      }

    } else {
      Serial.println(
        "[ERROR] Gagal membaca sensor DHT! Periksa sensor dan kabel."
      );
    }
  }
}
