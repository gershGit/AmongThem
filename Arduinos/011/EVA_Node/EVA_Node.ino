#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <math.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define TO_RAD 0.0174532925

//Pin declarations for physical tasks
#define PHOTO_RESISTOR A0
#define FUEL_CELLS 8
#define AIRLOCK_1 A1
#define AIRLOCK_2 A2
#define AIRLOCK_3 A3
#define PUMP_1 A6
#define PUMP_2 A7

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VERTICAL_KNOB_A 2
#define VERTICAL_KNOB_B 3
#define VERTICAL_KNOB_SWITCH 4
RotaryEncoder v_encoder(VERTICAL_KNOB_A, VERTICAL_KNOB_B);

#define HORIZONTAL_KNOB_A 5
#define HORIZONTAL_KNOB_B 6
#define HORIZONTAL_KNOB_SWITCH 7
RotaryEncoder h_encoder(HORIZONTAL_KNOB_A, HORIZONTAL_KNOB_B);

int v_pos = 0;
int v_last;
int v_knob_button_state = LOW;
int h_pos = 0;
int h_last;

//Variables for mark game
int boxPositions[8] = {0, 0, 0, 0, 0, 0, 0, 0};
bool boxStates[4] = {false, false, false, false};
bool select_held = false;

//Variables for panel game
int left_panel = 90;
int right_panel = 90;
int leftPowerRates[10] = {0};
int rightPowerRates[10] = {0};
int totalPower = 0;
int right_power = 0;
int left_power = 0;

//Variables for atmosphere game
int atmo_require[4] = {0};
int atmo_set[4] = {0};
bool atmo_done[4] = {false, false, false, false};
int gas_selected = 0;
unsigned long last_tick;
unsigned long gas_delay = 100;
bool gas_held = false;
int last_gas_pos;

//Variables for downloads
int dl_completed = 0;
unsigned long dl_delay = 100;

//Variables for soil inspection
bool light_holding = false;
int light_count = 0;
int light_base_threshold = 500;
int light_max_threshold = 600;
unsigned long light_hold_start;

//Vairables for fuel cells
bool cells_connected = false;
unsigned long cell_conn_start;
bool cells_finished = false;

//Variables for airlock
bool airlock_switch_state[3] = {false, false, false};
int airlock_status[3] = {0};

//Variables for pump
int pump_count = 0;
int pumps_needed = 1;
bool next_pump_ready = false;

//RFID data

//RF24 data

int node_state = 0;
int game_completed = 0;
int hover_select = 0;
int menu_pos = 0;
unsigned long start_click;
unsigned long delay_start;
bool click_held = false;

struct panel_line_t {
    int x1;
    int y1;
    int x2;
    int y2;
};

bool analogSwitchRead(int pin) {
    return analogRead(pin) < 100;
}

bool digitalSwitchRead(int pin) {
    return digitalRead(pin) == LOW;
}

void setup() {
    pinMode(PHOTO_RESISTOR, INPUT);
    pinMode(FUEL_CELLS, INPUT);
    pinMode(HORIZONTAL_KNOB_A, INPUT);
    pinMode(HORIZONTAL_KNOB_B, INPUT);
    pinMode(HORIZONTAL_KNOB_SWITCH, INPUT);
    pinMode(VERTICAL_KNOB_A, INPUT);
    pinMode(VERTICAL_KNOB_B, INPUT);
    pinMode(VERTICAL_KNOB_SWITCH, INPUT);

    Serial.begin(115200);
    delay(150);
    Serial.println("Starting up");
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.display();
    v_knob_button_state = digitalRead(VERTICAL_KNOB_SWITCH);
    delay(1000);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    v_encoder.tick();
    h_encoder.tick();
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();
}

