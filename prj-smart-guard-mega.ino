#include <Adafruit_Fingerprint.h>

// UART connections
constexpr uint32_t SERIAL_USB_BAUD = 115200;
constexpr uint32_t FINGERPRINT_BAUD = 57600;

Adafruit_Fingerprint fingerprint1(&Serial1);
Adafruit_Fingerprint fingerprint3(&Serial3);

bool registrationRequested = false;
String serial2Buffer;
bool serial2Capturing = false;
String serialCmdBuffer;

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
  processSerialCommands();
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

void processSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialCmdBuffer.length() > 0) {
        handleSerialCommand(serialCmdBuffer);
        serialCmdBuffer = "";
      }
      continue;
    }

    serialCmdBuffer += c;

    if (serialCmdBuffer == "DEBUGON" || serialCmdBuffer == "DEBUGOFF") {
      handleSerialCommand(serialCmdBuffer);
      serialCmdBuffer = "";
    }
  }
}

void handleSerialCommand(const String &command) {
  Serial.print("Forwarding ");
  Serial.print(command);
  Serial.println(" to Serial2");
  Serial2.print(command);
}

void processSerial2() {
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      continue;
    }

    if (c == '$') {
      serial2Capturing = true;
      serial2Buffer = "$";
      Serial.println("Serial2: start delimiter found, buffering command");
      continue;
    }

    if (!serial2Capturing) {
      Serial.print("Serial2: ignoring unexpected char '");
      Serial.print(c);
      Serial.println("'");
      continue;
    }

    serial2Buffer += c;

    if (serial2Buffer.length() > 64) {
      Serial.println("Serial2: buffer overflow, resetting");
      serial2Buffer = "";
      serial2Capturing = false;
      continue;
    }

    if (c == '#') {
      Serial.print("Serial2 received: ");
      Serial.println(serial2Buffer);

      if (serial2Buffer == "$FP_REG#") {
        registrationRequested = true;
      } else if (serial2Buffer == "$OPEN_LOCK#") {
        Serial.println("Lock successfully opened");
      } else {
        Serial.println("Serial2: unknown command");
      }

      serial2Buffer = "";
      serial2Capturing = false;
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
