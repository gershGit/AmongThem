#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <RF24Network.h>
#include <RF24.h>
#include <math.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define TO_RAD 0.0174532925

#define _POS1_ 0b00000001
#define _POS1_ 0b00000010
#define _POS1_ 0b00000100
#define _POS1_ 0b00001000
#define _POS1_ 0b00010000
#define _POS1_ 0b00100000
#define _POS1_ 0b01000000
#define _POS1_ 0b10000000

byte task_ids[10] = {0, 111, 112, 113, 114, 115, 116, 117, 118, 119};

//Pin declarations for physical tasks
#define PHOTO_RESISTOR A0
#define FUEL_CELLS A6
#define AIRLOCK_1 A1
#define AIRLOCK_2 A2
#define AIRLOCK_3 A3
#define RFID_RST 7
#define RFID_SDA 8
#define RF_CE 9
#define RF_CS 10

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VERTICAL_KNOB_A 5
#define VERTICAL_KNOB_B 6
#define VERTICAL_KNOB_SWITCH A7
RotaryEncoder v_encoder(VERTICAL_KNOB_A, VERTICAL_KNOB_B);

#define HORIZONTAL_KNOB_A 2
#define HORIZONTAL_KNOB_B 3
#define HORIZONTAL_KNOB_SWITCH 4
RotaryEncoder h_encoder(HORIZONTAL_KNOB_A, HORIZONTAL_KNOB_B);

MFRC522 mfrc522(RFID_SDA, RFID_RST);
RF24 radio(RF_CE, RF_CS);
RF24Network network(radio);
byte node_id = 1;
const uint16_t this_node = 01;
const uint16_t master00 = 00;

//TODO delete
byte last_state = 0;

byte v_pos = 0;
byte v_last;
byte h_pos = 0;
byte h_last;
//[..., v_knob_held, h_knob_held]
byte buttons_held = 0b0;

//Variables for mark game
byte boxPositions[8] = {0, 0, 0, 0, 0, 0, 0, 0};
//byte boxStates = 0b00000000;
byte marking = false;
byte boxStates[4] = {false, false, false, false};

//Variables for panel game
byte left_panel = 90;
byte right_panel = 90;
byte leftPowerRates[10] = {0};
byte rightPowerRates[10] = {0};
byte totalPower = 0;
byte right_power = 0;
byte left_power = 0;

//Variables for atmosphere game
byte atmo_require[4] = {0};
byte atmo_set[4] = {0};
byte atmo_done[4] = {false, false, false, false};
byte gas_selected = 0;
unsigned long last_tick;
unsigned long gas_delay = 100;
byte gas_held = false;
byte last_gas_pos;

//Variables for downloads
byte dl_completed = 0;
byte dl_delay = 100;

//Variables for soil inspection
byte light_holding = false;
byte light_count = 0;
int light_base_threshold = 500;
int light_max_threshold = 600;
unsigned long light_hold_start;

//Vairables for fuel cells
byte cells_connected = false;
unsigned long cell_conn_start;
byte cells_finished = false;

//Variables for airlock
byte airlock_switch_state[3] = {false, false, false};
byte airlock_status[3] = {0};

//Nodewide data
byte node_state = 0;
byte task_complete = false;
byte task_num_completed = -1;
unsigned long task_start_time;
byte hover_select = 0;
byte menu_pos = 0;
byte click_held = false;

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

struct panel_line_t
{
    int x1;
    int y1;
    int x2;
    int y2;
};

bool checkFlag(byte flag, byte set)
{
    return flag & set;
}

byte clearFlag(byte flag, byte set)
{
    return (!flag) & set;
}

byte setFlag(byte flag, byte set)
{
    return flag | set;
}

static bool analogSwitchRead(int pin)
{
    return analogRead(pin) < 50;
}

static bool digitalSwitchRead(int pin)
{
    return digitalRead(pin) == LOW;
}

static void displayText(char* text)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 6);
    display.setTextColor(SSD1306_WHITE);
    display.print(text);
    display.display();
}

static bool checkReturn() {
    if (digitalSwitchRead(HORIZONTAL_KNOB_SWITCH))
    {
        returnToMenu();
        return true;
    }
    return false;
}

