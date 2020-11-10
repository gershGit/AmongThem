//Include statements cover the RFID, NRF24, and ESP Wifi

#include <MFRC522.h>     //Used for RFID
#include <RF24Network.h> //Used for RF24
#include <RF24.h>        //Used for RF24
#include <ESP8266WiFi.h> //Used for WiFi
#include <SPI.h>         //Used for RFID and RF24

//Defines the pins we are using for all of the I/O for RFID and RF24
#define RFID_RST 5       //D1
#define RFID_SDA 4       //D2
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
// 3 -> Atmoic Clock (between 12-14 seconds - use green led as indicator)
unsigned long state_begin_time; //Used to check if something was started and never completed
bool task_complete = false;     //Turns to true when a task is awaiting RFID claim
byte task_active = 0;           //Holds the ID of the task currently in progress or most recently completed

bool log_status = false;
unsigned long log_start;

bool encrypt_status = false;
unsigned long encrypt_status_start_time;

unsigned long last_meeting;
unsigned long meeting_cooldown = 30000;
bool meeting_held = false;

int clock_count = 0;
bool atomic_button_state = false;
bool atomic_status = false;
unsigned long last_click_time;
bool click_ready = false;

//LED info
int led_pins[3];
bool led_states[3];
unsigned long led_last_cycles[3];
#define _GREEN_ 0
#define _RED_ 1
#define _YELLOW_ 2

//Node status
//[Master 01 011 021 02 012 022 03 013 023]
byte nodes_alive[10] = {0};

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
const char *wifi_ssid = "Vie at University Resident";
const char *wifi_pass = "";
IPAddress server(172, 16, 18, 150);
int port = 8771;
bool wifi_blink = false;

void lobby();

void clearLights() {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(RFID_LED, LOW);
}

void setLightsRFID_Ready() {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(RFID_LED, HIGH);
}

void cycleLED(int led_id, unsigned long cycle_time) {
    if (millis()-led_last_cycles[led_id] > cycle_time) {
        if (led_states[led_id]) {
            led_states[led_id] = false;
            digitalWrite(led_pins[led_id], LOW);
        } else {
            led_states[led_id] = true;
            digitalWrite(led_pins[led_id], HIGH);
        }
        led_last_cycles[led_id] = millis();
    }
}

