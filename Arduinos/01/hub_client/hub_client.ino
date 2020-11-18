#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio(16, 5);               // nRF24L01 (CE,CSN)
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
  delay(2000);
  Serial.println();
  Serial.println("Starting up");

  SPI.begin();
  radio.begin();
  network.begin(90, this_node); //(channel, node address)

  //Loop to register with the server
  Serial.print("Connecting to root...");
  while(1) {
    network.update();
    RF24NetworkHeader header_00(master00);
    payload_t payload = {99,1,0,0};
    bool ok = network.write(header_00, &payload, sizeof(payload));
    if (ok) {
      Serial.println("Connected to server");
      break;
    } else {
      Serial.print(".");
    }
    delay(1000);
  }
}
void loop() {
  network.update();

  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header;
    payload_t payload;
    network.read(header,&payload,sizeof(payload));

    if (payload.msg_code == 99) {
      Serial.println("Start Command");
    } else if (payload.msg_code == 1) {
      Serial.println("Other Command");
    }
  }
}
