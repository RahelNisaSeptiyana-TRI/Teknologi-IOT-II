#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ========================================
// 1. KONFIGURASI WI-FI
// ========================================
const char* ssid = "ARDIANSYAH 4G";
const char* password = "Ardiansyah23";

// ========================================
// 2. KONFIGURASI MQTT
// ========================================
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* clientId = "ESP8266_LabIoT_25782050";

// Topik MQTT
const char* topicTelemetry =
    "kampus/labiot/25782050/telemetri";

const char* topicControl =
    "kampus/labiot/25782050/kendali";

const char* topicAlert =
    "kampus/labiot/25782050/alert";

const char* topicStatus =
    "kampus/labiot/25782050/status";

// Pesan Last Will and Testament
const char* willMessage =
    "{\"status\":\"OFFLINE\","
    "\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

// ========================================
// 3. KONFIGURASI SENSOR DAN LED
// ========================================
#define DHTPIN 2
#define DHTTYPE DHT11

const byte ledPin = 5;  // D1 / GPIO 5

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// ========================================
// 4. TIMER NON-BLOCKING
// ========================================
unsigned long lastTelemetryTime = 0;
unsigned long lastReconnectAttempt = 0;

const unsigned long telemetryInterval = 4000;
const unsigned long reconnectInterval = 5000;

// ========================================
// 5. STATUS SISTEM
// ========================================
bool ledManual = false;
bool ledBahaya = false;
bool alertSent = false;

float suhuTerakhir = 0;
float kelembapanTerakhir = 0;

bool sensorValid = false;

// ========================================
// 6. FUNGSI MENGATUR LED
// ========================================
void updateLED() {
  // Kondisi bahaya selalu mengutamakan keselamatan
  bool ledAktif = ledBahaya || ledManual;

  digitalWrite(ledPin, ledAktif ? HIGH : LOW);
}

// ========================================
// 7. CALLBACK PERINTAH MQTT
// ========================================
void callback(char* topic, byte* payload,
              unsigned int length) {
  char message[256];

  if (length >= sizeof(message)) {
    Serial.println("Pesan MQTT terlalu panjang!");
    return;
  }

  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("[PESAN MASUK] ");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(message);

  if (strcmp(topic, topicControl) != 0) {
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.println("JSON perintah tidak valid!");
    return;
  }

  if (!doc.containsKey("led")) {
    Serial.println("Perintah tidak memiliki field led!");
    return;
  }

  int perintahLED = doc["led"];

  if (perintahLED != 0 && perintahLED != 1) {
    Serial.println("Nilai led harus 0 atau 1!");
    return;
  }

  // Kendali manual hanya ketika suhu aman
  if (sensorValid && suhuTerakhir <= 31.0) {
    ledManual = (perintahLED == 1);
    updateLED();

    Serial.println(
      ledManual ? "LED MANUAL MENYALA" :
                  "LED MANUAL PADAM"
    );
  } else {
    Serial.println(
      "Kendali manual ditolak: sensor belum valid "
      "atau suhu berada dalam kondisi bahaya!"
    );
  }
}

// ========================================
// 8. KONEKSI WI-FI
// ========================================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan ke Wi-Fi");

  // Tunggu koneksi awal tanpa delay()
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    yield();
  }

  Serial.println();
  Serial.println("Wi-Fi terhubung!");

  Serial.print("IP lokal: ");
  Serial.println(WiFi.localIP());
}

// ========================================
// 9. KONEKSI ULANG MQTT
// ========================================
void reconnectMQTT() {
  unsigned long now = millis();

  if (client.connected()) {
    return;
  }

  if (now - lastReconnectAttempt < reconnectInterval) {
    return;
  }

  lastReconnectAttempt = now;

  Serial.println("Mencoba menghubungkan MQTT...");

  bool connected = client.connect(
    clientId,
    topicStatus,
    1,              // QoS Last Will
    true,           // Retain Last Will
    willMessage
  );

  if (connected) {
    Serial.println("MQTT terhubung!");

    client.subscribe(topicControl);

    // Publikasi status ONLINE dengan retain
    const char* onlineMessage =
      "{\"status\":\"ONLINE\","
      "\"keterangan\":\"Perangkat Beroperasi Normal\"}";

    client.publish(topicStatus, onlineMessage, true);

    Serial.print("Subscribe: ");
    Serial.println(topicControl);

    Serial.println("Status ONLINE dipublikasikan.");
  } else {
    Serial.print("Koneksi MQTT gagal. Kode: ");
    Serial.println(client.state());
  }
}

// ========================================
// 10. SETUP
// ========================================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  setupWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(512);

  // Percobaan koneksi pertama
  reconnectMQTT();
}

// ========================================
// 11. LOOP UTAMA
// ========================================
void loop() {
  // Memulihkan koneksi Wi-Fi jika terputus
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  reconnectMQTT();

  // Wajib dipanggil sesering mungkin
  client.loop();

  unsigned long now = millis();

  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();

    if (isnan(suhu) || isnan(kelembapan)) {
      sensorValid = false;
      Serial.println("Gagal membaca sensor DHT!");
      return;
    }

    sensorValid = true;
    suhuTerakhir = suhu;
    kelembapanTerakhir = kelembapan;

    // ====================================
    // RULE ENGINE / KESELAMATAN SUHU
    // ====================================
    bool kondisiBahaya = (suhu > 31.0);

    if (kondisiBahaya) {
      // Aktifkan LED otomatis
      ledBahaya = true;
      ledManual = false;
      updateLED();

      // Kirim alert sekali untuk setiap kejadian panas
      if (!alertSent && client.connected()) {
        StaticJsonDocument<256> alertDoc;

        alertDoc["status"] = "BAHAYA";
        alertDoc["parameter"] = "OVERHEAT";
        alertDoc["nilai_suhu"] = suhu;

        char alertBuffer[256];
        serializeJson(alertDoc, alertBuffer);

        if (client.publish(topicAlert, alertBuffer)) {
          Serial.print("[ALERT] ");
          Serial.println(alertBuffer);
          alertSent = true;
        }
      }
    } else {
      // Suhu kembali aman
      ledBahaya = false;
      alertSent = false;
      updateLED();
    }

    // ====================================
    // TELEMETRI JSON SETIAP 4 DETIK
    // ====================================
    StaticJsonDocument<384> doc;

    doc["node_id"] = clientId;
    doc["suhu"] = suhu;
    doc["kelembapan"] = kelembapan;
    doc["status_led"] = digitalRead(ledPin);
    doc["kondisi_suhu"] =
        kondisiBahaya ? "PANAS" : "NORMAL";

    doc["uptime_detik"] = millis() / 1000;
    doc["ip_lokal"] = WiFi.localIP().toString();

    char telemetryBuffer[384];
    serializeJson(doc, telemetryBuffer);

    if (client.connected()) {
      if (client.publish(topicTelemetry, telemetryBuffer)) {
        Serial.print("[TELEMETRI] ");
        Serial.println(telemetryBuffer);
      } else {
        Serial.println("Gagal mengirim telemetri!");
      }
    }

    Serial.print("Kondisi suhu: ");
    Serial.println(kondisiBahaya ? "PANAS" : "NORMAL");
  }
}