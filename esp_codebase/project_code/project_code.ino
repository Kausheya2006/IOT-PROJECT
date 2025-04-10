#include <WiFi.h>
#include <ThingSpeak.h>

#define WIFI_SSID "LAPTOP-FPTIS8PP 6659"
#define WIFI_PASSWORD "12365478"
#define THINGSPEAK_API_KEY "QUNCGTVNYZOBVIH2"
#define TURBIDITY_SENSOR_PIN 34
#define TDS_SENSOR_PIN 33
#define PH_SENSOR_PIN 35

WiFiClient client;
unsigned long myChannelNumber = 2914163;

void setup() {
  Serial.begin(115200);
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
  float voltage = (rawValue / 4095.0) * 3.3;  // Convert raw value to voltage
  
  // Apply the linear interpolation formula
  float ntu = 2.3 * voltage + 2.1168;   // higher ntu  dirtier water
  
  // Ensure non-negative NTU and cap it at the highest expected value (for worst water)
  if (ntu < 0.0) {
    ntu = 0.0;  // Clear water should give 0 NTU
  } else if (ntu > 6.74) {
    ntu = 6.74;  // Cap at the max NTU value (worst water)
  }
  
  return ntu;
}


float readTDS() {
  int rawValue = analogRead(TDS_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);  // Convert ADC to voltage (ESP32: 12-bit ADC, 3.3V ref)
  
  // Temperature compensation can be added later if needed
  float tdsValue = (133.42 * voltage * voltage * voltage
                   - 255.86 * voltage * voltage
                   + 857.39 * voltage) * 0.5;  // *0.5 is a common calibration factor
  
  return tdsValue;  // TDS in ppm
}


float readPH() {
  int rawValue = analogRead(PH_SENSOR_PIN);
  float voltage = rawValue * (3.3 / 4095.0);  // 12-bit ADC, 3.3V reference

  // Linear mapping based on sensor calibration
  float phValue = -5.70 * voltage + 21.34;

  return phValue;
}


void loop() {
  float turbidity = readTurbidity();
  float pH = readPH();
  float tds = readTDS();

  Serial.print("Turbidity (NTU): ");
  Serial.print(turbidity);
  Serial.print("\tpH: ");
  Serial.print(pH);
  Serial.print("\tTDS (ppm): ");
  Serial.println(tds);

  ThingSpeak.setField(1, pH);
  ThingSpeak.setField(2, turbidity);
  ThingSpeak.setField(3, tds);

  int responseCode = ThingSpeak.writeFields(myChannelNumber, THINGSPEAK_API_KEY);
  if (responseCode != 200) Serial.println("ThingSpeak Error: " + String(responseCode));
  delay(15000);
}