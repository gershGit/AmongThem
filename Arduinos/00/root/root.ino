#include <ESP8266WiFi.h>

const byte GREEN_LED = 0;
const byte RED_LED = 4;
const byte LOG_SW = 5;
const byte MEET_SW = 16;

bool swipe_status = false;
unsigned long swipe_start;
unsigned long swipe_end;
bool wifi_blink = false;

WiFiClient client;
IPAddress server(172,16,18,150);
int port = 8771;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(LOG_SW, INPUT);
  pinMode(MEET_SW, INPUT);
  pinMode(RED_LED, OUTPUT);
  Serial.println("Pins set");

  //TODO loop on connection, checking for if setup button is pressed
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  Serial.println("Connecting to wifi...");
  WiFi.begin("Vie at University Resident");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (wifi_blink == false) {
      wifi_blink = true;
      digitalWrite(RED_LED, LOW);
    } else {
      wifi_blink = false;
      digitalWrite(RED_LED, HIGH);
    }
    
  }
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Connecting to server...");
  if (client.connect(server, port)) {
    Serial.println("Connected to server");
    digitalWrite(GREEN_LED, HIGH);
    client.print("R-Main Hub");
  }
  delay(2500);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  while (1) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      break;
    }
  }

  Serial.println("Starting main loop");

  
  //TODO go into hub loop waiting for nodes to connect to this network, send updates to the server about which nodes are connected
  //TODO go into game waiting function that waits for the server to tell it to start
}

void sendSwipeSuccess(){
  Serial.println("Swipe!");
  client.print("T-Log");
  delay(1000);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

void check_swipe(){
  if (swipe_end-swipe_start > 1000 && swipe_end-swipe_start < 1500) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    sendSwipeSuccess();
  } else {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  }
}

void callMeeting() {
  Serial.println("Calling a meeting!");
  client.print("A-Meeting");
}

void loop() {
  if (digitalRead(LOG_SW) == LOW) {
    if (swipe_status == false) {
      Serial.print("Starting swipe");
      //Starting a new swipe
      swipe_status = true;
      swipe_start = millis();
    }
  } else {
    if (swipe_status == true) {
      swipe_status = false;
      swipe_end = millis();
      check_swipe();
    }
  }

  if (digitalRead(MEET_SW) == LOW) {
    callMeeting();
    delay(2000);
  }

  if (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(line);
  }
}
