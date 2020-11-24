//Include statements cover the RFID, NRF24, and ESP Wifi
#include <MFRC522.h>
#include <SPI.h>		 //Used for RFID and RF24

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST 7
#define RFID_SDA 8

MFRC522 mfrc522(RFID_SDA, RFID_RST);

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
			Serial.print("RFID Detected: ");
			Serial.println(uid);
			return;
		}
	}
}

void setup()
{
	//Begin debugger
	Serial.begin(115200);
	delay(250);


	//Set pin modes
	Serial.println("Initializing pins");

	//Initialize RFID and RF24
	Serial.println("Initializing RFID and RF24");
	SPI.begin();
	mfrc522.PCD_Init();
	delay(4);
	delay(100);

	Serial.println("Ready for RFID cards");
}

void loop()
{
	checkRFID();
}