void updateRoverObstacles() {
    int v_diff = v_encoder.getPosition() - v_last;
    int h_diff = h_encoder.getPosition() - h_last;
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();

    if (v_diff != 0)
    {
        v_pos += (v_diff * 2);
        if (v_pos > 15)
        {
            v_pos = 15;
        }
        else if (v_pos < -16)
        {
            v_pos = -16;
        }
    }

    if (h_diff != 0)
    {
        h_pos += (h_diff * 4);
        Serial.print("h_diff: ");
        Serial.print(h_diff);
        Serial.print(" h_pos: ");
        Serial.println(h_pos);
        if (h_pos > 63)
        {
            h_pos = 63;
        }
        else if (h_pos < -64)
        {
            h_pos = -64;
        }
    }

    if (digitalRead(VERTICAL_KNOB_SWITCH) != v_knob_button_state)
    {
        v_knob_button_state = !v_knob_button_state;
        if (v_knob_button_state == HIGH)
        {
            Serial.println("Released");
            display.clearDisplay();
            display.display();
        }
        else
        {
            Serial.println("Pressed");
            display.clearDisplay();
            display.fillRect(0, 0, 16, 16, SSD1306_WHITE);
            display.display();
        }
    }
    else
    {
        display.clearDisplay();
        display.drawFastVLine(h_pos + 64, 0, display.height(), SSD1306_WHITE);
        display.drawFastHLine(0, v_pos + 16, display.width(), SSD1306_WHITE);
        display.display();
    }
}

void showMenu(int selected) {
    display.clearDisplay();
    int xpos = 25, ypos = 2;
    display.setTextSize(2);
    display.cp437(true);
    char menu_options[7] = {'M', 'P', 'S', 'R', 'E', 'A', 'L'};
    for (int i = 0; i < 7; i++) {
        if (i==3) {
            xpos = 10;
            ypos = 16;
        }
        display.setCursor(xpos, ypos);
        if (i==selected) {
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.write(menu_options[i]);
        xpos += 30;
    }
    if (game_completed != 0) {
        display.fillRect(0, 0, 8, 8, SSD1306_WHITE);
    }
    display.display();
}

void setGameComplete(int gameID)
{
    Serial.print("Game complete: ");
    Serial.println(gameID);
    game_completed = gameID;
}

void returnToMenu() {
    hover_select = 0;
    node_state = 0;
    delay_start = millis();
    click_held = false;
    menu_pos = v_encoder.getPosition();
    showMenu(hover_select);
}

void startDefaultGame() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,10);
    display.write("No Game");
    display.display();
}

void updateDefaultGame() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW) {
        returnToMenu();
    }
}

void startMarkObstacles() {
    game_completed = 0;
    v_pos = 16;
    h_pos = 64;
    h_last = h_encoder.getPosition();
    v_last = v_encoder.getPosition();
    for (int i = 0; i < 8; i++) {
        boxPositions[i++] = random(122);
        boxPositions[i] = random(26);
    }
    for (int i = 0; i < 4; i++) {
        boxStates[i] = false;
    }
}

void updateMarkObstacles() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW)
    {
        returnToMenu();
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
    if (digitalRead(VERTICAL_KNOB_SWITCH) == LOW && !select_held) {
        select_held = true;
        //Check for a matching box
        for (int i = 0; i < 8; i+=2) {
            if (h_pos > boxPositions[i] && h_pos < boxPositions[i] + 6 && v_pos > boxPositions[i + 1] && v_pos < boxPositions[i + 1] + 6)
            {
                boxStates[i / 2] = true;
                Serial.println("Box found!");
                bool remaining = false;
                for (int j = 0; j < 4; j++)
                {
                    if (boxStates[j] == false)
                    {
                        remaining = true;
                        break;
                    }
                }
                if (!remaining)
                {
                    setGameComplete(1);
                    returnToMenu();
                }
            }
        }
    }
    if (digitalRead(VERTICAL_KNOB_SWITCH) == HIGH) {
        select_held = false;
    }

    display.clearDisplay();
    for (int i = 0; i < 8; i+=2) {
        if (boxStates[i/2] == true) {
            display.fillRect(boxPositions[i], boxPositions[i + 1], 6, 6, SSD1306_WHITE);
        } else {
            display.drawRect(boxPositions[i], boxPositions[i + 1], 6, 6, SSD1306_WHITE);
        }
    }
    display.drawFastVLine(h_pos, 0, display.height(), SSD1306_WHITE);
    display.drawFastHLine(0, v_pos, display.width(), SSD1306_WHITE);
    display.display();
}

