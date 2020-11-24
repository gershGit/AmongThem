//Include statements cover the RFID, NRF24, and ESP Wifi
#include <MFRC522.h>
#include <RF24Network.h> //Used for RF24
#include <RF24.h>		 //Used for RF24
#include <SPI.h>		 //Used for RFID and RF24
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <math.h>

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST 7
#define RFID_SDA 8
#define RF24_CE 9
#define RF24_CS 10

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define TO_RAD 0.0174532925

//Pin declarations for physical tasks
#define PHOTO_RESISTOR A0
#define FUEL_CELLS A6
#define AIRLOCK_1 A1
#define AIRLOCK_2 A2
#define AIRLOCK_3 A3
#define PUMP_1 A6
#define PUMP_2 A7

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VERTICAL_KNOB_A 5
#define VERTICAL_KNOB_B 6
#define VERTICAL_KNOB_SWITCH A7
RotaryEncoder v_encoder(VERTICAL_KNOB_A, VERTICAL_KNOB_B);
int v_knob_button_state = false;
int v_pos;
int h_pos;

#define HORIZONTAL_KNOB_A 2
#define HORIZONTAL_KNOB_B 3
#define HORIZONTAL_KNOB_SWITCH 4
RotaryEncoder h_encoder(HORIZONTAL_KNOB_A, HORIZONTAL_KNOB_B);

//----------------------------------------------------------------------------------------------------------------

//Setup of libraries for communication
//MFRC522 mfrc522(RFID_SDA, RFID_RST);
MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF24_CE, RF24_CS);
RF24Network network(radio);
const uint16_t this_node = 02;
const uint16_t master00 = 00;

struct payload_t
{
	unsigned long msg_type;
	unsigned long msg_code;
	unsigned long uid;
	unsigned long task_code;
};

struct message_t
{
	bool valid = false;
	RF24NetworkHeader header;
	payload_t payload;
};

message_t checkNetwork()
{
	while (network.available())
	{
		RF24NetworkHeader header;
		payload_t incomingData;
		network.read(header, &incomingData, sizeof(incomingData));
		Serial.println("Received payload: ");
		Serial.print("\tType: ");
		Serial.println(incomingData.msg_type);
		Serial.print("\tCode: ");
		Serial.println(incomingData.msg_code);
		Serial.print("\tTask: ");
		Serial.println(incomingData.task_code);
		Serial.print("\tUID: ");
		Serial.println(incomingData.uid);
		message_t message;
		message.valid = true;
		message.header = header;
		message.payload = incomingData;
		return message;
	}
	message_t invalid;
	invalid.valid = false;
	return invalid;
}

void testNetwork() {
	delay(1000);
	Serial.print("Testing network...");
	while (true) {
		network.update();
		message_t next_message = checkNetwork();
		if (next_message.valid) {
			Serial.print("\tFrom: ");
			Serial.println(next_message.header.from_node);
			Serial.print("\tTo: ");
			Serial.println(next_message.header.to_node);
			Serial.print("\tID: ");
			Serial.println(next_message.header.id);
			break;
		}

		payload_t payload;
		payload.msg_code = 2;
		payload.msg_type = 99;
		payload.task_code = 0;
		payload.uid = 2;

		Serial.print(".");

		RF24NetworkHeader temp_header(master00);
		network.write(temp_header, &payload, sizeof(payload));
		delay(2000);
	}
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

void testRFID()
{
	Serial.println("Waiting for RFID card");
	while (true) {
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
		delay(250);
	}
}

void testPhotoResistor() {
	Serial.println("Shine a light now!");
	int start_light = analogRead(PHOTO_RESISTOR);
	while (true) {
		if (analogRead(PHOTO_RESISTOR) > start_light+300) {
			return;
		}
	}
}

void testEncoders() {
	Serial.println("Testing encoders...");
	while (true) {
		v_encoder.tick();
		h_encoder.tick();
		int v_diff = v_encoder.getPosition() - v_pos;
		v_pos = v_encoder.getPosition();
		int h_diff = h_encoder.getPosition() - h_pos;
		h_pos = h_encoder.getPosition();
		
		if (v_diff != 0 || h_diff != 0) {
			Serial.print(v_pos);
			Serial.print(",");
			Serial.println(h_pos);
		}
		if (analogRead(VERTICAL_KNOB_SWITCH) < 150 && digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW) {
			Serial.println("Button clicked!");
			return;
		}
		delay(50);
	}
}

void testOLED() {
	display.clearDisplay();
	int xpos = 25, ypos = 2;
	display.setTextSize(2);
	display.cp437(true);
	char menu_options[7] = {'M', 'P', 'S', 'R', 'E', 'A', 'L'};
	for (int i = 0; i < 7; i++)
	{
		if (i == 3)
		{
			xpos = 10;
			ypos = 16;
		}
		display.setCursor(xpos, ypos);
		display.setTextColor(SSD1306_WHITE);
		display.write(menu_options[i]);
		xpos += 30;
	}
	display.display();
}

void testAirlock() {
	Serial.println("Testing Airlock-1");
	while(analogRead(AIRLOCK_1) > 150) {
		delay(100);
	}
	Serial.println("Testing Airlock-2");
	while (analogRead(AIRLOCK_2) > 150)
	{
		delay(100);
	}
	Serial.println("Testing Airlock-3");
	while (analogRead(AIRLOCK_3) > 150)
	{
		delay(100);
	}
}

void testCells() {
	Serial.println("Testing fuel cells");
	while(true) {
		if (analogRead(FUEL_CELLS) > 5) {
			Serial.println("Cells connected!");
			return;
		}
		delay(500);
	}
}

void setup()
{
	//Begin debugger
	Serial.begin(115200);
	delay(250);
	Serial.println("Initializing RFID and RF24");

	//Set pin modes
	Serial.println("Initializing pins");
	pinMode(VERTICAL_KNOB_SWITCH, INPUT);
	pinMode(VERTICAL_KNOB_A, INPUT);
	pinMode(VERTICAL_KNOB_B, INPUT);
	pinMode(HORIZONTAL_KNOB_SWITCH, INPUT);
	pinMode(HORIZONTAL_KNOB_A, INPUT);
	pinMode(HORIZONTAL_KNOB_B, INPUT);
	pinMode(FUEL_CELLS, INPUT);
	pinMode(AIRLOCK_1, INPUT);
	pinMode(AIRLOCK_2, INPUT);
	pinMode(AIRLOCK_3, INPUT);
	pinMode(PHOTO_RESISTOR, INPUT);

	//Initialize RFID and RF24
	SPI.begin();
	mfrc522.PCD_Init();
	delay(4);
	radio.begin();
	network.begin(90, this_node);
	delay(100);

	if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
	{
		Serial.println(F("SSD1306 allocation failed"));
		for (;;);
	}
	display.display();
	v_knob_button_state = digitalRead(VERTICAL_KNOB_SWITCH);
	v_pos = v_encoder.getPosition();
	h_pos = h_encoder.getPosition();

	testOLED();
	//testCells();
	testAirlock();
	testPhotoResistor();
	testEncoders();
	testRFID();
	testNetwork();
}

void loop()
{
	network.update();
	checkNetwork();
	delay(500);
}