static message_t checkNetwork()
{
    while (network.available())
    {
        RF24NetworkHeader header;
        payload_t incomingData;
        network.read(header, &incomingData, sizeof(incomingData));
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

static unsigned long getRFID()
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

static void checkRFID()
{
    if (mfrc522.PICC_IsNewCardPresent())
    {
        unsigned long uid = getRFID();
        if (uid != -1 && task_complete)
        {
            task_complete = false;
            RF24NetworkHeader header_00(master00);
            payload_t payload = {1, 11, uid, task_ids[task_num_completed]};
            bool ok = network.write(header_00, &payload, sizeof(payload));
            while (!ok)
            {
                ok = network.write(header_00, &payload, sizeof(payload));
                delay(250);
            }
        }
    }
}

static void connectToServer()
{
    displayText("Connecting to \n server");
    //Serial.println("Connecting to server...");
    RF24NetworkHeader header_00(master00);
    payload_t payload = {99, node_id, 0, 0};
    bool ok = network.write(header_00, &payload, sizeof(payload));
    bool success = false;
    while (!success)
    {
        network.update();
        if (!ok)
        {
            delay(2000);
            ok = network.write(header_00, &payload, sizeof(payload));
            continue;
        }
        else
        {
            message_t mess = checkNetwork();
            if (mess.payload.msg_type == 99)
            {
                return;
            }
        }
    }
    return;
}

static void resetHub()
{
    node_state = 0;
    task_num_completed = 0;
    task_start_time = millis();
    task_complete = false;
    last_tick = millis();
    returnToMenu();
    delay(1000);
}

static void waitForStart()
{
    displayText("Waiting for start");
    //Serial.println("Waiting for start");
    while (true)
    {
        network.update();
        while (network.available())
        {
            RF24NetworkHeader header;
            payload_t incomingData;
            network.read(header, &incomingData, sizeof(payload_t));
            if (incomingData.msg_type == 80 && incomingData.msg_code == 0)
            {
                //Serial.println("Starting");
                randomSeed(millis());
                resetHub();
                return;
            }
        }
        delay(1000);
    }
}

void setup()
{
    pinMode(PHOTO_RESISTOR, INPUT);
    pinMode(FUEL_CELLS, INPUT);
    pinMode(HORIZONTAL_KNOB_A, INPUT);
    pinMode(HORIZONTAL_KNOB_B, INPUT);
    pinMode(HORIZONTAL_KNOB_SWITCH, INPUT);
    pinMode(VERTICAL_KNOB_A, INPUT);
    pinMode(VERTICAL_KNOB_B, INPUT);
    pinMode(VERTICAL_KNOB_SWITCH, INPUT);

    //Serial.begin(115200);
    delay(150);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    { // Address 0x3C for 128x32
        for (;;)
            ;
    }
    display.clearDisplay();
    display.display();
    delay(1000);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    SPI.begin();
    mfrc522.PCD_Init();
    delay(4);

    radio.begin();
    network.begin(90, this_node);
    delay(100);

    v_encoder.tick();
    h_encoder.tick();
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();

    connectToServer();
    waitForStart();
}

static void showMenu(int selected)
{
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
        if (i == selected)
        {
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }
        display.write(menu_options[i]);
        xpos += 30;
    }
    if (task_complete)
    {
        display.fillRect(0, 0, 8, 8, SSD1306_WHITE);
    }
    display.display();
}

static void returnToMenu()
{
    hover_select = 0;
    node_state = 0;
    task_start_time = millis();
    menu_pos = v_encoder.getPosition();
    showMenu(hover_select);
}

static void setGameComplete(int gameID)
{
    task_num_completed = gameID;
    task_complete = true;
    returnToMenu();
}

static void startMarkObstacles()
{
    v_pos = 16;
    h_pos = 64;
    h_last = h_encoder.getPosition();
    v_last = v_encoder.getPosition();
    marking = false;
    for (int i = 0; i < 8; i++)
    {
        boxPositions[i++] = random(122);
        boxPositions[i] = random(26);
    }
    for (int i = 0; i < 4; i++) {
        boxStates[i] = false;
    }
}

static void updateMarkObstacles()
{
    if (checkReturn())
    {
        return;
    }
    int v_diff = v_encoder.getPosition() - v_last;
    int h_diff = h_encoder.getPosition() - h_last;
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();

    if (v_diff != 0)
    {
        v_pos += (v_diff * 2);
        if (v_pos > 31)
        {
            v_pos = 31;
        }
        else if (v_pos < 0)
        {
            v_pos = 0;
        }
    }
    if (h_diff != 0)
    {
        h_pos -= (h_diff * 4);
        if (h_pos > 127)
        {
            h_pos = 127;
        }
        else if (h_pos < 0)
        {
            h_pos = 0;
        }
    }

    //Check if they are clicking to mark an obstacle
    if (analogSwitchRead(VERTICAL_KNOB_SWITCH) && !marking)
    {
        marking = true;
        //Check for a matching box
        for (int i = 0; i < 8; i += 2)
        {
            if (h_pos > boxPositions[i] && h_pos < boxPositions[i] + 6 && v_pos > boxPositions[i + 1] && v_pos < boxPositions[i + 1] + 6)
            {
                boxStates[i / 2] = true;
                byte remaining = 0;
                for (int j = 0; j < 4; j++)
                {
                    if (boxStates[j] == false)
                    {
                        remaining = 1;
                        break;
                    }
                }
                if (remaining == 0)
                {
                    setGameComplete(1);
                }
            }
        }
    }
    if (!analogSwitchRead(VERTICAL_KNOB_SWITCH))
    {
        marking = false;
    }

    display.clearDisplay();
    for (int i = 0; i < 8; i += 2)
    {
        if (boxStates[i / 2] == true)
        {
            display.fillRect(boxPositions[i], boxPositions[i + 1], 6, 6, SSD1306_WHITE);
        }
        else
        {
            display.drawRect(boxPositions[i], boxPositions[i + 1], 6, 6, SSD1306_WHITE);
        }
    }
    display.drawFastVLine(h_pos, 0, display.height(), SSD1306_WHITE);
    display.drawFastHLine(0, v_pos, display.width(), SSD1306_WHITE);
    display.display();
}

static void startPanels()
{
    left_panel = 90;
    right_panel = 90;
    v_last = v_encoder.getPosition();
    h_last = v_encoder.getPosition();
    int leftReserved = random(10);
    int rightReserved = random(10);
    for (int i = 0; i < 10; i++)
    {
        leftPowerRates[i] = random(45);
    }
    for (int i = 0; i < 10; i++)
    {
        rightPowerRates[i] = random(45);
    }
    leftPowerRates[leftReserved] = 50;
    rightPowerRates[rightReserved] = 50;
    left_power = leftPowerRates[9];
    right_power = rightPowerRates[9];
    totalPower = left_power + right_power;
}

static panel_line_t getPanelLine(int x1, int y1, float angle, int length, int inverse_x)
{
    panel_line_t panel_line;
    panel_line.x1 = x1;
    panel_line.y1 = y1;
    if (inverse_x)
    {
        panel_line.x2 = x1 - (cos(angle * TO_RAD) * length);
    }
    else
    {
        panel_line.x2 = x1 + (cos(angle * TO_RAD) * length);
    }
    panel_line.y2 = y1 - (sin(angle * TO_RAD) * length);
    return panel_line;
}

static void updatePanels()
{
    if (checkReturn()) {
        return;
    }

    int v_diff = v_encoder.getPosition() - v_last;
    int h_diff = h_encoder.getPosition() - h_last;
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();

    if (v_diff != 0)
    {
        right_panel += (v_diff * 10);
        if (right_panel > 90)
        {
            right_panel = 90;
        }
        else if (right_panel < 0)
        {
            right_panel = 0;
        }
        right_power = rightPowerRates[right_panel / 10];
    }
    if (h_diff != 0)
    {
        left_panel -= (h_diff * 10);
        if (left_panel > 90)
        {
            left_panel = 90;
        }
        else if (left_panel < 0)
        {
            left_panel = 0;
        }
        left_power = leftPowerRates[left_panel / 10];
    }

    //Update values
    totalPower = left_power + right_power;

    //Draw the panels
    display.clearDisplay();
    //Total power bar
    display.drawRect(13, 2, 100, 6, SSD1306_WHITE);
    display.fillRect(13, 2, totalPower, 6, SSD1306_WHITE);

    //Left panel box
    display.drawRect(5, 10, 20, 20, SSD1306_WHITE);
    panel_line_t left_line = getPanelLine(24, 29, left_panel, 18, true);
    display.drawLine(left_line.x1, left_line.y1, left_line.x2, left_line.y2, SSD1306_WHITE);

    //Right panel box
    display.drawRect(102, 10, 20, 20, SSD1306_WHITE);
    panel_line_t right_line = getPanelLine(102, 29, right_panel, 18, false);
    display.drawLine(right_line.x1, right_line.y1, right_line.x2, right_line.y2, SSD1306_WHITE);

    //Left power
    display.drawRect(28, 10, 6, 20, SSD1306_WHITE);
    int left_height = (leftPowerRates[left_panel / 10] / 5) * 2;
    display.fillRect(29, 30 - left_height, 4, left_height, SSD1306_WHITE);

    //Right power
    display.drawRect(93, 10, 6, 20, SSD1306_WHITE);
    int right_height = (rightPowerRates[right_panel / 10] / 5) * 2;
    display.fillRect(94, 30 - right_height, 4, right_height, SSD1306_WHITE);

    //Power Reading
    display.setTextSize(2);
    display.setCursor(45, 15);
    display.print(totalPower);

    display.display();
    if (totalPower == 100)
    {
        delay(1000);
        setGameComplete(2);
    }
}

static void startAtmosphere()
{
    for (int i = 0; i < 4; i++)
    {
        atmo_require[i] = random(94);
        atmo_set[i] = 0;
        atmo_done[i] = false;
        gas_selected = 0;
    }
    last_gas_pos = v_encoder.getPosition();
}

static void updateAtmosphere()
{
    if (checkReturn())
    {
        return;
    }

    int diff = last_gas_pos - v_encoder.getPosition();
    if (diff != 0)
    {
        last_gas_pos = v_encoder.getPosition();
        gas_selected += diff;
        if (gas_selected > 3)
        {
            gas_selected = 3;
        }
        else if (gas_selected < 0)
        {
            gas_selected = 0;
        }
    }

    if (analogSwitchRead(VERTICAL_KNOB_SWITCH) && gas_held == false)
    {
        gas_held = true;
        last_tick = millis();
    }
    else if (analogSwitchRead(VERTICAL_KNOB_SWITCH) && gas_held == true)
    {
        if (millis() - last_tick > gas_delay)
        {
            atmo_set[gas_selected]++;
            if (atmo_set[gas_selected] > atmo_require[gas_selected] + 6)
            {
                startAtmosphere();
                return;
            }
            else if (atmo_set[gas_selected] > atmo_require[gas_selected] - 3 && atmo_set[gas_selected] < atmo_require[gas_selected] + 6)
            {
                atmo_done[gas_selected] = true;
                bool failed = false;
                for (int i = 0; i < 4; i++)
                {
                    if (atmo_done[i] == false)
                    {
                        failed = true;
                    }
                }
                if (!failed)
                {
                    delay(500);
                    setGameComplete(6);
                    return;
                }
            }
            last_tick = millis();
        }
    }
    else if (!analogSwitchRead(VERTICAL_KNOB_SWITCH) && gas_held == true)
    {
        gas_held = false;
    }

    //Display the bars
    display.clearDisplay();
    display.drawRect(2, 2, 100, 6, SSD1306_WHITE);
    display.drawRect(atmo_require[0], 2, 6, 6, SSD1306_WHITE);
    display.fillRect(3, 3, atmo_set[0], 4, SSD1306_WHITE);
    display.drawRect(2, 10, 100, 6, SSD1306_WHITE);
    display.drawRect(atmo_require[1], 10, 6, 6, SSD1306_WHITE);
    display.fillRect(3, 11, atmo_set[1], 4, SSD1306_WHITE);
    display.drawRect(2, 18, 100, 6, SSD1306_WHITE);
    display.drawRect(atmo_require[2], 18, 6, 6, SSD1306_WHITE);
    display.fillRect(3, 19, atmo_set[2], 4, SSD1306_WHITE);
    display.drawRect(2, 26, 100, 6, SSD1306_WHITE);
    display.drawRect(atmo_require[3], 26, 6, 6, SSD1306_WHITE);
    display.fillRect(3, 27, atmo_set[3], 4, SSD1306_WHITE);
    display.fillRect(104, 2 + (8 * gas_selected), 6, 6, SSD1306_WHITE);
    for (int i = 0; i < 4; i++)
    {
        if (atmo_done[i])
        {
            display.fillRect(112, 2 + (8 * i), 6, 6, SSD1306_WHITE);
        }
    }
    display.display();
}

static void startDownload()
{
    last_tick = millis();
    dl_completed = 0;
}

static void drawBar(byte completion, byte val) {
    display.drawRect(2, 6, 100, 20, SSD1306_WHITE);
    display.fillRect(2, 7, completion, 18, SSD1306_WHITE);
    display.setTextSize(1.5);
    display.setCursor(110, 12);
    display.print(val);
}

inline void updateDownload()
{
    if (checkReturn())
    {
        return;
    }

    if (millis() - last_tick > dl_delay)
    {
        dl_completed++;
        last_tick = millis();
    }

    display.clearDisplay();
    drawBar(dl_completed, dl_completed);
    display.display();

    if (dl_completed == 100)
    {
        delay(500);
        setGameComplete(node_state);
    }
}

static void startSoilCheck()
{
    light_holding = false;
    light_base_threshold = analogRead(PHOTO_RESISTOR);
}

static void updateSoilCheck()
{
    if (checkReturn())
    {
        return;
    }

    if (analogRead(PHOTO_RESISTOR) > light_base_threshold + 100 && !light_holding)
    {
        light_holding = true;
        last_tick = millis();
    }
    else if (analogRead(PHOTO_RESISTOR) > light_base_threshold + 100 && light_holding)
    {
        if (millis() - last_tick > 100)
        {
            light_count++;
            last_tick = millis();
        }
    }
    else if (analogRead(PHOTO_RESISTOR) < light_base_threshold + 100 && light_holding)
    {
        light_holding = false;
    }

    display.clearDisplay();
    drawBar(light_count - 1, light_count);
    display.display();

    if (light_count == 100)
    {
        delay(500);
        setGameComplete(3);
    }
}

static void startLightCalibrate()
{
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW)
    {
        returnToMenu();
        return;
    }
    light_base_threshold = analogRead(PHOTO_RESISTOR);
    light_count = 0;
    displayText("Keep Dark");
    delay(2000);
    displayText("Shine Now");
    delay(1500);
    light_max_threshold = analogRead(PHOTO_RESISTOR);
    if (light_max_threshold - light_base_threshold < 200)
    {
        startLightCalibrate();
    }
}

