//#include "DHT.h"
#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Losant.h>

#define DHTPIN 14     // what digital pin the DHT22 is conected to  D3
#define DHTTYPE DHT22 // There are multiple kinds of DHT sensors

//DHT dht(DHTPIN, DHTTYPE);
DHTesp dht;

// WiFi credentials.
const char *WIFI_SSID = "XXXX";
const char *WIFI_PASS = "XXXX";

// Losant credentials.
const char *LOSANT_DEVICE_ID = "XXXXX";
const char *LOSANT_ACCESS_KEY = "XXXXXX";
const char *LOSANT_ACCESS_SECRET = "XXXX";

//WiFiClientSecure wifiClient;
WiFiClient wifiClient; // << works

LosantDevice device(LOSANT_DEVICE_ID);

void connect()
{

  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // WiFi fix: https://github.com/esp8266/Arduino/issues/2186
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED)
  {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED)
    {
      Serial.println("Failed to connect to WIFI. Please verify credentials: ");
      Serial.println();
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      Serial.print("Password: ");
      Serial.println(WIFI_PASS);
      Serial.println();
    }

    delay(500);
    Serial.println("...");
    // Only try for 5 seconds.
    if (millis() - wifiConnectStart > 5000)
    {
      Serial.println("Failed to connect to WiFi");
      Serial.println("Please attempt to send updated configuration parameters.");
      return;
    }
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.print("Authenticating Device...");
  HTTPClient http;
  http.begin("http://api.losant.com/auth/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();
  root["deviceId"] = LOSANT_DEVICE_ID;
  root["key"] = LOSANT_ACCESS_KEY;
  root["secret"] = LOSANT_ACCESS_SECRET;
  String buffer;
  serializeJson(root, buffer);

  int httpCode = http.POST(buffer);

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      Serial.println("This device is authorized!");
    }
    else
    {
      Serial.println("Failed to authorize device to Losant.");
      if (httpCode == 400)
      {
        Serial.println("Validation error: The device ID, access key, or access secret is not in the proper format.");
      }
      else if (httpCode == 401)
      {
        Serial.println("Invalid credentials to Losant: Please double-check the device ID, access key, and access secret.");
      }
      else
      {
        Serial.println("Unknown response from API");
      }
    }
  }
  else
  {
    Serial.println("Failed to connect to Losant API.");
  }

  http.end();

  // Connect to Losant.
  Serial.println();
  Serial.print("Connecting to Losant...");

  //device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);
  device.connect(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET); // << works

  while (!device.connected())
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected!");
  Serial.println();
  Serial.println("This device is now ready for use!");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while (!Serial)
  {
  }

  Serial.println("Device Started");
  Serial.println("-------------------------------------");
  Serial.println("Running DHT!");
  Serial.println("-------------------------------------");
  dht.setup(14, DHTesp::DHT22); // Connect DHT sensor to GPIO 14//17

  connect();
}

void report(double humidity, double tempC, double tempF, double heatIndexC, double heatIndexF)
{
  StaticJsonDocument<500> doc;
  JsonObject root = doc.to<JsonObject>();
  root["humidity"] = humidity;
  root["tempC"] = tempC;
  root["tempF"] = tempF;
  root["heatIndexC"] = heatIndexC;
  root["heatIndexF"] = heatIndexF;
  device.sendState(root);
  Serial.println("Reported!");
}

int timeSinceLastRead = 0;
void loop()
{
  bool toReconnect = false;

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Disconnected from WiFi");
    toReconnect = true;
  }

  if (!device.connected())
  {
    Serial.println("Disconnected from MQTT");
    toReconnect = true;
  }

  if (toReconnect)
  {
    connect();
  }

  device.loop();

  // Report every 2 seconds.
  if (timeSinceLastRead > 5000)
  {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.getHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.getTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.toFahrenheit(t);

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f))
    {
      Serial.println("Failed to read from DHT sensor!");
      timeSinceLastRead = 0;
      return;
    }

    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(f, h, true);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);

    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print(f);
    Serial.print(" *F\t");
    Serial.print("Heat index: ");
    Serial.print(hic);
    Serial.print(" *C ");
    Serial.print(hif);
    Serial.println(" *F");
    report(h, t, f, hic, hif);

    timeSinceLastRead = 0;
  }
  delay(100);
  timeSinceLastRead += 100;
}