void startPanels() {
    game_completed = 0;
    left_panel = 90;
    right_panel = 90;
    v_last = v_encoder.getPosition();
    h_last = v_encoder.getPosition();
    int leftReserved = random(10);
    int rightReserved = random(10);
    for (int i = 0; i < 10; i++) {
        leftPowerRates[i] = random(45);
    }
    for (int i = 0; i < 10; i++) {
        rightPowerRates[i] = random(45);
    }
    leftPowerRates[leftReserved] = 50;
    rightPowerRates[rightReserved] = 50;
    left_power = leftPowerRates[9];
    right_power = rightPowerRates[9];
    totalPower = left_power + right_power;
}

panel_line_t getPanelLine(int x1, int y1, float angle, int length, int inverse_x) {
    panel_line_t panel_line;
    panel_line.x1 = x1;
    panel_line.y1 = y1;
    if (inverse_x) {
        panel_line.x2 = x1 - (cos(angle * TO_RAD) * length);
    } else {
        panel_line.x2 = x1 + (cos(angle * TO_RAD) * length);
    }
    panel_line.y2 = y1 - (sin(angle * TO_RAD) * length);
    return panel_line;
}

void updatePanels() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW)
    {
        returnToMenu();
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
        returnToMenu();
    }
}

void startAtmosphere() {
    for (int i = 0; i < 4; i++) {
        atmo_require[i] = random(94);
        atmo_set[i] = 0;
        atmo_done[i] = false;
        gas_selected = 0;
    }
    last_gas_pos = v_encoder.getPosition();
}

void updateAtmosphere() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW) {
        returnToMenu();
        return;
    }

    int diff = last_gas_pos - v_encoder.getPosition();
    if (diff != 0) {
        last_gas_pos = v_encoder.getPosition();
        gas_selected += diff;
        if (gas_selected > 3) {
            gas_selected = 3;
        } else if (gas_selected < 0) {
            gas_selected = 0;
        }
    }

    if (digitalRead(VERTICAL_KNOB_SWITCH) == LOW && gas_held == false) {
        gas_held = true;
        last_tick = millis();
    }
    else if (digitalRead(VERTICAL_KNOB_SWITCH) == LOW && gas_held==true) {
        if (millis() - last_tick > gas_delay) {
            atmo_set[gas_selected]++;
            Serial.print("Gas ");
            Serial.print(gas_selected);
            Serial.print(": ");
            Serial.print(atmo_set[gas_selected]);
            Serial.print("/");
            Serial.println(atmo_require[gas_selected]);
            if (atmo_set[gas_selected] > atmo_require[gas_selected]+6) {
                startAtmosphere();
                return;
            } else if (atmo_set[gas_selected] > atmo_require[gas_selected]-3 && atmo_set[gas_selected] < atmo_require[gas_selected]+6) {
                atmo_done[gas_selected] = true;
                bool failed = false;
                for (int i = 0; i < 4; i++){
                    if (atmo_done[i] == false) {
                        failed = true;
                    }
                }
                if (!failed) {
                    delay(500);
                    setGameComplete(6);
                    returnToMenu();
                    return;
                }
            }
            last_tick = millis();
        }
    } else if (digitalRead(VERTICAL_KNOB_SWITCH) == HIGH && gas_held == true) {
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
    for (int i = 0; i < 4; i++) {
        if (atmo_done[i]) {
            display.fillRect(112, 2 + (8 * i), 6, 6, SSD1306_WHITE);
        }
    }
    display.display();
}

