#include <WiFi.h>
#include <ThingSpeak.h>

// WiFi and ThingSpeak Configuration
#define WIFI_SSID "LAPTOP-FPTIS8PP 6659"
#define WIFI_PASSWORD "12365478"
#define THINGSPEAK_API_KEY "QUNCGTVNYZOBVIH2"
unsigned long myChannelNumber = 2914163;
WiFiClient client;

// Sensor Pins
#define TURBIDITY_SENSOR_PIN 34
#define TDS_SENSOR_PIN 33
#define PH_SENSOR_PIN 35

// Alarm Pins
#define BUZZER_PIN 4
#define RED_LED_PIN 5    // Alarm indicator
#define GREEN_LED_PIN 17  // Safe indicator

// Threshold Values
#define TDS_THRESHOLD 11         // ppm
#define TURBIDITY_THRESHOLD 5.03 // NTU
#define PH_LOW_THRESHOLD 6.5     // Minimum safe pH
#define PH_HIGH_THRESHOLD 8.5    // Maximum safe pH

void setup() {
  Serial.begin(115200);
  
  // Initialize outputs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  // Connect to WiFi
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
  float ntu = 2.3 * voltage + 2.1168;
  return constrain(ntu, 0.0, 6.74);
}

float readTDS() {
  int rawValue = analogRead(TDS_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  return (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
}

float readPH() {
  int rawValue = analogRead(PH_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);
  return -5.70 * voltage + 21.34;
}

void checkAlarmConditions(float turbidity, float pH, float tds) {
  bool turbidityAlert = turbidity > TURBIDITY_THRESHOLD;
  bool phAlert = (pH < PH_LOW_THRESHOLD) || (pH > PH_HIGH_THRESHOLD);
  bool tdsAlert = tds > TDS_THRESHOLD;
  bool anyAlert = turbidityAlert || phAlert || tdsAlert;

  // Control indicators
  digitalWrite(GREEN_LED_PIN, !anyAlert);  // Green ON when safe
  digitalWrite(RED_LED_PIN, anyAlert);     // Red ON when alarm
  digitalWrite(BUZZER_PIN, !anyAlert);      // Buzzer ON when alarm

  // Print warnings
  if (anyAlert) {
    Serial.println("ALARM! Unsafe conditions detected:");
    if (turbidityAlert) Serial.println("- High turbidity!");
    if (phAlert) {
      if (pH < PH_LOW_THRESHOLD) Serial.println("- pH too low (acidic)!");
      else Serial.println("- pH too high (basic)!");
    }
    if (tdsAlert) Serial.println("- High TDS level!");
  }
}

void loop() {
  // Read sensors
  float turbidity = readTurbidity();
  float pH = readPH();
  float tds = readTDS();

  // Display readings
  Serial.print("Turbidity (NTU): ");
  Serial.print(turbidity);
  Serial.print("\tpH: ");
  Serial.print(pH);
  Serial.print("\tTDS (ppm): ");
  Serial.println(tds);

  // Check alarm conditions
  checkAlarmConditions(turbidity, pH, tds);

  // Send data to ThingSpeak
  ThingSpeak.setField(1, pH);
  ThingSpeak.setField(2, turbidity);
  ThingSpeak.setField(3, tds);
  
  int responseCode = ThingSpeak.writeFields(myChannelNumber, THINGSPEAK_API_KEY);
  if (responseCode != 200) {
    Serial.println("ThingSpeak Error: " + String(responseCode));
  }

  delay(15000); // Wait 15 seconds between readings
}