static void updateLightCalibrate()
{
    if (checkReturn())
    {
        return;
    }

    int light_percent = float(analogRead(PHOTO_RESISTOR) - light_base_threshold) / float(light_max_threshold - light_base_threshold) * 100;

    display.clearDisplay();
    drawBar(light_percent - 1, light_count * 2);
    display.drawRect(42, 6, 20, 20, SSD1306_WHITE);
    display.display();

    if (light_percent > 40 && light_percent < 60 && millis() - last_tick > 100)
    {
        last_tick = millis();
        light_count++;
    }

    if (light_count > 50)
    {
        delay(500);
        setGameComplete(3);
        returnToMenu();
    }
}

static void startAirlockGame()
{
    airlock_status[0] = 0;
    airlock_status[1] = 0;
    airlock_status[2] = 0;
}

static void updateAirlockGame()
{
    if (checkReturn())
    {
        return;
    }

    if (analogSwitchRead(AIRLOCK_1) && millis() - last_tick > 50)
    {
        last_tick = millis();
        airlock_status[0]++;
        if (airlock_status[0] > 100)
        {
            airlock_status[0] = 100;
        }
    }

    if (analogSwitchRead(AIRLOCK_2) && millis() - last_tick > 50)
    {
        last_tick = millis();
        airlock_status[1]++;
        if (airlock_status[1] > 100)
        {
            airlock_status[1] = 100;
        }
    }

    if (analogSwitchRead(AIRLOCK_3) && millis() - last_tick > 50)
    {
        last_tick = millis();
        airlock_status[2]++;
        if (airlock_status[2] > 100)
        {
            airlock_status[2] = 100;
        }
    }

    bool complete = true;
    for (int i = 0; i < 3; i++)
    {
        if (airlock_status[i] < 100)
        {
            complete = false;
        }
    }

    if (complete == true)
    {
        display.clearDisplay();
        display.fillRect(42, 10, 6, 20, SSD1306_WHITE);
        display.fillRect(53, 2, 21, 6, SSD1306_WHITE);
        display.fillRect(78, 10, 6, 20, SSD1306_WHITE);
        display.fillCircle(63, 20, 10, SSD1306_WHITE);
        display.display();
        delay(1500);

        setGameComplete(9);
        return;
    }
    else
    {
        display.clearDisplay();
        display.drawRect(42, 10, 6, 20, SSD1306_WHITE);
        display.fillRect(43, 30 - (airlock_status[0] / 5), 4, airlock_status[0] / 5, SSD1306_WHITE);
        display.drawRect(53, 2, 21, 6, SSD1306_WHITE);
        display.fillRect(54, 3, airlock_status[1] / 5, 4, SSD1306_WHITE);
        display.drawRect(78, 10, 6, 20, SSD1306_WHITE);
        display.fillRect(79, 30 - (airlock_status[2] / 5), 4, airlock_status[2] / 5, SSD1306_WHITE);
        display.drawCircle(63, 20, 10, SSD1306_WHITE);
        display.display();
    }
}

