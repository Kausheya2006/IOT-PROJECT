#include <WiFi.h>
#include <HTTPClient.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#define WIFI_SSID "LAPTOP-FPTIS8PP 6659"
#define WIFI_PASSWORD "12365478"
#define THINGSPEAK_API_KEY "QUNCGTVNYZOBVIH2"
unsigned long myChannelNumber = 2914163;
WiFiClient client;

#define TURBIDITY_SENSOR_PIN 34
#define TDS_SENSOR_PIN 33
#define PH_SENSOR_PIN 35

#define BUZZER_PIN 4
#define RED_LED_PIN 5
#define GREEN_LED_PIN 17

unsigned long lastBlink = 0;
bool ledState = LOW;

void setup() {
  Serial.begin(115200);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT); // Set Buzzer Pin as OUTPUT

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  ThingSpeak.begin(client);
}

float readTurbidity() {
  int rawValue = analogRead(TURBIDITY_SENSOR_PIN);
  float voltage = (rawValue / 4095.0) * 3.3;
  return constrain(2.3 * voltage + 2.1168, 0.0, 6.74);
}

float readTDS() {
  int rawValue = analogRead(TDS_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  float tds = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
  return tds * 1.73;
}

float readPH() {
  int rawValue = analogRead(PH_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  return -5.70 * voltage + 7.4435;
}

int fetchScore() {
  HTTPClient http;
  http.begin("http://192.168.137.196:5001/predict");  // Change to your actual server IP

  // Send GET request to fetch prediction data
  int httpCode = http.GET();
  int score = -1;

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float safetyScore = doc["Safety_Score"];
      score = round(safetyScore); // Convert float to int
    } else {
      Serial.println("Failed to parse JSON");
    }
  } else {
    Serial.println("Failed to fetch score");
  }

  http.end();
  return score;
}

void applyStatus(int score) {
  if (score >= 85) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW); // Silent
  } else if (score >= 70) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH); // Continuous beep
  } else if (score >= 60) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Continuous beep
  } else if (score >= 50) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Continuous beep
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Continuous beep
  }
}

void loop() {
  float turbidity = readTurbidity();
  float pH = readPH();
  float tds = readTDS();

  Serial.printf("Turbidity: %.2f NTU, pH: %.2f, TDS: %.2f ppm\n", turbidity, pH, tds);

  int score = fetchScore();
  if (score != -1) {
    Serial.println("Water Quality Score: " + String(score));
    applyStatus(score);
  }

  // Send sensor data to ThingSpeak via POST request
  ThingSpeak.setField(1, pH);
  ThingSpeak.setField(2, turbidity);
  ThingSpeak.setField(3, tds);
  int response = ThingSpeak.writeFields(myChannelNumber, THINGSPEAK_API_KEY);
  if (response != 200) {
    Serial.println("ThingSpeak Error: " + String(response));
  }

  delay(15000); // Wait before sending the next set of data
}
