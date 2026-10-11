#include <ESP8266WiFi.h>
#include <espnow.h>

// GANTI dengan MAC Address Board Penerima 1 dan Board Penerima 2 Anda!
uint8_t receiver1[] = {0x60, 0x01, 0x94, 0x7B, 0x85, 0x24};
uint8_t receiver2[] = {0x34, 0x94, 0x54, 0x8A, 0x84, 0xC0};

// Definisi struktur data paket instruksi
typedef struct struct_pesan {
  int perintahId;
  int nilaiParameter;
} struct_pesan;

struct_pesan paketKirim;

unsigned long previousMillis = 0;
const long interval = 2000; // Transmisi berkala setiap 2 detik

// Callback otomatis saat paket selesai dipancarkan radio
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  Serial.print("Kirim paket ke: ");
  Serial.print(macStr);
  Serial.print(" | Status: ");
  if (sendStatus == 0) {
    Serial.println("Berhasil Diterima");
  } else {
    Serial.println("Gagal (Tidak Terjangkau)");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Inisialisasi stack protokol ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Gagal menginisialisasi ESP-NOW!");
    return;
  }

  // Tetapkan peran board sebagai Controller
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // Daftarkan kedua board penerima ke tabel peer internal
  esp_now_add_peer(receiver1, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_add_peer(receiver2, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  Serial.println("Controller ESP-NOW Siap!");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    paketKirim.perintahId = 101;
    paketKirim.nilaiParameter = random(10, 100);

    // Argumen 0 berarti mengirimkan paket ke seluruh peer yang terdaftar
    esp_now_send(0, (uint8_t *) &paketKirim, sizeof(paketKirim));
  }
}