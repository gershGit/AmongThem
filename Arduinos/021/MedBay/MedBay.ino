#include <MFRC522.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

//Pin declarations for physical tasks
#define PUPILS_SW 6
#define BLOOD_CAP A6
#define DL_SW A7
#define MRI_CONTACT A3
#define ACCEPT_POWER 4
#define VITALS_L A0
#define PR_PIN A1
#define VITALS_R A2
#define DEP_TRIG 2
#define DEP_ECHO 3
#define DEP_SW A5
#define LED_RED_PIN 5
#define LED_BLUE_PIN A4
#define RF_CS 10
#define RF_CE 9
#define RFID_RST 7
#define RFID_SDA 8

int task_ids[10] = {0, 211, 212, 213, 214, 215, 216, 217, 218, 219};

#define DHTTYPE DHT11
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

#define RED_LED 0
#define BLUE_LED 1

//Nodewide variables
int last_node_state = 0;
int node_state = 0;
bool task_complete = false;
int task_num_completed = -1;
bool led_states[2] = {false, false};
int led_pins[2] = {LED_RED_PIN, LED_BLUE_PIN};
unsigned long led_tick;
unsigned long last_tick;
unsigned long task_start_time;

//Variables for download task
int dl_count = 0;
bool download_begin_state = false;

//Variables for vitals task
int vitals_count = 0;
bool vital_l_state = false;
bool vital_r_state = false;

//Variables for depth task
float base_depth = 0;
int depth_required = 0;
bool depth_pressed = false;
int depth_count = 0;

//Variables for vision check
int pupil_pos = 0;
int wait_count = 0;
bool pupil_pressed = false;
bool reverse_vision = false;
float base_light = 0;
int tick_needed = 0;
bool vision_ready = false;

//Power waiting
bool power_waiting = true;
bool accept_state = false;

//Variables for blood accept
int blood_count = 0;
bool blood_state = false;

//Variables for MRI
bool MRI_connected = false;

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

bool readSwitchAnalog(int pin) {
	return analogRead(pin) < 200;
}

void setGameComplete(int task_num) {
	digitalWrite(LED_BLUE_PIN, HIGH);
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
			digitalWrite(LED_BLUE_PIN, LOW);
			delay(500);
			digitalWrite(LED_BLUE_PIN, HIGH);
			delay(500);
			digitalWrite(LED_BLUE_PIN, LOW);
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
	return;
}

void resetHub() {
	node_state = 0;
	task_num_completed = 0;
	task_start_time = millis();
	task_complete = false;
	last_tick = millis();
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

void setup()
{
	Serial.begin(115200);
	pinMode(DL_SW, INPUT);
	pinMode(PR_PIN, INPUT);
	pinMode(ACCEPT_POWER, INPUT);
	pinMode(PUPILS_SW, INPUT);
	pinMode(DEP_SW, INPUT);
	pinMode(LED_BLUE_PIN, OUTPUT);
	pinMode(LED_RED_PIN, OUTPUT);
	pinMode(DEP_TRIG, OUTPUT);
	pinMode(DEP_ECHO, INPUT);

	SPI.begin();
	mfrc522.PCD_Init();
	delay(4);

	radio.begin();
	network.begin(90, this_node);
	delay(100);

	Serial.println("Initialization complete");
	connectToServer();
	waitForStart();
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
		setGameComplete(3);
	}
	else
	{
		digitalWrite(LED_BLUE_PIN, LOW);
		digitalWrite(LED_RED_PIN, HIGH);
	}
}

void startVisionTask() {
	Serial.println("Starting vision task");
	analogWrite(LED_RED_PIN, 0);
	node_state = 1;
	task_start_time = millis();
	base_light = analogRead(PR_PIN);
	task_complete = false;
	last_tick = task_start_time;
	reverse_vision = false;
	vision_ready = false;
	randomSeed(last_tick);
	pupil_pos = 0;
	tick_needed = random(10, 20);
}

void updateVisionTask() {
	unsigned long now = millis();
	if (now-last_tick > 500) {
		last_tick = now;
		if (pupil_pos == tick_needed && wait_count > 3)  {
			pupil_pos--;
			reverse_vision = true;
			wait_count = 0;
		} else if (pupil_pos != tick_needed) {
			if (!reverse_vision) {
				pupil_pos++;
			} else {
				pupil_pos--;
			}
		} else if (pupil_pos == tick_needed) {
			wait_count++;
			Serial.print("Wait count: ");
			Serial.println(wait_count);
		}
		if (pupil_pos <= 0) {
			reverse_vision = false;
		}
		int brightness = (float(pupil_pos) / float(tick_needed)) * 255;
		analogWrite(LED_RED_PIN, brightness);
	}
	if (pupil_pos == tick_needed) {
		vision_ready = true;
	} else {
		vision_ready = false;
	}

	if (analogRead(PR_PIN) > base_light+300 && !vision_ready) {
		digitalWrite(LED_RED_PIN, HIGH);
		delay(250);
		digitalWrite(LED_RED_PIN, LOW);
		delay(250);
		digitalWrite(LED_RED_PIN, HIGH);
		delay(250);
		digitalWrite(LED_RED_PIN, LOW);
		delay(250);
		digitalWrite(LED_RED_PIN, HIGH);
		delay(250);
		digitalWrite(LED_RED_PIN, LOW);
		startVisionTask();
	}
	else if (analogRead(PR_PIN) > base_light + 300) {
		setGameComplete(1);
	}
}

