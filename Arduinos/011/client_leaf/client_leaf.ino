#include <MFRC522.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

#define GREEN_LED A3
#define RED_LED A5
#define CAP_IN A0
#define RFID_SDA 8
#define RFID_RST 7
bool cap_down = false;

MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(9, 10);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t this_node = 01;   // Address of our node in Octal format ( 04,031, etc)
const uint16_t master00 = 00;    // Address of the other node in Octal format

struct payload_t
{
  unsigned long msg_type;
  unsigned long msg_code;
  unsigned long uid;
  unsigned long task_code;
};

void setup() {
  Serial.begin(115200);

  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();
  radio.begin();
  network.begin(90, this_node); //(channel, node address)
  delay(100);

  //Pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(CAP_IN, INPUT);
  analogWrite(GREEN_LED, 128);
  analogWrite(RED_LED, 255);
  delay(1000);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
}

unsigned long getRFID()
{
  if (!mfrc522.PICC_ReadCardSerial())
  {
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

void checkRFID()
{
  if (mfrc522.PICC_IsNewCardPresent())
  {
    unsigned long uid = getRFID();
    if (uid != -1)
    {
      Serial.print("Sending tap.");
      RF24NetworkHeader header_00(master00);
      payload_t payload = {0, 2, 0, 0};
      bool ok = network.write(header_00, &payload, sizeof(payload));
      while (!ok) {
        Serial.print(".");
        ok = network.write(header_00, &payload, sizeof(payload));
        delay(250);
      }
      Serial.println("ok.");
    }
  }
}

void loop() {
  network.update();
  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header;
    payload_t incomingData;
    network.read(header, &incomingData, sizeof(payload_t)); // Read the incoming data
    Serial.print("Received: ");
    Serial.println(incomingData.msg_type);

    if (incomingData.msg_type == 15) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

  if (analogRead(CAP_IN) > 150 && !cap_down) {
    Serial.print("Sending message...");
    cap_down = true;
    payload_t payload;
    payload.msg_type = 1;
    payload.msg_code = 1;
    payload.task_code = 5;
    payload.uid = 42521234;
    RF24NetworkHeader header_00(master00);
    bool ok = network.write(header_00, &payload, sizeof(payload));
    if (ok)
    {
      Serial.println("ok.");
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
    }
    else
    {
      Serial.println("failed.");
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
    }
  }
  else if (analogRead(CAP_IN) < 150 && cap_down)
  {
    cap_down = false;
  }

  checkRFID();
}
