/* Example dumpoing to Serial values from the BME80 and the TIC inputs. */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Create the BME280 object
Adafruit_BME280 bme; 

void setup() {
  Serial.begin(115200);
  while(!Serial);    // Wait for serial monitor to open
  
  Serial.println(F("BME280 Test - Imperial Units"));

  // Default address is usually 0x77. If it doesn't work, try 0x76.
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

void loop() {
  // Reading Temperature and converting C to F
  float tempF = (bme.readTemperature());
  
  // Reading Pressure and converting hPa to inHg
  float pressureInHg = bme.readPressure() / 3386.39;
  
  // Reading Humidity (%)
  float humidity = bme.readHumidity();

  // Print results to Serial
  Serial.print("Temp: ");
  Serial.print(tempF);
  Serial.print(" °C | ");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" % | ");

  Serial.print("Pressure: ");
  Serial.print(pressureInHg);
  Serial.println(" inHg");

  delay(2000); // 2-second update interval
}