void startDepthTask() {
	Serial.println("Starting depth task");
	analogWrite(LED_RED_PIN, 0);
	node_state = 2;
	task_start_time = millis();
	task_complete = false;
	last_tick = task_start_time;
	depth_count = 0;
	randomSeed(last_tick);
	digitalWrite(LED_RED_PIN, LOW);
	depth_required = random(5, 20);
}

void updateDepthTask() {
	unsigned long now = millis();
	if (now-last_tick > 250) {
		last_tick = now;
		digitalWrite(DEP_TRIG, LOW);
		delayMicroseconds(2);
		digitalWrite(DEP_TRIG, HIGH);
		delayMicroseconds(10);
		digitalWrite(DEP_TRIG, LOW);
		long duration = pulseIn(DEP_ECHO, HIGH);
		int distance = duration * 0.034 / 2;
		if (distance > depth_required-2 && distance < depth_required+2) {
			digitalWrite(LED_RED_PIN, HIGH);
			depth_count++;
			if (depth_count >= 16) {
				setGameComplete(2);
			}
		} else {
			digitalWrite(LED_RED_PIN, LOW);
		}
	}
}

void startDownloadTask()
{
	task_start_time = millis();
	node_state = 2;
	dl_count = 0;
	task_complete = false;
	digitalWrite(LED_BLUE_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
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
		setGameComplete(4);
	}
}

void startVitalsTask() {
	vitals_count = 0;
	task_start_time = millis();
	last_tick = task_start_time;
	task_complete = false;
	node_state = 5;
	digitalWrite(LED_BLUE_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
}

void updateVitalsTask() {
	if (readSwitchAnalog(VITALS_L) || readSwitchAnalog(VITALS_R))
	{
		vitals_count = 0;
		digitalWrite(LED_RED_PIN, LOW);
		delay(750);
		digitalWrite(LED_RED_PIN, HIGH);
		delay(750);
		digitalWrite(LED_RED_PIN, LOW);
	}
	else if (millis() - last_tick > 100)
	{
		task_start_time = millis();
		digitalWrite(LED_RED_PIN, HIGH);
		last_tick = millis();
		vitals_count++;
		if (vitals_count >= 50)
		{
			setGameComplete(5);
		}
	}
}

void startBloodTask()
{
	blood_count = 0;
	task_start_time = millis();
	last_tick = task_start_time;
	task_complete = false;
	node_state = 6;
	digitalWrite(LED_BLUE_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
}

void updateBloodTask()
{
	if (readSwitchAnalog(BLOOD_CAP))
	{
		blood_count = 0;
		digitalWrite(LED_RED_PIN, LOW);
		delay(750);
		digitalWrite(LED_RED_PIN, HIGH);
		delay(750);
		digitalWrite(LED_RED_PIN, LOW);
	}
	else if (millis() - last_tick > 100)
	{
		task_start_time = millis();
		digitalWrite(LED_RED_PIN, HIGH);
		last_tick = millis();
		blood_count++;
		if (blood_count >= 50)
		{
			setGameComplete(6);
		}
	}
}

void loop()
{
	network.update();
	checkRFID();
	payload_t payload = checkNetwork();
	if (!payload.msg_type == -1) {
		if (payload.msg_type == 80) {
			power_waiting = true;
		} else if (payload.msg_type = 70) {
			resetHub();
			waitForStart();
		}
	}

	//Check on reaction task (1)
	if (readSwitchDigital(PUPILS_SW) && !pupil_pressed) {
		pupil_pressed = true;
		startVisionTask();
	}
	else if (!readSwitchDigital(PUPILS_SW) && pupil_pressed)
	{
		pupil_pressed = false;
	}
	if (node_state == 1) {
		updateVisionTask();
	}

	//Check on depth task (2)
	if (readSwitchAnalog(DEP_SW) && !depth_pressed)
	{
		depth_pressed = true;
		startDepthTask();
	}
	else if (!readSwitchAnalog(DEP_SW) && depth_pressed)
	{
		depth_pressed = false;
	}
	if (node_state == 2)
	{
		updateDepthTask();
	}

	//Check on accept power task (3)
	if (readSwitchDigital(ACCEPT_POWER) && !accept_state)
	{
		accept_state = true;
		attemptAccept();
	}
	else if (!readSwitchDigital(ACCEPT_POWER) && accept_state)
	{
		accept_state = false;
	}

	//Check on vitals task (5)
	if (!readSwitchAnalog(VITALS_L) && !blood_state && node_state != 5 && millis() - last_tick > 1000 && !readSwitchAnalog(VITALS_R) && !vital_r_state)
	{
		vital_l_state = true;
		vital_r_state = true;
		startVitalsTask();
	}
	else if (readSwitchAnalog(VITALS_L) && vital_l_state && readSwitchAnalog(VITALS_R) && vital_r_state)
	{
		vital_l_state = false;
		vital_r_state = false;
	}
	if (node_state == 5)
	{
		updateVitalsTask();
	}

	//Check on blood sample task (6)
	if (!readSwitchAnalog(BLOOD_CAP) && !blood_state && node_state != 6 && millis() - last_tick > 1000)
	{
		blood_state = true;
		startBloodTask();
	}
	else if (readSwitchAnalog(BLOOD_CAP) && blood_state)
	{
		blood_state = false;
	}
	if (node_state == 6)
	{
		updateBloodTask();
	}

	if (millis() - task_start_time > 25000)
	{
		Serial.println("Resetting");
		task_start_time = millis();
		node_state = 0;
		digitalWrite(LED_RED_PIN, LOW);
		digitalWrite(LED_BLUE_PIN, LOW);
	}
}
