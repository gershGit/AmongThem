#include <DHT.h>

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

#define DHTTYPE DHT11
DHT dht(TEMPERATURE_PIN, DHTTYPE);

#define GREEN_LED 0
#define RED_LED 0
#define BLUE_LED 0

//Nodewide variables
int last_node_state = 0;
int node_state = 0;
bool task_complete = false;
int task_num_completed = -1;
bool led_states[3] = {false, false, false};
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

bool readSwitchDigital(int pin)
{
	return digitalRead(pin) == LOW;
}

void setGameComplete(int task_num) {
	digitalWrite(LED_BLUE_PIN, HIGH);
	digitalWrite(LED_GREEN_PIN, LOW);
	digitalWrite(LED_RED_PIN, LOW);
	node_state = 0;
	task_complete = true;
	task_num_completed = task_num;
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
	last_tick = millis();
	delay(1000);
}


void startTempSensor() {
	digitalWrite(LED_BLUE_PIN, LOW);
	task_start_time = millis();
	node_state = 1;
	base_temp = dht.readTemperature();
	task_complete = false;
	last_tick = millis();
}

void updateTempSensor() {
	float current_temp = dht.readTemperature();
	if (millis() - last_tick > 250) {
		last_tick = millis();
		led_states[GREEN_LED] = !led_states[GREEN_LED];
		if (led_states[GREEN_LED]) {
			digitalWrite(LED_GREEN_PIN, HIGH);
		} else {
			digitalWrite(LED_GREEN_PIN, LOW);
		}
	}
	if (current_temp > base_temp + 0.4) {
		setGameComplete(1);
	}
}

void startDownloadTask() {
	task_start_time = millis();
	node_state = 2;
	dl_count = 0;
	digitalWrite(LED_BLUE_PIN, LOW);
	digitalWrite(LED_GREEN_PIN, LOW);
	last_tick = millis();
}

void updateDownloadTask() {
	if (millis() - last_tick > 100) {
		last_tick = millis();
		dl_count++;
		led_states[RED_LED] = !led_states[RED_LED];
		if (led_states[RED_LED]) {
			digitalWrite(LED_RED_PIN, HIGH);
		} else {
			digitalWrite(LED_RED_PIN, LOW);
		}
	}
	if (dl_count >= 100) {
		setGameComplete(2);
	}
}

void startHeartBeatTask() {
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
	if (millis() - last_tick > 750 && millis() - last_tick < 1250)
	{
		digitalWrite(LED_GREEN_PIN, HIGH);
		if (readSwitchDigital(HEARTBEAT_SW))
		{
			digitalWrite(LED_GREEN_PIN, LOW);
			last_tick = millis();
			hb_count++;
			if (hb_count == 10)
			{
				setGameComplete(3);
			}
		}
	}
	else if (millis() - last_tick > 1250)
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

void startPressurizeTask() {
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

void startVentTask() {
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
	}
	else if (!readSwitchDigital(VENT_SW) && !venting && vent_state == true)
	{
		vent_state = false;
	}
	else if (readSwitchDigital(VENT_SW) && millis() - last_tick > 100 && venting)
	{
		last_tick = millis();
		vent_count++;
		vent_state = true;
		if (vent_stage == 0) {
			digitalWrite(LED_RED_PIN, HIGH);
			digitalWrite(LED_GREEN_PIN, HIGH);
		} else {
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

void attemptAccept() {
	digitalWrite(LED_BLUE_PIN, LOW);
	node_state = 6;
	task_start_time = millis();
	if (power_waiting) {
		power_waiting = false;
		setGameComplete(6);
	} else {
		digitalWrite(LED_GREEN_PIN, LOW);
		digitalWrite(LED_BLUE_PIN, LOW);
		digitalWrite(LED_RED_PIN, HIGH);
	}
}

void startLockTanks() {
	digitalWrite(LED_BLUE_PIN, LOW);
	node_state = 7;
	lock_count = 0;
	task_start_time = millis();
}

void updateLockTanks() {
	if (analogRead(LOCK_OXYGEN_CAP) > 200 && millis()-last_tick > 100 ) {
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
	if (node_state != last_node_state) {
		Serial.print("Node state: ");
		Serial.println(node_state);
		last_node_state = node_state;
	}

	//Check on temperature task
	if (analogRead(TEMPERATURE_BEGIN) > 150 && temp_begin_state == false) {
		temp_begin_state = true;
		startTempSensor();
	}
	else if (analogRead(TEMPERATURE_BEGIN) < 150 && temp_begin_state == true)
	{
		temp_begin_state = false;
	}
	if (node_state == 1) {
		updateTempSensor();
	}

	//Check on download task
	if (readSwitchDigital(DOWNLOAD_SW) && !download_begin_state)
	{
		download_begin_state = true;
		startDownloadTask();
	}
	else if (!readSwitchDigital(DOWNLOAD_SW) && download_begin_state)
	{
		download_begin_state = false;
	}
	if (node_state == 2) {
		updateDownloadTask();
	}

	//Check on heartbeat task
	if (readSwitchDigital(HEARTBEAT_SW) && !hb_state && node_state != 3 && millis()- last_tick > 1000)
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

	if (readSwitchDigital(REPRESSURIZE_SW) && !pressure_state && node_state != 4 && millis() - last_tick > 1000)
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

	if (readSwitchDigital(VENT_SW) && !vent_state && node_state != 5 && millis() - last_tick > 1000)
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

	if (readSwitchDigital(ACCEPT_POWER) && !accept_state) {
		accept_state = true;
		attemptAccept();
	} else if (!readSwitchDigital(ACCEPT_POWER) && accept_state) {
		accept_state = false;
	}

	if (analogRead(LOCK_OXYGEN_CAP) > 200 && !lock_state && node_state != 7 && millis() - last_tick > 1000) {
		lock_state = true;
		startLockTanks();
	}
	else if (analogRead(LOCK_OXYGEN_CAP) < 200 && lock_state) {
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
