
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <string.h>

#define MESH_PREFIX "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT 1111

#define LED_PIN D6

Scheduler userScheduler;
painlessMesh mesh;

float suhuTerakhir = 0;
int adcTerakhir = 1023;

bool dataSuhuAda = false;
bool dataCahayaAda = false;

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.println("[ERROR] JSON tidak valid");
    return;
  }

  const char* tipe = doc["tipe"] | "";

  if (strcmp(tipe, "suhu_node") == 0) {
    suhuTerakhir = doc["suhu"] | 0.0;
    float kelembapan = doc["kelembapan"] | 0.0;
    dataSuhuAda = true;

    Serial.printf(
      "[SUHU] %.2f C | Kelembapan %.2f %%\n",
      suhuTerakhir, kelembapan
    );

  } else if (strcmp(tipe, "cahaya_node") == 0) {
    adcTerakhir = doc["adc"] | 1023;
    dataCahayaAda = true;

    Serial.printf(
      "[CAHAYA] Nilai ADC: %d\n",
      adcTerakhir
    );
  } else {
    return;
  }

  bool suhuKritis =
    dataSuhuAda && suhuTerakhir > 31.0;

  bool gelap =
    dataCahayaAda && adcTerakhir < 300;

  if (suhuKritis || gelap) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[AKTUATOR] LED MENYALA");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("[AKTUATOR] LED MATI");
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf(
    "--> Node terhubung: %u\n",
    nodeId
  );
}

void changedConnectionCallback() {
  Serial.println("--> Topologi mesh berubah");
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT
  );

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  Serial.println("Node 3: Hub kontrol aktif");
}

void loop() {
  mesh.update();
}