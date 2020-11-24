#include <DHT.h>
#include <MFRC522.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

//Pin declarations for physical tasks
#define DOWNLOAD_SW 2
#define HEARTBEAT_SW 3
#define REPRESSURIZE_SW 4
#define VENT_SW 6
#define ACCEPT_POWER 5
#define LOCK_OXYGEN_CAP A0
#define TEMPERATURE_PIN A1
#define TEMPERATURE_BEGIN A2
#define LED_GREEN_PIN A3
#define LED_BLUE_PIN A4
#define LED_RED_PIN A5
#define RF_CS 10
#define RF_CE 9
#define RFID_RST 7
#define RFID_SDA 8

int task_ids[10] = {0, 011, 012, 013, 014, 015, 016, 017, 018, 019};

#define DHTTYPE DHT11
DHT dht(TEMPERATURE_PIN, DHTTYPE);
MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF_CE, RF_CS);
RF24Network network(radio);
const uint16_t this_node = 01;
const uint16_t master00 = 00;

struct payload_t
{
	unsigned long msg_type;
	unsigned long msg_code;
	unsigned long uid;
	unsigned long task_code;
};

#define GREEN_LED 0
#define RED_LED 1
#define BLUE_LED 2

//Nodewide variables
int last_node_state = 0;
int node_state = 0;
bool task_complete = false;
int task_num_completed = -1;
bool led_states[3] = {false, false, false};
int led_pins[3] = {LED_GREEN_PIN, LED_RED_PIN, LED_BLUE_PIN};
unsigned long led_tick;
unsigned long last_tick;
unsigned long task_start_time;

//Variables for temperature task
float base_temp = 0;
bool temp_begin_state = false;

//Variables for download task
int dl_count = 0;
bool download_begin_state = false;

//Variables for heartbeat task
int hb_count = 0;
bool hb_state = false;

//Variables for pressurize
int pressure_count = 0;
bool pressure_state = false;

//Variables for vent
int vent_count = 0;
bool vent_state = false;
bool venting = false;
int vent_stage = 0;

//Power waiting
bool power_waiting = true;
bool accept_state = false;

//Variables for oxygen lock
int lock_count = 0;
bool lock_state = false;

void blinkLED(int pin, int delay) {
	if (millis() - led_tick > delay || delay == -1) {
		led_tick = millis();
		led_states[pin] = !led_states[pin];
		if (led_states[pin] == true) {
			digitalWrite(led_pins[pin], HIGH);
		} else {
			digitalWrite(led_pins[pin], LOW);
		}
	}
}

bool readSwitchDigital(int pin)
{
	return digitalRead(pin) == LOW;
}

void setGameComplete(int task_num) {
	digitalWrite(LED_BLUE_PIN, HIGH);
	digitalWrite(LED_GREEN_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
	task_start_time = millis();
	node_state = 0;
	task_complete = true;
	task_num_completed = task_num;
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
		if (uid != -1 && task_complete)
		{
			Serial.print("Sending task complete.");
			task_complete = false;
			digitalWrite(LED_BLUE_PIN, LOW);
			RF24NetworkHeader header_00(master00);
			payload_t payload = {1, 1, uid, task_ids[task_num_completed]};
			bool ok = network.write(header_00, &payload, sizeof(payload));
			while (!ok)
			{
				Serial.print(".");
				ok = network.write(header_00, &payload, sizeof(payload));
				delay(250);
			}
			Serial.println("ok.");
			digitalWrite(LED_GREEN_PIN, HIGH);
			delay(1000);
			digitalWrite(LED_GREEN_PIN, LOW);
		}
	}
}

payload_t checkNetwork()
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
		Serial.print("\tTask #: ");
		Serial.println(incomingData.task_code);
		Serial.print("\tUID: ");
		Serial.println(incomingData.uid);
		return incomingData;
	}
	return payload_t{-1, -1, -1, -1};
}

void connectToServer() {
	led_tick = millis();
	Serial.print("Connecting to server.");
	RF24NetworkHeader header_00(master00);
	payload_t payload = {99, 1, 0, 0};
	bool ok = network.write(header_00, &payload, sizeof(payload));
	while (!ok)
	{
		network.update();
		Serial.print(".");
		ok = network.write(header_00, &payload, sizeof(payload));
		delay(2000);
		blinkLED(RED_LED, -1);
	}
	Serial.println("ok.");
	digitalWrite(LED_GREEN_PIN, HIGH);
	return;
}

