#include <SPI.h> 
#include <MFRC522.h> 

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST D1       //D1
#define RFID_SDA D2       //D2
#define RF24_CE 2        //D4
#define RF24_CS 15       //D8

//Sensor Pin definitions
#define MEETING_BUTTON 1 //TX
#define GREEN_LED 0      //D3
#define RED_LED 9        //SD2
#define RFID_LED 10      //SD3
#define ATOMIC_SW A0     //A0
#define ENCRYPT_SW 16    //D0
#define LOG_SW 3         //RX

MFRC522 mfrc522(RFID_SDA, RFID_RST);

void setup() {
    //Serial.begin(115200);
    delay(100);
    Serial.println("Setting pins");
    //pinMode(1, FUNCTION_3); 
    //pinMode(3, FUNCTION_3); 
    pinMode(MEETING_BUTTON, INPUT);
    pinMode(LOG_SW, INPUT);
    pinMode(ENCRYPT_SW, INPUT);
    pinMode(ATOMIC_SW, INPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(RFID_LED, OUTPUT);
    Serial.println("Pin Set, turning LED OFF");
    SPI.begin();      // Init SPI bus
    mfrc522.PCD_Init(); 
    delay(4);
    
    digitalWrite(RFID_LED, LOW);
    delay(1000);
    digitalWrite(RFID_LED, HIGH);
    delay(1000);
    digitalWrite(RFID_LED, LOW);
}

unsigned long getRFID()
{
    if (!mfrc522.PICC_ReadCardSerial())
    {
        return 0;
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
        digitalWrite(RFID_LED, HIGH);
        delay(2000);
        digitalWrite(RFID_LED, LOW);
    }
}

void loop() {
    checkRFID();
}
