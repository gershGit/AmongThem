#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#define BUTTON 16
#define GREEN_LED 0
#define RED_LED 4
#define SECONDARY_BUTTON 5
#define D4 2
#define D8 15

RF24 radio(D4, D8);
RF24Network network(radio);      
const uint16_t this_node = 00;   
const uint16_t node01 = 01;     
const uint16_t node011 = 011;

bool button_status = false;
bool bs_2 = false;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  radio.begin();
  network.begin(90, this_node);  //(channel, node address)
  radio.setDataRate(RF24_2MBPS);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(SECONDARY_BUTTON, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
}
void loop() {
  network.update();
  //===== Receiving =====//
  while ( network.available() ) {     // Is there any incoming data?
    RF24NetworkHeader header;
    unsigned long incomingData;
    network.read(header, &incomingData, sizeof(incomingData)); // Read the incoming data
  }
  //===== Sending =====//
  if (digitalRead(BUTTON) == LOW && button_status == true){
    Serial.println("Sending on to 01");
    button_status = false;  
    RF24NetworkHeader header2(node01);     // (Address where the data is going)
    char message[5] = "ON";
    bool ok = network.write(header2, message, 5); // Send the data 
  } else if (digitalRead(BUTTON) == HIGH && button_status == false) {
    Serial.println("Sending off to 01");
    button_status = true;
    RF24NetworkHeader header2(node01);     // (Address where the data is going)
    char message[5] = "OFF";
    bool ok = network.write(header2, message, 5); // Send the data
  }

  if (digitalRead(SECONDARY_BUTTON) == LOW && bs_2 == true){
    Serial.println("Sending on to 011");
    bs_2 = false;  
    RF24NetworkHeader header2(node011);     // (Address where the data is going)
    char message[5] = "ON";
    bool ok = network.write(header2, message, 5); // Send the data 
  } else if (digitalRead(SECONDARY_BUTTON) == HIGH && bs_2 == false) {
    Serial.println("Sending off to 011");
    bs_2 = true;
    RF24NetworkHeader header2(node011);     // (Address where the data is going)
    char message[5] = "OFF";
    bool ok = network.write(header2, message, 5); // Send the data
  }
}
