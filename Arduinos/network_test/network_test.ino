#include <ESP8266WiFi.h>

const char* ssid = "Vie at University Resident";
const char* password = "";

const char* host = "172.16.18.150";
const uint16_t port = 8771;

const int GREEN_LED = D7;
const int YELLOW_LED = D0;
const int RED_LED = D6;
const int BUTTON = D5;

byte buttonState = 0;
byte reactorState = 0;
byte wifiState = 0;
unsigned long time_since_last_poll;

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
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  digitalWrite(GREEN_LED, HIGH);
  time_since_last_poll = millis();
}

void connectToServer(){
    Serial.print("Establishing connection to server...");
    if (client.connect(host, port))
      {
        Serial.println("Connected to server!");
        client.print(String("R-Reactor"));
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
    reactorState = 1;
  } else {
    Serial.println("Unknown Command");
  }
}

void loop()
{
  if (wifiState == 0 and millis()-time_since_last_poll>5000) {
    time_since_last_poll = millis();
    connectToServer();
  } else if (client.available()) {
    String line = client.readStringUntil('\n');
    handle_server_instruction(line);
    Serial.println(line);
  } else if (!client.connected() and millis()-time_since_last_poll>5000) {
    Serial.print("Client not connected\n");
    wifiState = 0;
  }
  if (millis() - time_since_last_poll > 10000){
    Serial.println("V-Reactor");
    client.print(String("V-Reactor"));
    time_since_last_poll = millis();
  }

  if (digitalRead(BUTTON) == HIGH){
    if (buttonState == 0){
      buttonState = 1;
      Serial.println("F-Reactor");
      client.print("F-Reactor");
    }
  } else if (digitalRead(BUTTON) == LOW) {
    if (buttonState == 1) {
      buttonState = 0;
      Serial.println("D-Reactor");
      client.print("D-Reactor");
    }
  }
  if (reactorState==1 && buttonState==1){
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
  } else if (reactorState==0) {
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } else if (reactorState==1 && buttonState==0){
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
  
}
