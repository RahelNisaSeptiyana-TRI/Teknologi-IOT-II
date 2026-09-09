const int buttonPin = 4;
const int ledPin = 5;

int buttonState = 0;
int lastButtonState = LOW;

bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Saat pertama dinyalakan, LED mati
  digitalWrite(ledPin, LOW);
}

void loop() {
  buttonState = digitalRead(buttonPin);

  // Jika tombol baru saja ditekan
  if (buttonState == HIGH && lastButtonState == LOW) {

    // Membalik status LED: mati -> nyala, nyala -> mati
    ledState = !ledState;

    digitalWrite(ledPin, ledState);

    // Debounce untuk mencegah satu tekanan terbaca berkali-kali
    delay(200);
  }

  lastButtonState = buttonState;
}