#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#define GREEN_LED 9

RF24 radio(7, 8);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t this_node = 01;   // Address of our node in Octal format ( 04,031, etc)
const uint16_t master00 = 00;    // Address of the other node in Octal format

void setup() {
  Serial.begin(9600);
  SPI.begin();
  radio.begin();
  network.begin(90, this_node); //(channel, node address)
  radio.setDataRate(RF24_2MBPS);
  pinMode(GREEN_LED, OUTPUT);
}
void loop() {
  network.update();
  //===== Receiving =====//
  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header;
    char incomingData[5];
    network.read(header, incomingData, 5); // Read the incoming data
    incomingData[4] = '\0';
    Serial.println(incomingData);
    
    if (strcmp(incomingData, "ON") == 0) {
      digitalWrite(GREEN_LED, HIGH);
    } else {
      digitalWrite(GREEN_LED, LOW);
    }
  }
}
