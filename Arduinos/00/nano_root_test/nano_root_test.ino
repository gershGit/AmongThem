//Include statements cover the RFID, NRF24, and ESP Wifi
#include <RF24Network.h> //Used for RF24
#include <RF24.h>		 //Used for RF24
#include <SPI.h>		 //Used for RFID and RF24

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST 7
#define RFID_SDA 8
#define RF24_CE 9
#define RF24_CS 10

//Sensor Pin definitions
#define GREEN_LED 2

//Setup of libraries for communication
//MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF24_CE, RF24_CS);
RF24Network network(radio);
const uint16_t this_node = 00;
const uint16_t node1 = 01;

unsigned long last_tick;

//Node status
#define NODE_COUNT 2
bool node_status[2] = {true, false};
const uint16_t node_addresses[2] = {00, 01};

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

void resetHub() {
	last_tick = millis();
}

int registerNode(int node_id) {
	//Serial.print("Registering node: ");
	//Serial.println(node_id);
	int reg_count = 0;
	node_status[node_id] = true;
	for (int i = 0; i < NODE_COUNT; i++) {
		if (node_status[i] == true) {
			reg_count++;
		}
	}
	return reg_count;
}

void waitForConnections(int conn_needed) {
	//Serial.println("Waiting for connections");
	while (true) {
		network.update();
		message_t message = checkNetwork();
		if (message.payload.msg_type == -1) {
			delay(500);
			continue;
		}
		if (message.payload.msg_type == 99) {
			int reg_count = registerNode(message.payload.msg_code);
			RF24NetworkHeader temp_header(message.header.from_node);
			network.write(temp_header, &message.payload, sizeof(message.payload));

			//Update server on node registration
			String toSend = String("R:");
			toSend += String(message.payload.msg_code);
			toSend += String(":") + reg_count;
			Serial.println(toSend);
			if (reg_count >= conn_needed)
			{
				return;
			}
		}
	}
}

void broadcast(payload_t payload) {
	//Serial.println("Broadcasting");
	for (int i = 1; i < NODE_COUNT; i++) {
		RF24NetworkHeader temp_header(node_addresses[i]);
		while (!network.write(temp_header, &payload, sizeof(payload))) {
			delay(250);
		}
	}
}

void startAfter(int seconds) {
	//Serial.println("Start countdown begin");
	delay(seconds * 1000);
	payload_t payload = {80, 0, 0, 0};
	broadcast(payload);
	resetHub();
	//Serial.println("Starting...");
	return;
}

void waitForStart() {
	while (true) {
		network.update();
		message_t message = checkNetwork();
		if (message.valid)
		{
			switch (message.payload.msg_type)
			{
			case 0:
				break;
			case 1:
				sendTask(message);
				break;
			case 99:
			{
				int reg_count = registerNode(message.payload.msg_code);
				RF24NetworkHeader temp_header(message.header.from_node);
				network.write(temp_header, &message.payload, sizeof(message.payload));
				String toSend = String("R:");
				toSend += String(message.payload.msg_code);
				toSend += String(":") + reg_count;
				Serial.println(toSend);
				break;
			}
			default:
				break;
			}
		}
		if (Serial.available()) {
			String data = Serial.readStringUntil('\n');
			if (strcmp(data.c_str(), "Start") == 0) {
				return;
			}
		}
		delay(100);
	}
}

void setup()
{
	//Begin debugger
	Serial.begin(115200);
	delay(250);
	//Serial.println("Initializing RFID and RF24");

	//Set pin modes
	//Serial.println("Initializing pins");
	pinMode(LED_BUILTIN, OUTPUT);

	//Initialize RFID and RF24
	SPI.begin();
	//mfrc522.PCD_Init();
	radio.begin();
	network.begin(90, this_node);
	delay(100);

	network.update();
	waitForStart();
}

message_t checkNetwork()
{
	while (network.available())
	{
		RF24NetworkHeader header;
		payload_t incomingData;
		network.read(header, &incomingData, sizeof(incomingData));
		/*
		Serial.println("Received payload: ");
		Serial.print("\tType: ");
		Serial.println(incomingData.msg_type);
		Serial.print("\tCode: ");
		Serial.println(incomingData.msg_code);
		Serial.print("\tTask: ");
		Serial.println(incomingData.task_code);
		Serial.print("\tUID: ");
		Serial.println(incomingData.uid);
		*/
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

void sendTask(message_t message){
	String toSend = String("T:");
	toSend += String(message.payload.uid);
	toSend += String(":");
	toSend += String(message.payload.task_code);
	toSend += String(":");
	//message += String(message.payload.msg_code);
	toSend += String("0");
	Serial.println(toSend);
}

void loop()
{
	network.update();
	message_t message = checkNetwork();
	if (message.valid) {
		switch(message.payload.msg_type) {
			case 0:
				break;
			case 1:
				sendTask(message);
				break;
			case 99:
			{
				int reg_count = registerNode(message.payload.msg_code);
				RF24NetworkHeader temp_header(message.header.from_node);
				network.write(temp_header, &message.payload, sizeof(message.payload));
				String toSend = String("R:");
				toSend += String(message.payload.msg_code);
				toSend += String(":") + reg_count;
				Serial.println(toSend);
				break;
			}
			default:
				break;
		}
	}
	if (Serial.available()) {
		String data = Serial.readStringUntil('\n');
	}
	delay(500);
}
