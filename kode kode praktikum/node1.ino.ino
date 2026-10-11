
#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX "LabIoTMesh"
#define MESH_PASSWORD "mutiacantik"
#define MESH_PORT 4444

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();

Task taskSendMessage(
  TASK_SECOND * 3,
  TASK_FOREVER,
  &sendMessage
);

void sendMessage() {
  int adc = analogRead(A0);

  StaticJsonDocument<200> doc;
  doc["tipe"] = "cahaya_node";
  doc["adc"] = adc;

  String msg;
  serializeJson(doc, msg);

  mesh.sendBroadcast(msg);

  Serial.print("[KIRIM CAHAYA] ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT
  );

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.println("Node 2: Sensor LDR aktif");
}

void loop() {
  mesh.update();
}