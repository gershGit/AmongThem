#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VERTICAL_KNOB_A A2
#define VERTICAL_KNOB_B A3
#define VERTICAL_KNOB_SWITCH 4
RotaryEncoder v_encoder(VERTICAL_KNOB_A, VERTICAL_KNOB_B);

#define HORIZONTAL_KNOB_A 9
#define HORIZONTAL_KNOB_B 10
RotaryEncoder h_encoder(9, 10);

int v_pos = 0;
int v_last;
int v_knob_button_state = LOW;
int h_pos = 0;
int h_last;

void setup() {
    pinMode(HORIZONTAL_KNOB_A, INPUT_PULLUP);
    pinMode(HORIZONTAL_KNOB_B, INPUT_PULLUP);
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

void loop () {
    v_encoder.tick();
    h_encoder.tick();
    int v_diff = v_encoder.getPosition() - v_last; 
    int h_diff = h_encoder.getPosition() - h_last;
    v_last = v_encoder.getPosition();
    h_last = h_encoder.getPosition();

    if (v_diff != 0) {
        v_pos += (v_diff*2);
            if (v_pos > 15) {
            v_pos = 15;
        } else if (v_pos < -16) {
            v_pos = -16;
        }
    }
    
    if (h_diff != 0) {
        h_pos += (h_diff*4);
        Serial.print("h_diff: ");
        Serial.print(h_diff);
        Serial.print(" h_pos: ");
        Serial.println(h_pos);
        if (h_pos > 63) {
            h_pos = 63;
        } else if (h_pos < -64) {
            h_pos = -64;
        }
    }

    if (digitalRead(VERTICAL_KNOB_SWITCH) != v_knob_button_state) {
        v_knob_button_state = !v_knob_button_state;
        if (v_knob_button_state == HIGH) {
            Serial.println("Released");
            display.clearDisplay();
            display.display();
        } else {
            Serial.println("Pressed");
            display.clearDisplay();
            display.fillRect(0, 0, 16, 16, SSD1306_WHITE);
            display.display();
        }
    } else {
        display.clearDisplay();
        display.drawFastVLine(h_pos+64, 0, display.height(), SSD1306_WHITE);
        display.drawFastHLine(0, v_pos+16, display.width(), SSD1306_WHITE);
        display.display();
    }
}