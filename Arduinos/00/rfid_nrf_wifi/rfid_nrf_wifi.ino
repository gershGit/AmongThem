//Include statements cover the RFID, NRF24, and ESP Wifi

#include <MFRC522.h>      //Used for RFID
#include <RF24Network.h>  //Used for RF24
#include <RF24.h>         //Used for RF24
#include <ESP8266WiFi.h>  //Used for WiFi
#include <SPI.h>          //Used for RFID and RF24
#include <ESP_EEPROM.h>

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST  5   //D1
#define RFID_SDA  4   //D2
#define RF24_CE   2   //D4
#define RF24_CS   15  //D8

//Sensor Pin definitions
#define TOUCH_PIN 10

//Setup of libraries for communication
MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF24_CE, RF24_CS);
RF24Network network(radio);
const uint16_t this_node = 00;
const uint16_t node1 = 01;
const uint16_t node2 = 011;

//Status codes
// 00000001 -> Touch Sensor
// 00000010 -> Meeting Button
// 00000100 -> Log Switch
// 00001000 -> Atomic Clock Switch
byte status_code = 0;
byte encrypt_status = 0;
byte hub_state = 0;

//Node status

//[Master 01 011 021 02 012 022 03 013 023]
byte nodes_alive[10] = {0};

struct payload_t {
  unsigned long msg_type;
  unsigned long msg_code;
  unsigned long uid;
  unsigned long task_code;
};

struct wifi_data_t {
  char ssid[32];
  char password[32];
  int ip_address[4];
  int password_used;
};

struct player_t {
  unsigned long uid;
  char country[5];
  char p_name[16];
};

player_t players[20];
int player_count = 0;

WiFiClient client;
const char* wifi_ssid = "Vie at University Resident";
const char* wifi_pass = "";
IPAddress server(172,16,18,150);
int port = 8771;
bool wifi_blink = false;

void lobby();

void setup() {
  //Begin debugger
  Serial.begin(115200);
  delay(250);
  Serial.println("Initializing RFID and RF24");

  //Set pin modes
  Serial.println("Initializing pins");
  pinMode(TOUCH_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  //Initialize RFID and RF24
  SPI.begin();
  mfrc522.PCD_Init();
  radio.begin();
  network.begin(90, this_node);
  delay(100);

  //Attempt WiFi connection and register with the server
  Serial.print("Connecting to wifi...");
  WiFi.begin(wifi_ssid, wifi_pass);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (wifi_blink == false) {
      wifi_blink = true;
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      wifi_blink = false;
      digitalWrite(LED_BUILTIN, HIGH);
    }
    
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.print("Connecting to server...");
  while (1) {
    if (client.connect(server, port)) {
      Serial.println("Connected to server");
      digitalWrite(LED_BUILTIN, HIGH);
      client.print("R-Main Hub");
      break;
    } else {
      Serial.print(".");
    }
    delay(500); 
    if (wifi_blink == false) {
      wifi_blink = true;
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      wifi_blink = false;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
  lobby();
}

void lobby() {
  Serial.println("Lobby active");
  digitalWrite(LED_BUILTIN, HIGH);
  while (1) {
    yield();
    //Check for new nodes
    network.update();
    while(network.available()){
      RF24NetworkHeader header;
      payload_t incomingData;
      network.read(header, &incomingData, sizeof(incomingData));
      if (incomingData.msg_type == 200) {
        Serial.print("Registering Node: ");
        Serial.println(incomingData.msg_code);
        nodes_alive[incomingData.msg_code] = 1;
        String toSend = "N-" + String(incomingData.msg_code);
        Serial.print("Sending on WiFi: ");
        Serial.println(toSend);
        client.print(toSend);
      }
    }

    //Check for lobby ready
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.print("Received on WiFi: ");
      Serial.println(line);
      if (line == "start") {
        Serial.println("Starting game");
        break;
      } else if (line[0] == 'P') {
        Serial.println("Registering player");
      }
    }
    delay(500); 
  }
}

void checkNetwork(){
  while(network.available()){
    RF24NetworkHeader header;
    payload_t incomingData;
    network.read(header, &incomingData, sizeof(incomingData));
    Serial.print("Received: ");
    Serial.println(incomingData.msg_code);
  }
}

unsigned long getRFID(){
  if (! mfrc522.PICC_ReadCardSerial()) {
    return -1;
  }
  unsigned long hex_num;
  hex_num = mfrc522.uid.uidByte[0] << 24;
  hex_num += mfrc522.uid.uidByte[1] << 16;
  hex_num += mfrc522.uid.uidByte[2] << 8;
  hex_num += mfrc522.uid.uidByte[3];
  mfrc522.PICC_HaltA();
  return hex_num;
}

void checkRFID(){
  if (mfrc522.PICC_IsNewCardPresent()) {
    unsigned long uid = getRFID();
    if (uid != -1) {
      Serial.print("Card detected, UID: ");
      Serial.println(uid);
      Serial.print("Sending FLIP signal...");
      RF24NetworkHeader header_01(node1);
      payload_t payload = {0,2};
      bool ok = network.write(header_01, &payload, sizeof(payload)); 
      if (ok) {
        Serial.println("ok.");
      } else {
        Serial.println("failed.");
      }
    }
  }
}

void checkTouch() {
  if (digitalRead(TOUCH_PIN) == HIGH && (status_code & 0b00000001)) {
    Serial.print("Sending 0...");
    status_code = status_code & 11111110;
    RF24NetworkHeader header_01(node1);
    payload_t payload = {0,0};
    bool ok = network.write(header_01, &payload, sizeof(payload)); 
    if (ok) {
      Serial.println("ok.");
    } else {
      Serial.println("failed.");
    }
  } else if (digitalRead(TOUCH_PIN) == LOW && (!(status_code & 0b00000001))) {
    Serial.print("Sending 1...");
    status_code = status_code | 00000001;
    RF24NetworkHeader header_01(node1);
    payload_t payload = {0,1};
    bool ok = network.write(header_01, &payload, sizeof(payload)); 
    if (ok) {
      Serial.println("ok.");
    } else {
      Serial.println("failed.");
    }
  }
}

void loop() {
  network.update();
  checkNetwork();
  checkRFID();
  checkTouch();
}