void resetHub() {
	node_state = 0;
	task_num_completed = 0;
	task_start_time = millis();
	task_complete = false;
	last_tick = millis();
	digitalWrite(LED_GREEN_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
	digitalWrite(LED_BLUE_PIN, LOW);
	delay(1000);
}

void waitForStart() {
	Serial.print("Waiting for start.");
	led_tick = millis();
	while (true) {
		network.update();
		while (network.available()) {
			RF24NetworkHeader header;
			payload_t incomingData;
			network.read(header, &incomingData, sizeof(payload_t));
			if (incomingData.msg_type == 99 && incomingData.msg_code == 0) {
				resetHub();
				Serial.println("Starting.");
				return;
			}
		}
		delay(1000);
		blinkLED(RED_LED, -1);
	}
}

void waitForRFID() {
	while(true) {
		if (mfrc522.PICC_IsNewCardPresent())
		{
			unsigned long uid = getRFID();
			if (uid != -1)
			{
				digitalWrite(LED_BLUE_PIN, HIGH);
				return;
			}
		}
		delay(250);
	}
}

void setup()
{
	Serial.begin(115200);
	pinMode(DOWNLOAD_SW, INPUT);
	pinMode(HEARTBEAT_SW, INPUT);
	pinMode(REPRESSURIZE_SW, INPUT);
	pinMode(TEMPERATURE_PIN, INPUT);
	pinMode(TEMPERATURE_BEGIN, INPUT);
	pinMode(ACCEPT_POWER, INPUT);
	pinMode(DOWNLOAD_SW, INPUT);
	pinMode(LED_GREEN_PIN, OUTPUT);
	pinMode(LED_BLUE_PIN, OUTPUT);
	pinMode(LED_RED_PIN, OUTPUT);

	dht.begin();
	SPI.begin();
	mfrc522.PCD_Init();
	delay(4);

	radio.begin();
	network.begin(90, this_node);
	delay(100);

	Serial.println("Initialization complete");

	waitForRFID();
	connectToServer();
	waitForStart();
}

void startTempSensor()
{
	digitalWrite(LED_BLUE_PIN, LOW);
	task_start_time = millis();
	node_state = 1;
	base_temp = dht.readTemperature();
	task_complete = false;
	last_tick = millis();
}

void updateTempSensor()
{
	float current_temp = dht.readTemperature();
	if (millis() - last_tick > 250)
	{
		last_tick = millis();
		led_states[GREEN_LED] = !led_states[GREEN_LED];
		if (led_states[GREEN_LED])
		{
			digitalWrite(LED_GREEN_PIN, HIGH);
		}
		else
		{
			digitalWrite(LED_GREEN_PIN, LOW);
		}
	}
	if (current_temp > base_temp + 0.4)
	{
		setGameComplete(1);
	}
}

void startDownloadTask()
{
	task_start_time = millis();
	node_state = 2;
	dl_count = 0;
	digitalWrite(LED_BLUE_PIN, LOW);
	digitalWrite(LED_GREEN_PIN, LOW);
	last_tick = millis();
}

void updateDownloadTask()
{
	if (millis() - last_tick > 100)
	{
		task_start_time = millis();
		last_tick = millis();
		dl_count++;
		led_states[RED_LED] = !led_states[RED_LED];
		if (led_states[RED_LED])
		{
			digitalWrite(LED_RED_PIN, HIGH);
		}
		else
		{
			digitalWrite(LED_RED_PIN, LOW);
		}
	}
	if (dl_count >= 100)
	{
		setGameComplete(2);
	}
}

void startHeartBeatTask()
{
	digitalWrite(LED_BLUE_PIN, LOW);
	task_start_time = millis();
	hb_count = 0;
	node_state = 3;
	last_tick = millis();
	digitalWrite(LED_GREEN_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
}

void updateHeartBeatTask()
{
	if (millis() - last_tick > 750 && millis() - last_tick < 1750)
	{
		digitalWrite(LED_GREEN_PIN, HIGH);
		if (readSwitchDigital(HEARTBEAT_SW))
		{
			task_start_time = millis();
			digitalWrite(LED_GREEN_PIN, LOW);
			last_tick = millis();
			hb_count++;
			if (hb_count == 10)
			{
				setGameComplete(3);
			}
		}
	}
	else if (millis() - last_tick > 1750)
	{
		digitalWrite(LED_RED_PIN, HIGH);
		digitalWrite(LED_GREEN_PIN, LOW);
		delay(2000);
		hb_count = 0;
		node_state = 3;
		last_tick = millis();
		digitalWrite(LED_GREEN_PIN, LOW);
		digitalWrite(LED_RED_PIN, LOW);
	}
	else
	{
		digitalWrite(LED_GREEN_PIN, LOW);
		if (millis() - last_tick > 250 && readSwitchDigital(HEARTBEAT_SW))
		{
			task_start_time = millis();
			digitalWrite(LED_RED_PIN, HIGH);
			digitalWrite(LED_GREEN_PIN, LOW);
			delay(2000);
			hb_count = 0;
			node_state = 3;
			last_tick = millis();
			digitalWrite(LED_GREEN_PIN, LOW);
			digitalWrite(LED_RED_PIN, LOW);
		}
	}
}

void startPressurizeTask()
{
	digitalWrite(LED_BLUE_PIN, LOW);
	task_start_time = millis();
	pressure_count = 0;
	node_state = 4;
	last_tick = millis();
}

void updatePressurizeTask()
{
	if (!readSwitchDigital(REPRESSURIZE_SW))
	{
		pressure_count = 0;
		digitalWrite(LED_RED_PIN, HIGH);
		digitalWrite(LED_GREEN_PIN, LOW);
	}
	else if (millis() - last_tick > 100)
	{
		task_start_time = millis();
		digitalWrite(LED_RED_PIN, LOW);
		digitalWrite(LED_GREEN_PIN, HIGH);
		last_tick = millis();
		pressure_count++;
		if (pressure_count >= 70)
		{
			setGameComplete(4);
		}
	}
}

void startVentTask()
{
	digitalWrite(LED_BLUE_PIN, LOW);
	task_start_time = millis();
	vent_count = 0;
	node_state = 5;
	last_tick = millis();
	venting = true;
	vent_stage = 0;
}

void updateVentTask()
{
	if (!readSwitchDigital(VENT_SW) && venting)
	{
		vent_count = 0;
		last_tick = millis();
		venting = false;
		vent_state = false;
		digitalWrite(LED_RED_PIN, LOW);
		digitalWrite(LED_GREEN_PIN, LOW);
		delay(1000);
		digitalWrite(LED_RED_PIN, HIGH);
		digitalWrite(LED_GREEN_PIN, HIGH);
		delay(500);
	}
	else if (readSwitchDigital(VENT_SW) && !venting && vent_state == false)
	{
		venting = true;
		task_start_time = millis();
	}
	else if (!readSwitchDigital(VENT_SW) && !venting && vent_state == true)
	{
		vent_state = false;
	}
	else if (readSwitchDigital(VENT_SW) && millis() - last_tick > 100 && venting)
	{
		task_start_time = millis();
		last_tick = millis();
		vent_count++;
		vent_state = true;
		if (vent_stage == 0)
		{
			digitalWrite(LED_RED_PIN, HIGH);
			digitalWrite(LED_GREEN_PIN, HIGH);
		}
		else
		{
			digitalWrite(LED_RED_PIN, LOW);
			digitalWrite(LED_GREEN_PIN, HIGH);
		}
		if (vent_count >= 60)
		{
			vent_stage++;
			vent_count = 0;
			venting = false;
			digitalWrite(LED_RED_PIN, LOW);
			digitalWrite(LED_GREEN_PIN, HIGH);
			if (vent_stage == 2)
			{
				setGameComplete(5);
			}
		}
	}
}

void attemptAccept()
{
	task_start_time = millis();
	digitalWrite(LED_BLUE_PIN, LOW);
	node_state = 6;
	task_start_time = millis();
	if (power_waiting)
	{
		power_waiting = false;
		setGameComplete(6);
	}
	else
	{
		digitalWrite(LED_GREEN_PIN, LOW);
		digitalWrite(LED_BLUE_PIN, LOW);
		digitalWrite(LED_RED_PIN, HIGH);
	}
}

void startLockTanks()
{
	digitalWrite(LED_BLUE_PIN, LOW);
	node_state = 7;
	lock_count = 0;
	task_start_time = millis();
}

void updateLockTanks()
{
	if (analogRead(LOCK_OXYGEN_CAP) > 200 && millis() - last_tick > 100)
	{
		task_start_time = millis();
		digitalWrite(LED_GREEN_PIN, HIGH);
		digitalWrite(LED_RED_PIN, LOW);
		last_tick = millis();
		lock_count++;
		if (lock_count >= 60)
		{
			setGameComplete(7);
		}
	}
}

void loop()
{
	network.update();
	checkRFID();
	payload_t payload = checkNetwork();
	if (!payload.msg_type == -1)
	{
		if (payload.msg_type == 80)
		{
			power_waiting = true;
		}
		else if (payload.msg_type = 70)
		{
			resetHub();
			waitForStart();
		}
	}

	if (node_state != last_node_state)
	{
		Serial.print("Node state: ");
		Serial.println(node_state);
		last_node_state = node_state;
	}

	//Check on temperature task
	if (analogRead(TEMPERATURE_BEGIN) > 150 && temp_begin_state == false)
	{
		temp_begin_state = true;
		startTempSensor();
	}
	else if (analogRead(TEMPERATURE_BEGIN) < 150 && temp_begin_state == true)
	{
		temp_begin_state = false;
	}
	if (node_state == 1)
	{
		updateTempSensor();
	}

	//Check on download task
	if (readSwitchDigital(DOWNLOAD_SW) && !download_begin_state && node_state != 2 && millis()-last_tick > 150)
	{
		download_begin_state = true;
		startDownloadTask();
	}
	else if (!readSwitchDigital(DOWNLOAD_SW) && download_begin_state)
	{
		download_begin_state = false;
	}
	if (node_state == 2)
	{
		updateDownloadTask();
	}

	//Check on heartbeat task
	if (readSwitchDigital(HEARTBEAT_SW) && !hb_state && node_state != 3 && millis() - last_tick > 150)
	{
		hb_state = true;
		startHeartBeatTask();
	}
	else if (!readSwitchDigital(HEARTBEAT_SW) && hb_state)
	{
		hb_state = false;
	}
	if (node_state == 3)
	{
		updateHeartBeatTask();
	}

	if (readSwitchDigital(REPRESSURIZE_SW) && !pressure_state && node_state != 4 && millis() - last_tick > 150)
	{
		pressure_state = true;
		startPressurizeTask();
	}
	else if (!readSwitchDigital(REPRESSURIZE_SW) && pressure_state)
	{
		pressure_state = false;
	}
	if (node_state == 4)
	{
		updatePressurizeTask();
	}

	if (readSwitchDigital(VENT_SW) && !vent_state && node_state != 5 && millis() - last_tick > 150)
	{
		Serial.println("Starting vent task");
		vent_state = true;
		startVentTask();
	}
	else if (!readSwitchDigital(VENT_SW) && vent_state)
	{
		vent_state = false;
	}
	if (node_state == 5)
	{
		updateVentTask();
	}

	if (readSwitchDigital(ACCEPT_POWER) && !accept_state)
	{
		accept_state = true;
		attemptAccept();
	}
	else if (!readSwitchDigital(ACCEPT_POWER) && accept_state)
	{
		accept_state = false;
	}

	if (analogRead(LOCK_OXYGEN_CAP) > 200 && !lock_state && node_state != 7 && millis() - last_tick > 150)
	{
		lock_state = true;
		startLockTanks();
	}
	else if (analogRead(LOCK_OXYGEN_CAP) < 200 && lock_state)
	{
		lock_state = false;
	}
	if (node_state == 7)
	{
		updateLockTanks();
	}

	if (millis() - task_start_time > 25000)
	{
		Serial.println("Resetting");
		task_start_time = millis();
		node_state = 0;
		digitalWrite(LED_RED_PIN, LOW);
		digitalWrite(LED_GREEN_PIN, LOW);
		digitalWrite(LED_BLUE_PIN, LOW);
	}
}