void setup()
{
    //Set pin modes
    pinMode(1, FUNCTION_3); 
    pinMode(3, FUNCTION_3); 
    pinMode(MEETING_BUTTON, INPUT);
    pinMode(LOG_SW, INPUT);
    pinMode(ENCRYPT_SW, INPUT);
    pinMode(ATOMIC_SW, INPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(RFID_LED, OUTPUT);

    //Initialize RFID and RF24
    SPI.begin();
    mfrc522.PCD_Init();
    radio.begin();
    network.begin(90, this_node);
    delay(100);

    //Attempt WiFi connection and register with the server
    /*
    Serial.print("Connecting to wifi...");
    WiFi.begin(wifi_ssid, wifi_pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (wifi_blink == false)
        {
            wifi_blink = true;
            digitalWrite(LED_BUILTIN, LOW);
        }
        else
        {
            wifi_blink = false;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Connecting to server...");
    while (1)
    {
        if (client.connect(server, port))
        {
            Serial.println("Connected to server");
            digitalWrite(LED_BUILTIN, HIGH);
            client.print("R-Main Hub");
            break;
        }
        else
        {
            Serial.print(".");
        }
        delay(500);
        if (wifi_blink == false)
        {
            wifi_blink = true;
            digitalWrite(LED_BUILTIN, LOW);
        }
        else
        {
            wifi_blink = false;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
    lobby();
    */
   digitalWrite(GREEN_LED, HIGH);
   digitalWrite(RED_LED, HIGH);
   digitalWrite(RFID_LED, HIGH);
   delay(2500);
   digitalWrite(GREEN_LED, LOW);
   digitalWrite(RED_LED, LOW);
   digitalWrite(RFID_LED, LOW);
   last_meeting = millis();
}

String getNameFromUid(unsigned long uid)
{
    for (int i = 0; i < 14; i++)
    {
        if (players[i].uid == uid)
        {
            return players[i].p_name;
        }
    }
    return "-";
}

void lobby()
{
    digitalWrite(LED_BUILTIN, HIGH);
    while (1)
    {
        yield();
        //Check for new nodes
        network.update();
        while (network.available())
        {
            RF24NetworkHeader header;
            payload_t incomingData;
            network.read(header, &incomingData, sizeof(incomingData));
            if (incomingData.msg_type == 200)
            {
                nodes_alive[incomingData.msg_code] = 1;
                String toSend = "N-" + String(incomingData.msg_code);
                //client.print(toSend);
            }
        }

        //Check for lobby ready
        if (client.available())
        {
            String line = client.readStringUntil('\n');
            if (line == "start")
            {
                last_meeting = millis();
                break;
            }
            else if (line[0] == 'P')
            {
            }
            else if (line[0] == 'G')
            { //G-M:30
                if (line[2] == 'M')
                {
                    meeting_cooldown = line.substring(4).toInt() * 1000;
                }
            }
        }
        delay(500);
    }
}

void checkNetwork()
{
    while (network.available())
    {
        RF24NetworkHeader header;
        payload_t incomingData;
        network.read(header, &incomingData, sizeof(incomingData));
        if (incomingData.msg_type == 1) {
            String sendStr = "T-C:" + String(incomingData.task_code) + getNameFromUid(incomingData.uid);
            //client.print(sendStr);
        }
    }
}

void checkWiFi()
{
    /*
    if (client.available())
    {
        String line = client.readStringUntil('\n');
        Serial.print("Received on WiFi: ");
        Serial.println(line);
        if (line == "resume")
        {
            Serial.println("Resuming game");
            last_meeting = millis();
            task_complete = false;
            task_active = 0;
            encrypt_status = false;
            return;
        }
        else if (line[0] == 'P')
        { //P-usa:gersh
            String country = line.substring(2, 4);
            String name = line.substring(6);
            Serial.println("Registering player");
            for (int i = 0; i < 14; i++)
            {
                if (players[i].country == country)
                {
                    players[i].active = true;
                    players[i].p_name = name;
                }
            }
        }
        else if (line[0] == 'G')
        { //G-M:30
            if (line[2] == 'M')
            {
                meeting_cooldown = line.substring(4).toInt();
            }
        }
    }
    */
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
        unsigned long uid = getRFID();
        if (uid != 0)
        {
            if (digitalRead(MEETING_BUTTON) == HIGH && (millis() - last_meeting > meeting_cooldown))
            {
                String toSend = "A-Meeting:" + getNameFromUid(uid);
                clearLights();
                last_meeting = millis();
                meeting_held = false;
                //client.print(toSend);
                return;
            }
            if (task_complete)
            {
                task_complete = false;
                String toSend = "T-N:";
                switch (task_active)
                {   
                    case 1:
                        toSend += "encrypt:" + getNameFromUid(uid);
                        //client.print(toSend);
                        digitalWrite(GREEN_LED, HIGH);
                        delay(500);
                        clearLights();
                        task_active = 0;
                        return;
                    case 2:
                        toSend += "log:" + getNameFromUid(uid);
                        //client.print(toSend);
                        digitalWrite(GREEN_LED, HIGH);
                        delay(500);
                        clearLights();
                        task_active = 0;
                        return;
                    case 3:
                        toSend += "clock:" + getNameFromUid(uid);
                        //client.print(toSend);
                        digitalWrite(GREEN_LED, HIGH);
                        delay(500);
                        clearLights();
                        task_active = 0;
                        return;
                    default:
                        digitalWrite(RED_LED,HIGH);
                        return;
                }
            }
        }
    }
}

