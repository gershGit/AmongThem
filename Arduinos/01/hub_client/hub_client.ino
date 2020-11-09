#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#define GREEN_LED 9

RF24 radio(7, 8);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t this_node = 01;   // Address of our node in Octal format ( 04,031, etc)
const uint16_t master00 = 00;    // Address of the other node in Octal format

bool LED_status = false;

struct payload_t {
  unsigned long msg_type;
  unsigned long msg_code;
  unsigned long uid;
  unsigned long task_code;
};

void setup() {
  Serial.begin(115200);
  SPI.begin();
  radio.begin();
  network.begin(90, this_node); //(channel, node address)
  pinMode(GREEN_LED, OUTPUT);
  Serial.println("Setup complete");

  //Loop to register with the server
  Serial.print("Connecting to hub...");
  while(1) {
    network.update();
    RF24NetworkHeader header_00(master00);
    payload_t payload = {200,1};
    bool ok = network.write(header_00, &payload, sizeof(payload)); 
    //TODO wait for a response that specifies a true connection
    if (ok) {
      Serial.println("Connected");
      break;
    }
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Going into game-mode");
}
void loop() {
  network.update();

  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header;
    payload_t payload;
    network.read(header,&payload,sizeof(payload));
    Serial.print("Received: ");
    Serial.println(payload.msg_code);
    
    if (payload.msg_code == 0) {
      digitalWrite(GREEN_LED, HIGH);
      LED_status = true;
    } else if (payload.msg_code == 1) {
      digitalWrite(GREEN_LED, LOW);
      LED_status = false;
    } else if (payload.msg_code == 2) {
      if (LED_status == true) {
        digitalWrite(GREEN_LED, LOW);
        LED_status = false;
      } else {
        digitalWrite(GREEN_LED, HIGH);
        LED_status = true;
      }
    }
  }
}
