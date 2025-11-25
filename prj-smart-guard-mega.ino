#include <Adafruit_Fingerprint.h>

// UART connections
constexpr uint32_t SERIAL_USB_BAUD = 115200;
constexpr uint32_t FINGERPRINT_BAUD = 57600;

Adafruit_Fingerprint fingerprint1(&Serial1);
Adafruit_Fingerprint fingerprint3(&Serial3);

bool registrationRequested = false;
String serial2Buffer;

void setup() {
  Serial.begin(SERIAL_USB_BAUD);
  Serial1.begin(FINGERPRINT_BAUD);
  Serial2.begin(SERIAL_USB_BAUD);
  Serial3.begin(FINGERPRINT_BAUD);

  fingerprint1.begin(FINGERPRINT_BAUD);
  fingerprint3.begin(FINGERPRINT_BAUD);

  Serial.println("Fingerprint system ready");
}

void loop() {
  processSerial2();

  if (registrationRequested) {
    registrationRequested = false;
    int newId = enrollFingerprint(fingerprint1);

    if (newId > 0) {
      sendSerial2Response("$FP_OK: " + String(newId) + "#");
    } else {
      sendSerial2Response("$FP_FAIL#");
    }
  }
}

void processSerial2() {
  while (Serial2.available()) {
    char incoming = Serial2.read();
    if (incoming == '\n' || incoming == '\r') {
      continue;
    }

    serial2Buffer += incoming;

    if (incoming == '#') {
      Serial.print("Serial2 received: ");
      Serial.println(serial2Buffer);

      if (serial2Buffer == "$FP_REG#") {
        registrationRequested = true;
      } else {
        Serial.println("Serial2: unknown command");
      }

      serial2Buffer = "";
    }

    if (serial2Buffer.length() > 64) {
      serial2Buffer = "";
    }
  }
}

void sendSerial2Response(const String &message) {
  Serial.print("Serial2 response: ");
  Serial.println(message);
  Serial2.print(message);
}

bool waitForFinger(Adafruit_Fingerprint &sensor) {
  int result = -1;
  unsigned long start = millis();

  while (millis() - start < 15000) {
    result = sensor.getImage();

    if (result == FINGERPRINT_OK) {
      Serial.println("Finger captured");
      return true;
    }

    if (result != FINGERPRINT_NOFINGER) {
      Serial.print("Capture error (");
      Serial.print(result);
      Serial.println(")");
    }

    delay(200);
  }

  Serial.println("Capture timeout");
  return false;
}

int enrollFingerprint(Adafruit_Fingerprint &sensor) {
  Serial.println("Starting enrollment on Serial1");

  if (!waitForFinger(sensor)) {
    return 0;
  }

  if (sensor.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Image to template 1 failed");
    return 0;
  }

  Serial.println("Remove finger");
  delay(2000);

  if (!waitForFinger(sensor)) {
    return 0;
  }

  if (sensor.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("Image to template 2 failed");
    return 0;
  }

  if (sensor.createModel() != FINGERPRINT_OK) {
    Serial.println("Model creation failed");
    return 0;
  }

  int nextId = 1;
  if (sensor.getTemplateCount() == FINGERPRINT_OK) {
    nextId = sensor.templateCount + 1;
  }

  if (nextId > 127) {
    Serial.println("No available template slots");
    return 0;
  }

  if (sensor.storeModel(nextId) != FINGERPRINT_OK) {
    Serial.println("Saving model failed");
    return 0;
  }

  Serial.print("Enrollment success template ID ");
  Serial.println(nextId);
  return nextId;
}