void checkEncryptData()
{
    if (digitalRead(ENCRYPT_SW) == HIGH && (encrypt_status == false))
    {
        clearLights();
        encrypt_status = true;
        encrypt_status_start_time = millis();
        state_begin_time = millis();
    }
    else if (digitalRead(ENCRYPT_SW) == LOW && (encrypt_status == true) && !task_complete)
    {
        encrypt_status = false;
        clearLights();
        digitalWrite(RED_LED, HIGH);
    }
    else if (digitalRead(ENCRYPT_SW) == HIGH && encrypt_status == true)
    {
        state_begin_time = millis();
        if (millis() - encrypt_status_start_time >= 6000)
        {
            task_active = 1;
            task_complete = true;
            state_begin_time = millis();
            setLightsRFID_Ready();
        } else {
            cycleLED(_RED_, 250);
        }
    }
}

void checkMeetingButton()
{
    
    if (digitalRead(MEETING_BUTTON) == LOW && meeting_held == false) {
        if (millis() - last_meeting > meeting_cooldown) {
            state_begin_time = millis();
            setLightsRFID_Ready();
        } else {
            clearLights();
            digitalWrite(RED_LED, HIGH);
        }
        meeting_held = true;
    } else if (digitalRead(MEETING_BUTTON) == HIGH && meeting_held == true) {
        clearLights();
        meeting_held = false;
    }
}

void checkLogManifest()
{
    //Swipe start
    if (digitalRead(LOG_SW) == LOW && log_status == false) {
        state_begin_time = millis();
        clearLights();
        task_complete = false;
        log_status = true;
        log_start = millis();
        digitalWrite(GREEN_LED, HIGH);
    } else if (digitalRead(LOG_SW) == HIGH && log_status == true) { //Swipe end
        unsigned long diff = millis() - log_start;
        if (diff > 1000 && diff < 1500) {
            task_active = 2;
            task_complete = true;
            log_status = false;
            setLightsRFID_Ready();
        } else {
            digitalWrite(GREEN_LED, LOW);
            digitalWrite(RED_LED, HIGH);
            log_status = false;
        }
    }
}

void checkAtomicClock()
{
    //Indicator light for when to hit the next spin
    if (atomic_status == true) {
        if (millis()-last_click_time > 2000 && millis()-last_click_time < 2800) {
            digitalWrite(GREEN_LED, LOW);
            click_ready = true;
        } else {
            digitalWrite(GREEN_LED, HIGH);
            click_ready = false;
        }
    }

    if (analogRead(ATOMIC_SW) < 800 && atomic_status == false && atomic_button_state == false) { //Start task
        task_active = 3;
        state_begin_time = millis();
        last_click_time = millis();
        atomic_button_state = true;
        atomic_status = true;
        clearLights();
    } else if (analogRead(ATOMIC_SW) < 800 && atomic_status == true && atomic_button_state == false) { //Next click
        if (click_ready) {
            clock_count++;
            state_begin_time = millis();
            last_click_time = millis();
            atomic_button_state = true;
            if (clock_count == 5) {
                atomic_status=false;
                task_complete = true;
                task_active = 3;
                setLightsRFID_Ready();
            }
        } else {
            clearLights();
            digitalWrite(RED_LED, HIGH);
            task_active = 0;
            atomic_status = false;
        }
    } else if (analogRead(ATOMIC_SW) > 800 && atomic_button_state == true) {
        atomic_button_state = false;
    }
}

void checkStateAbandoned()
{
    if (task_active != 0 && millis() - state_begin_time > 10000)
    {
        task_complete = false;
        task_active = 0;
        clearLights();
    }
}

void loop()
{
    network.update();
    checkNetwork();
    checkWiFi();
    checkRFID();
    checkMeetingButton();
    checkLogManifest();
    checkAtomicClock();
    checkEncryptData();
    checkStateAbandoned();
}