void startDownload() {
    last_tick = millis();
    dl_completed = 0;
}

void updateDownload() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW) {
        returnToMenu();
        return;
    }

    if (millis() - last_tick > dl_delay) {
        dl_completed++;
        last_tick = millis();
    }

    display.clearDisplay();
    display.drawRect(2, 6, 100, 20, SSD1306_WHITE);
    display.fillRect(2, 7, dl_completed - 1, 18, SSD1306_WHITE);
    display.setTextSize(1.5);
    display.setCursor(110, 12);
    display.print(dl_completed);
    display.display();

    if (dl_completed == 100) {
        delay(500);
        setGameComplete(node_state);
        returnToMenu();
    }
}

void startSoilCheck() {
    light_holding = false;
    light_base_threshold = analogRead(PHOTO_RESISTOR);
}

void updateSoilCheck() {
    if (analogRead(PHOTO_RESISTOR) > light_base_threshold + 100 && !light_holding) {
        light_holding = true;
        last_tick = millis();
    }
    else if (analogRead(PHOTO_RESISTOR) > light_base_threshold + 100 && light_holding) {
        if (millis() - last_tick > 100) {
            light_count++;
            last_tick = millis();
        }
    }
    else if (analogRead(PHOTO_RESISTOR) < light_base_threshold + 100 && light_holding) {
        light_holding = false;
    }

    display.clearDisplay();
    display.drawRect(2, 6, 100, 20, SSD1306_WHITE);
    display.fillRect(2, 7, light_count - 1, 18, SSD1306_WHITE);
    display.setTextSize(1.5);
    display.setCursor(110, 12);
    display.print(light_count);
    display.display();

    if (light_count == 100)
    {
        delay(500);
        setGameComplete(3);
        returnToMenu();
    }
}

void startLightCalibrate() {
    if (digitalRead(HORIZONTAL_KNOB_SWITCH) == LOW) {
        returnToMenu();
        return;
    }
    light_base_threshold = analogRead(PHOTO_RESISTOR);
    Serial.print("Base: ");
    Serial.println(light_base_threshold);
    light_count = 0;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(5, 12);
    display.print("Keep Dark");
    display.display();
    delay(1500);
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(5, 12);
    display.print("Shine Now");
    display.display();
    delay(2000);
    light_max_threshold = analogRead(PHOTO_RESISTOR);
    Serial.print("Max: ");
    Serial.println(light_max_threshold);
    if (light_max_threshold - light_base_threshold < 200) {
        startLightCalibrate();
    }
}

