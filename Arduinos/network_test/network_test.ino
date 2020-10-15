#include <ESP8266WiFi.h>

const char* ssid = "Vie at University Resident";
const char* password = "";

const char* host = "172.16.18.150";
const uint16_t port = 8771;

const int GREEN_LED = D7;
const int RED_LED = D6;
const int BUTTON = D5;

byte buttonState = 0;
byte wifiState = 0;

WiFiClient client;

void setup()
{
  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.println("Setting up I/O");
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
}

void connectToServer(){
    Serial.print("Establishing connection to server...");
    if (client.connect(host, port))
      {
        Serial.println("Connected to server!");
        client.print(String("Hello from reactor!"));
        wifiState = 1;
    }
    else
    {
      Serial.println("Couldnt connect to server");
      wifiState = 0;
      client.stop();
    }
}

void handle_server_instruction(String line) {
  if (line == "MELTDOWN") {
    digitalWrite(RED_LED, HIGH);
  } else {
    Serial.println("Unknown Command");
  }
}

void loop()
{
  if (wifiState == 0) {
    connectToServer();
  } else if (client.available()) {
    String line = client.readStringUntil('\n');
    handle_server_instruction(line);
    Serial.println(line);
  } else if (!client.connected()) {
    Serial.print("Client not connected\n");
  }
}
