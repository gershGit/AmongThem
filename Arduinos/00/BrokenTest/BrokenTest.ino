//Include statements cover the RFID, NRF24, and ESP Wifi

#include <MFRC522.h>	 //Used for RFID
#include <RF24Network.h> //Used for RF24
#include <RF24.h>		 //Used for RF24
#include <ESP8266WiFi.h> //Used for WiFi
#include <SPI.h>		 //Used for RFID and RF24

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST 5 //D1
#define RFID_SDA 4 //D2
#define RF24_CE 2  //D4
#define RF24_CS 15 //D8

//Sensor Pin definitions
#define MEETING_BUTTON 1 //TX
#define GREEN_LED 0		 //D3
#define RED_LED 9		 //SD2
#define RFID_LED 10		 //SD3
#define ATOMIC_SW A0	 //A0
#define ENCRYPT_SW 16	 //D0
#define LOG_SW 3		 //RX

//Setup of libraries for communication
MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF24_CE, RF24_CS);
RF24Network network(radio);
const uint16_t this_node = 00;
const uint16_t node1 = 01;
const uint16_t node2 = 011;

//Task codes
// 1 -> Encryption
// 2 -> Logging Manifest
// 3 -> Atmoic Clock

int last_node_state = 0;
int node_state = 0;
bool task_complete = false;
int task_num_completed = -1;
bool led_states[3] = {false, false, false};
int led_pins[3] = {GREEN_LED, RED_LED, RFID_LED};
unsigned long led_tick;
unsigned long last_tick;
unsigned long task_start_time;

bool log_status = false;
unsigned long log_start;

bool encrypt_status = false;
unsigned long encrypt_status_start_time;

unsigned long last_meeting;
unsigned long meeting_cooldown = 30;
bool meeting_button = 0;

int clock_count = 0;
bool atomic_button_state = false;
bool atomic_status = false;
unsigned long last_click_time;
bool click_ready = false;

//LED info
#define _GREEN_ 0
#define _RED_ 1
#define _RFID_ 2

struct payload_t
{
	unsigned long msg_type;
	unsigned long msg_code;
	unsigned long uid;
	unsigned long task_code;
};

struct wifi_data_t
{
	char ssid[32];
	char password[32];
	int ip_address[4];
	int password_used;
};

struct player_t
{
	bool active;
	unsigned long uid;
	String country;
	String p_name;
};

player_t players[14] = {
	{false, 1420740908, "usa", "gersh"},
	{false, 2087611938, "cnsa", "kenna"},
	{false, 0, "esa", "-"},
	{false, 0, "iran", "-"},
	{false, 0, "isa", "-"},
	{false, 0, "asi", "-"},
	{false, 0, "kcst", "-"},
	{false, 0, "kari", "-"},
	{false, 0, "isro", "-"},
	{false, 0, "jaxa", "-"},
	{false, 0, "nasa", "-"},
	{false, 0, "cnes", "-"},
	{false, 0, "ssau", "-"},
	{false, 0, "ros", "-"}};
int player_count = 0;

WiFiClient client;
const char *wifi_ssid = "Gersh";
const char *wifi_pass = "gershhome1";
IPAddress server(10, 0, 0, 246);
int port = 8771;
bool wifi_blink = false;

//-----------------------------I/O Functions------------------------------------------------------

bool digitalReadSwitch(int pin) {
	return digitalRead(pin) == LOW;
}

bool analogReadSwitch(int pin) {
	return analogRead(pin) < 200;
}

void clearLEDs()
{
	digitalWrite(GREEN_LED, LOW);
	digitalWrite(RED_LED, LOW);
	digitalWrite(RFID_LED, LOW);
}

void setGameComplete(int task_num)
{
	digitalWrite(RFID_LED, HIGH);
	digitalWrite(GREEN_LED, LOW);
	digitalWrite(RED_LED, LOW);
	task_start_time = millis();
	node_state = 0;
	task_complete = true;
	task_num_completed = task_num;
}

void blinkLED(int pin, int delay)
{
	if (millis() - led_tick > delay || delay == -1)
	{
		led_tick = millis();
		led_states[pin] = !led_states[pin];
		if (led_states[pin] == true)
		{
			digitalWrite(led_pins[pin], HIGH);
		}
		else
		{
			digitalWrite(led_pins[pin], LOW);
		}
	}
}

//-------------------------------------------------------------------------------------------------


//-------------------------Setup and lobby functions-----------------------------------------------
void connectWiFiLoop() {
	WiFi.begin(wifi_ssid, wifi_pass);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		if (wifi_blink == false)
		{
			wifi_blink = true;
			digitalWrite(GREEN_LED, LOW);
		}
		else
		{
			wifi_blink = false;
			digitalWrite(GREEN_LED, HIGH);
		}
	}
	digitalWrite(GREEN_LED, HIGH);
}

void connectServerLoop() {
	while (1)
	{
		if (client.connect(server, port))
		{
			node_state = 99;
			clearLEDs();
			break;
		}
		delay(500);
		if (wifi_blink == false)
		{
			wifi_blink = true;
			digitalWrite(RED_LED, LOW);
		}
		else
		{
			wifi_blink = false;
			digitalWrite(RED_LED, HIGH);
		}
	}
}

void setup() {
	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);
	pinMode(RFID_LED, OUTPUT);
	pinMode(LOG_SW, INPUT);
	pinMode(ATOMIC_SW, INPUT);
	pinMode(MEETING_BUTTON, INPUT);
	pinMode(ENCRYPT_SW, INPUT);

	SPI.begin();
	mfrc522.PCD_Init();
	delay(4);

	radio.begin();
	network.begin(90, this_node);
	delay(100);

	//connectWiFiLoop();
	//connectServerLoop();
	//lobby();
}