static void select_game(int game_id)
{
    node_state = game_id;
    task_complete = false;
    switch (game_id)
    {
    case 1:
        startMarkObstacles();
        break;
    case 2:
        startPanels();
        break;
    case 3:
        startSoilCheck();
        break;
    case 4:
        startDownload();
        break;
    case 5:
        startDownload();
        break;
    case 6:
        startAtmosphere();
        break;
    case 7:
        startLightCalibrate();
        break;
    case 9:
        startAirlockGame();
        break;
    default:
        break;
    }
}

void loop()
{
    v_encoder.tick();
    h_encoder.tick();
    checkNetwork();
    checkRFID();

    switch (node_state) {
        case 0:
            {
                int menu_diff = v_encoder.getPosition() - menu_pos;
                menu_pos = v_encoder.getPosition();
                if (menu_diff != 0)
                {
                    hover_select -= menu_diff;
                    if (hover_select > 6)
                    {
                        hover_select = 6;
                    }
                    else if (hover_select < 0)
                    {
                        hover_select = 0;
                    }
                }
                showMenu(hover_select);
                if (analogSwitchRead(VERTICAL_KNOB_SWITCH) && click_held == false && millis() - task_start_time > 1000)
                {
                    click_held = true;
                }
                else if (analogSwitchRead(VERTICAL_KNOB_SWITCH) && click_held == true)
                {
                    select_game(hover_select + 1);
                    click_held = false;
                }

                if (analogSwitchRead(AIRLOCK_1) || analogSwitchRead(AIRLOCK_2) || analogSwitchRead(AIRLOCK_3))
                {
                    select_game(9);
                }
                break;
            }
        case 1:
            updateMarkObstacles();
            break;
        case 2:
            updatePanels();
            break;
        case 3:
            updateSoilCheck();
            break;
        case 4:
        case 5:
            updateDownload();
            break;
        case 6:
            updateAtmosphere();
            break;
        case 7:
            updateLightCalibrate();
            break;
        case 9:
            updateAirlockGame();
            break;
        default:
            break;
    }

    //Check for attachment of cells
    if (cells_finished == false && digitalRead(FUEL_CELLS) == HIGH && cells_connected == false)
    {
        cell_conn_start = millis();
        cells_connected = true;
    }
    else if (digitalRead(FUEL_CELLS) == HIGH && cells_finished == false && cells_connected == true)
    {
        if (millis() - cell_conn_start > 1500)
        {
            displayText("Cells connected");
            cells_finished = true;
            delay(2000);
            setGameComplete(8);
        }
    }
    else if (digitalRead(FUEL_CELLS) == LOW && cells_connected == true)
    {
        cells_connected = false;
    }
}