void updateLightCalibrate() {

    int light_percent = float(analogRead(PHOTO_RESISTOR) - light_base_threshold) / float(light_max_threshold - light_base_threshold) * 100;

    display.clearDisplay();
    display.drawRect(2, 6, 100, 20, SSD1306_WHITE);
    display.drawRect(42, 6, 20, 20, SSD1306_WHITE);
    display.fillRect(2, 7, light_percent - 1, 18, SSD1306_WHITE);
    display.setTextSize(1.5);
    display.setCursor(110, 12);
    display.print(light_count * 2);
    display.display();

    if (light_percent > 40 && light_percent < 60 && millis() - last_tick > 100) {
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

void startAirlockGame() {
    airlock_status[0] = 0;
    airlock_status[1] = 0;
    airlock_status[2] = 0;
}

void updateAirlockGame() {
    if (digitalSwitchRead(HORIZONTAL_KNOB_SWITCH)) {
        returnToMenu();
        return;
    }

    if (analogSwitchRead(AIRLOCK_1) && millis()-last_tick > 50) {
        last_tick = millis();
        airlock_status[0]++;
        if (airlock_status[0] > 100) {
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
    for (int i = 0; i < 3; i++) {
        if (airlock_status[i] < 100) {
            complete = false;
        }
    }

    if (complete == true) {
        display.clearDisplay();
        display.fillRect(42, 10, 6, 20, SSD1306_WHITE);
        display.fillRect(53, 2, 21, 6, SSD1306_WHITE);
        display.fillRect(78, 10, 6, 20, SSD1306_WHITE);
        display.fillCircle(63, 20, 10, SSD1306_WHITE);
        display.display();
        delay(2000);

        setGameComplete(9);
        returnToMenu();
        return;
    } else {
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

void startPumpGame() {
    pump_count = 1;
    pumps_needed = random(10);
    next_pump_ready = false;
}

void updatePumpGame() {
    if (digitalSwitchRead(HORIZONTAL_KNOB_SWITCH)) {
        returnToMenu();
        return;
    }

    if (analogSwitchRead(PUMP_1) && next_pump_ready) {
        pump_count++;
        next_pump_ready = false;
    }

    if (analogSwitchRead(PUMP_2) && !next_pump_ready) {
        next_pump_ready = true;
    }

    int percent_pumps = (float(pump_count) / pumps_needed) * 100;

    display.clearDisplay();
    display.drawRect(2, 6, 100, 20, SSD1306_WHITE);
    display.fillRect(2, 7, percent_pumps - 1, 18, SSD1306_WHITE);
    display.setTextSize(1.5);
    display.setCursor(110, 12);
    display.print(percent_pumps);
    display.display();

    if (percent_pumps >= 100)
    {
        setGameComplete(10);
        returnToMenu();
        return;
    }
}

void select_game(int game_id) {
    Serial.print("Selecting game: ");
    Serial.println(game_id);
    node_state = game_id;
    switch (game_id) {
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
        case 10:
            startPumpGame();
            break;
        default:
            startDefaultGame();
            break;
    }
}

void loop () {
    v_encoder.tick();
    h_encoder.tick();

    Serial.println(analogRead(AIRLOCK_1));

    if (node_state == 0) {
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
        if (digitalRead(VERTICAL_KNOB_SWITCH) == LOW && click_held == false && millis() - delay_start > 1000)
        {
            start_click = micros();
            click_held = true;
        }
        else if (digitalRead(VERTICAL_KNOB_SWITCH) == HIGH && click_held == true)
        {
            randomSeed(micros() - start_click);
            select_game(hover_select + 1);
            click_held = false;
        }

        if (analogSwitchRead(AIRLOCK_1) || analogSwitchRead(AIRLOCK_2) || analogSwitchRead(AIRLOCK_3)) {
            select_game(9);
        }

        if (analogSwitchRead(PUMP_1)) {
            select_game(10);
        }
    } else if (node_state == 1) {
        updateMarkObstacles();
    } else if (node_state == 2) {
        updatePanels();
    } else if (node_state == 3) {
        updateSoilCheck();
    } else if (node_state == 4 || node_state == 5) {
        updateDownload();
    }
     else if (node_state == 6) {
        updateAtmosphere();
    } else if (node_state == 7) {
        updateLightCalibrate();
    } else if (node_state == 9) {
        updateAirlockGame();
    } else if (node_state == 10) {
        updatePumpGame();
    }
     else {
        updateDefaultGame();
    }

    //Check for attachment of cells
    if (cells_finished == false && digitalRead(FUEL_CELLS) == HIGH && cells_connected == false) {
        cell_conn_start = millis();
        cells_connected = true;
    }
    else if (digitalRead(FUEL_CELLS) == HIGH && cells_finished==false &&cells_connected == true) {
        if (millis() - cell_conn_start > 1500) {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setTextSize(2);
            display.setCursor(5, 12);
            display.print("Cells Conn");
            display.display();
            cells_finished = true;
            setGameComplete(8);
            delay(2000);
            returnToMenu();
        }
    }
    else if (digitalRead(FUEL_CELLS) == LOW && cells_connected == true)
    {
        cells_connected = false;
    }

}