//-------------------------------------------------------------------------------------------------


//-------------------------------Game time functions-----------------------------------------------
void handleNetwork(RF24NetworkHeader header, payload_t payload) {

}

void handleWiFi(String message) {

}

payload_t checkNetwork()
{
	while (network.available())
	{
		RF24NetworkHeader header;
		payload_t incomingData;
		network.read(header, &incomingData, sizeof(incomingData));
		handleNetwork(header, incomingData);
		return incomingData;
	}
	return payload_t{-1, -1, -1, -1};
}

String checkWiFi() {
	if (client.available())
	{
		String line = client.readStringUntil('\n');
		return line;
	}
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

bool has_meeting(int uid) {
	//TODO track if the uid still has a meeting
	return true;
}

void checkRFID()
{
	if (mfrc522.PICC_IsNewCardPresent())
	{
		unsigned long uid = getRFID();
		if (uid != 0)
		{
			if (digitalReadSwitch(MEETING_BUTTON) && (millis() - last_meeting > meeting_cooldown) && has_meeting(uid))
			{
				digitalWrite(RFID_LED, LOW);
				//String toSend = "A-Meeting:" + getNameFromUid(uid);
				//client.print(toSend);
				digitalWrite(GREEN_LED, HIGH);
				delay(1000);
				digitalWrite(GREEN_LED, LOW);
				return;
			}
			else if (task_complete) {
				task_complete = false;
				digitalWrite(RFID_LED, LOW);
				//TODO send WiFi message
				digitalWrite(GREEN_LED, HIGH);
				delay(1000);
				digitalWrite(GREEN_LED, LOW);
			}
		}
	}
}

void updateEncryptData()
{
	unsigned long time_diff = millis() - encrypt_status_start_time;
	if (digitalRead(ENCRYPT_SW) == LOW && time_diff < 6000)
	{
		clearLEDs();
		digitalWrite(RED_LED, HIGH);
		delay(500);
		node_state = 0;
	}
	else if (digitalRead(ENCRYPT_SW) == HIGH)
	{
		task_start_time = millis();
		if (time_diff >= 6000)
		{
			setGameComplete(1);
		}
		else
		{
			blinkLED(_RED_, 100);
		}
	}
}

void updateLogManifest()
{
	if (!digitalReadSwitch(LOG_SW))
	{ //Swipe end
		unsigned long diff = millis() - log_start;
		if (diff > 1800 && diff < 2500)
		{
			digitalWrite(GREEN_LED, HIGH);
			delay(500);
			setGameComplete(2);
		}
		else
		{
			node_state = 0;
			digitalWrite(RED_LED, HIGH);
			delay(500);
		}
	}
}

void updateAtomicClock()
{
	if (millis() - last_click_time > 2000 && millis() - last_click_time < 2800)
	{
		digitalWrite(GREEN_LED, LOW);
		click_ready = true;
	}
	else
	{
		digitalWrite(GREEN_LED, HIGH);
		click_ready = false;
	}

	if (analogReadSwitch(ATOMIC_SW)) {
		digitalWrite(RED_LED, HIGH);
	} else {
		digitalWrite(RED_LED, LOW);
	}

	if (analogReadSwitch(ATOMIC_SW) && atomic_button_state == false)
	{ //Next click
		atomic_button_state = true;
		if (click_ready)
		{
			clock_count++;
			task_start_time = millis();
			last_click_time = millis();
			if (clock_count == 4)
			{
				digitalWrite(GREEN_LED, LOW);
				setGameComplete(3);
			}
		}
		else
		{
			clearLEDs();
			digitalWrite(RED_LED, HIGH);
			delay(500);
			node_state = 0;
		}
	}
	else if (analogReadSwitch(ATOMIC_SW) && atomic_button_state == true)
	{
		atomic_button_state = false;
	} else if (millis() - last_click_time > 3000) {
		clearLEDs();
		digitalWrite(RED_LED, HIGH);
		delay(500);
		node_state = 0;
	}
}

void checkStateAbandoned()
{
	if (millis() - task_start_time > 20000)
	{
		task_start_time = millis();
		node_state = 0;
		clearLEDs();
	}
}

void loop() {
	network.update();
	//checkWiFi();
	//checkNetwork();
	checkRFID();

	if (node_state != 1 && digitalRead(ENCRYPT_SW) == HIGH)
	{
		led_tick = millis();
		node_state = 1;
		clearLEDs();
		encrypt_status_start_time = led_tick;
		task_start_time = led_tick;
	}

	if (node_state != 2 && digitalReadSwitch(LOG_SW))
	{
		node_state = 2;
		task_start_time = millis();
		clearLEDs();
		task_complete = false;
		log_start = millis();
	}

	if (node_state != 3 && analogReadSwitch(ATOMIC_SW) && atomic_button_state == false) {
		clock_count = 0;
		node_state = 3;
		task_start_time = millis();
		last_click_time = millis();
		atomic_button_state = true;
		atomic_status = true;
		clearLEDs();
	}

	if (node_state == 1) {
		updateEncryptData();
	} else if (node_state == 2) {
		updateLogManifest();
	} else if (node_state == 3) {
		digitalWrite(RFID_LED, HIGH);
		updateAtomicClock();
	} else {
		digitalWrite(RFID_LED, LOW);
	}

	checkStateAbandoned();
	if (task_complete || digitalReadSwitch(MEETING_BUTTON)) {
		digitalWrite(RFID_LED, HIGH);
	} else {
		digitalWrite(RFID_LED, LOW);
	}
}