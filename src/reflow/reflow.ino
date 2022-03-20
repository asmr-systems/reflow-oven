#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "Adafruit_STMPE610.h"
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

// These are 'flexible' lines that can be changed
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST -1 // RST can be set to -1 if you tie it to Arduino's reset
#define STMPE_CS 8

// Use hardware SPI (on Nano, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

Adafruit_STMPE610 touch = Adafruit_STMPE610(STMPE_CS);


// Color Palette (RGB565)
// Use this https://chrishewett.com/blog/true-rgb565-colour-picker/
// as a color picker.
#define PINK 0xfb14
#define ORANGE 0xe2a3
#define YELLOW 0xe743
#define VIOLET 0xa97a
#define GREEN 0x27a4
#define BLUE 0x23dd
#define RED 0xe2ab
#define BLACK 0x0000
#define WHITE 0xFFFF

// indices (eventually store in EEPROM)
#define PREHEAT_RAMPUP    0  // C/s
#define SOAK_START_TEMP   1  // C
#define SOAK_END_TEMP     2  // C
#define SOAK_DURATION     3  // s
#define REFLOW_RAMPUP     4  // C/s
#define REFLOW_START_TEMP 5  // C
#define REFLOW_PEAK_TEMP  6  // C
#define REFLOW_DURATION   7  // s
#define COOLDOWN          8  // C/s
float Profiles[3][9] = {
  {1, 100, 110, 40, 5, 138, 150, 20, 5},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
};

char *ProfileNames[3] = {"Sn42/Bi57.6/Ag0.4", "bar", "bletch"};

// finite state machine
enum STATES {
  MAIN_NOT_RUNNING,
  MAIN_RUNNING,
  CHOOSE_PROFILE,
  EDIT_PROFILE,
};
STATES state = MAIN_NOT_RUNNING;

class Button {
public:
    uint16_t x;                       // x origin
    uint16_t y;                       // y origin
    uint16_t w;                       // width
    uint16_t h;                       // height
    String txt;                       // text
    int color;                        // color
    GFXfont* font;                    // font
    int alt_color = ORANGE;           // alt color
    int clear_color = WHITE;          // clear color

    struct {
        int x = 10;
        int y = 40;
    } txt_margin;    // text margin

    Button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, String txt, int color, GFXfont* font = &FreeSerif18pt7b)
        : x(x), y(y), w(w), h(h), txt(txt), color(color), font(font)
        {}

    void update() {
        if (is_pressed()) {
            render(alt_color, BLACK);
        }
    }

    void render() {
        render(color, BLACK);
    }
    void render(int _color, int txt_color) {
        tft.fillRect(x, y, w, h, _color);
        tft.setCursor(x + txt_margin.x, y + txt_margin.y);
        tft.setFont(&FreeSerif18pt7b);
        tft.setTextColor(txt_color);
        tft.println(txt);
    }

    void clear() {
        tft.fillRect(x, y, w, h, clear_color);
    }

    bool is_unpressed() {
        if (unpressed) {
            unpressed = false;
            return true;
        }
        return unpressed;
    }

    void disable() { disabled = true; }
    void enable() { disabled = false; }

private:
    bool pressed = false;
    bool unpressed = false;
    bool disabled = false;            // button is disabled

    bool is_pressed() {
        if (disabled) return false;

        uint16_t _x, _y;
        read_touch(_x, _y);
        if (is_within_bounds(_x, _y)) {
            pressed = true;
        } else {
            if (pressed) unpressed = true;
            pressed = false;
        }
        return pressed;
    }

    void read_touch(uint16_t& _x, uint16_t& _y) {
        uint8_t z;
        while (! touch.bufferEmpty()) {
            touch.readData(&_x, &_y, &z);
        }

        // max (3681, 3751)
        // min (558, 364)
        _x = constrain(map(_x, 558,3681, 0, 320), 0, 320);
        _y = constrain(map(_y, 364, 3751, 0, 480), 0, 480);

        return;
    }

    bool is_within_bounds(uint16_t _x, uint16_t _y) {
        if (_x > x && _x < (x + w) && _y > y && _y < (y+h)) return true;
        return false;
    }

};

class Clock {
public:
    // count down clock
};

class Temperature {
public:
    // temperature readings.
};

class Phases {
public:
    // information about phase duration, start, end temp, etc.

    struct phase {
        int start_temp; // in Celcius
        int end_temp;   // in Celcius
        float rate;     // in C/s
        int duration;   // in seconds
        int peak_temp;  // in Celcius
    };

    phase preheat;
    phase soak;
    phase reflow_rampup;
    phase reflow;           // temperature above liquidus
    phase cooldown;

    int total_duration;

    Phases(int profile) {
        calculate_from_profile(profile);
    }

    void calculate_from_profile(int prof, float initial_temp = 25.0) {

        // pre-heat
        preheat.start_temp = initial_temp;
        preheat.end_temp   = Profiles[prof][SOAK_START_TEMP];
        preheat.rate       = Profiles[prof][PREHEAT_RAMPUP];
        preheat.duration   = (Profiles[prof][SOAK_START_TEMP] - preheat.start_temp)/Profiles[prof][PREHEAT_RAMPUP];
        preheat.peak_temp  = preheat.end_temp;

        // soak
        soak.start_temp = Profiles[prof][SOAK_START_TEMP];
        soak.end_temp   = Profiles[prof][SOAK_END_TEMP];
        soak.rate       = (soak.end_temp-soak.start_temp)/soak.duration;
        soak.duration   = Profiles[prof][SOAK_DURATION];
        soak.peak_temp  = soak.end_temp;

        // reflow_ramp
        reflow_ramp.start_temp = soak.end_temp;
        reflow_ramp.end_temp   = Profiles[prof][REFLOW_START_TEMP];
        reflow_ramp.rate       = Profiles[prof][REFLOW_RAMPUP];
        reflow_ramp.duration   = (reflow_ramp.end_temp - reflow_ramp.start_temp)/reflow_ramp.rate;
        reflow_ramp.peak_temp  = reflow_ramp.end_temp;

        // reflow
        reflow.start_temp = Profiles[prof][REFLOW_START_TEMP];
        reflow.end_temp   = Profiles[prof][REFLOW_START_TEMP];
        reflow.duration   = Profiles[prof][REFLOW_DURATION];
        reflow.peak_temp  = Profiles[prof][REFLOW_PEAK];
        reflow.rate       = 2*(reflow.peak_temp - reflow.start_temp)/reflow.duration;

        // cooldown
        cooldown.start_temp = reflow.end_temp;
        cooldown.end_temp   = initial_temp;
        cooldown.rate       = Profiles[prof][COOLDOWN];
        cooldown.duration   = (cooldown.end_temp - cooldown.start_temp)/cooldown.rate;
        cooldown.peak_temp  = reflow.end_temp;

        // total
        total_duration = preheat.duration + soak.duration + reflow_ramp.duration +reflow.duration + cooldown.duration;
    }
}

Button start_button = Button(10, 400, 150, 60, "Start", WHITE);
Button profile_button = Button(0, 0, tft.width(), 40, ProfileNames[selected_profile], WHITE, &FreeSerif18pt7b);
Phases phases(selected_profile);


void setup() {
  // allow some time before initializing the display
  // otherwise it seems to invert the image on powerup.
  delay(50);

  // DEBUG
  Serial.begin(9600);

  // I don't know why this needs to be pin 10.
  // if it is gone, it doesn't work. or if it is another pin
  // it doens't work.
  pinMode(10, OUTPUT);

  // begin capacitive touch sensor and lcd screen
  touch.begin();
  tft.begin();

  tft.setRotation(0);  // make screen hamburger style

  // configure gui stuff
  profile_button.txt_margin.x = 10;
  profile_button.txt_margin.y = 30;

  render_loading_screen();

  tft.fillScreen(PINK);
  render_main_screen();

}

void loop() {
  // put your main code here, to run repeatedly:
  switch (state) {
    case MAIN_NOT_RUNNING:
    case MAIN_RUNNING:
      handle_main_screen();
      break;
  }
}

void handle_main_screen() {
  // listen for touches
  uint16_t x, y;

  if (touch.touched()) {
    read_touch(x, y);
    Serial.print("(");
    Serial.print(x); Serial.print(", ");
    Serial.print(y);
    Serial.println(")");

    start_button.update();

    // if in boundaries of buttons
    if (state == MAIN_NOT_RUNNING && is_within_start_button(x, y)) {
      state = MAIN_RUNNING;
    }
    if (state == MAIN_RUNNING && is_within_start_button(x, y)) {
      state = MAIN_NOT_RUNNING;
    }
  }

  return;
}

void read_touch(uint16_t& x, uint16_t& y) {
  uint8_t z;
  while (! touch.bufferEmpty()) {
    touch.readData(&x, &y, &z);
  }

  // max (3681, 3751)
  // min (558, 364)
  x = constrain(map(x, 558,3681, 0, 320), 0, 320);
  y = constrain(map(y, 364, 3751, 0, 480), 0, 480);

   return;
}

bool is_within_start_button(uint16_t x, uint16_t y) {
  if (x > 10 && x < 160 && y > 400 && y < 460) return true;
  return false;
}

void render_loading_screen() {
  tft.fillScreen(WHITE);

  int rect_x_origin = tft.width()/2 - 60;
  int rect_y_origin = tft.height()/2 - 30;
  tft.drawRect(rect_x_origin, rect_y_origin, 30, 30, PINK);

  int text_x_origin = tft.width()/2 - 25;
  int text_y_origin = tft.height()/2 - 10;
  tft.setCursor(text_x_origin, text_y_origin);
  tft.setFont(&FreeSerif18pt7b);
  tft.setTextColor(RED);
  tft.println("ASMR");

  delay(100);
  int colors[5] = {PINK, ORANGE, YELLOW, GREEN, VIOLET};
  for (int i = 1; i < 40; i++) {
      tft.drawRect(rect_x_origin - i*10, rect_y_origin - i*10, 30 + i*20, 30 + i*20, PINK + i*50);
      if (i < 5) {
        tft.setCursor(text_x_origin + i*15, text_y_origin+i*15);
        tft.setTextColor(colors[i]);
        tft.println("ASMR");
        delay(100);
      } else{
        tft.setCursor(rect_x_origin, rect_y_origin + 200);
        tft.setTextColor(BLUE);
        switch (i) {
          case 5:
          tft.println("R");
          break;
          case 6:
          tft.println("R E");
          break;
          case 7:
          tft.println("R E F");
          break;
          case 8:
          tft.println("R E F L");
          break;
          case 9:
          tft.println("R E F L O");
          break;
          case 10:
          tft.println("R E F L O W");
          break;
          default:
          tft.println("R E F L O W");
          break;
        }
        delay(50);
      }
  }

  delay(500);
}

void render_main_screen() {
  int selected_profile = 0;
  main_screen_current_profile(selected_profile);
  profile_plot(0, 40, tft.width(), tft.height()*0.70, selected_profile);

  if (state == MAIN_NOT_RUNNING) {
      start_button.render();
    // render_start_button();
  }

  return;
}

void render_start_button() {
  tft.fillRect(10, 400, 150, 60, WHITE);
  tft.setCursor(20, 440);
  tft.setFont(&FreeSerif18pt7b);
  tft.setTextColor(BLACK);
  tft.println("START");

  return;
}

void main_screen_current_profile(int selected_profile) {
  tft.setCursor(10, 30);
  tft.setFont(&FreeSerif18pt7b);
  tft.setTextColor(GREEN);
  tft.println(ProfileNames[selected_profile]);
}

void profile_plot(int x, int y, int w, int h, int selected_profile) {
  int margin_y = 18; int margin_x = 18; int axis_width = 15;
  profile_plot_axis(x, y, w, h, margin_x, margin_y, axis_width, selected_profile);
  profile_plot_curve(x+margin_x+axis_width, y+margin_y, w-margin_x-2*axis_width, h-2*margin_y, selected_profile);
}

void profile_plot_curve(int x, int y, int w, int h, int selected_profile) {
  float preheat_duration = Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][PREHEAT_RAMPUP];
  float soak_duration = Profiles[selected_profile][SOAK_DURATION];
  float reflow_ramp_duration = (Profiles[selected_profile][REFLOW_START_TEMP]-Profiles[selected_profile][SOAK_END_TEMP])/Profiles[selected_profile][REFLOW_RAMPUP];
  float reflow_duration = Profiles[selected_profile][REFLOW_DURATION];
  float cooldown_duration = Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][COOLDOWN];
  float total_duration = preheat_duration + soak_duration+reflow_ramp_duration+reflow_duration+cooldown_duration;


  int x_s = x; int x_e = x_s + (w*(preheat_duration/total_duration));
  tft.drawLine(x_s, y+h, x_e, y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);

  x_s = x_e; x_e = x_s + (w*(soak_duration/total_duration));
  tft.drawLine(x_s, y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))),x_e, y+(h - (h* (Profiles[selected_profile][SOAK_END_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);

  x_s = x_e; x_e = x_s + (w*(reflow_ramp_duration/total_duration));
  tft.drawLine(x_s, y+(h - (h* (Profiles[selected_profile][SOAK_END_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))),x_e, y+(h - (h* (Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);

  x_s = x_e; x_e = x_s + (w*((reflow_duration/2)/total_duration));
  tft.drawLine(x_s, y+(h - (h* (Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))),x_e, y+(h - (h* (Profiles[selected_profile][REFLOW_PEAK_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);
  x_s = x_e; x_e = x_s + (w*((reflow_duration)/total_duration));
  tft.drawLine(x_s, y+(h - (h* (Profiles[selected_profile][REFLOW_PEAK_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))),x_e, y+(h - (h* (Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);

  x_s = x_e; x_e = x_s + (w*((cooldown_duration)/total_duration));
  tft.drawLine(x_s, y+(h - (h* (Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))),x_e, y+(h), PINK);

  tft.drawLine(x, y+h, x+(w*(preheat_duration/total_duration)), y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);
  tft.drawLine(x, y+h, x+(w*(preheat_duration/total_duration)), y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);
  tft.drawLine(x, y+h, x+(w*(preheat_duration/total_duration)), y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);
  tft.drawLine(x, y+h, x+(w*(preheat_duration/total_duration)), y+(h - (h* (Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP]))), PINK);

}

void profile_plot_axis(int x, int y, int w, int h, int margin_x, int margin_y, int axis_width, int selected_profile) {
  // make backgorund of axis
  tft.fillRect(x, y, w, h, BLACK);

  int dashed_line_len = 5;
  // === draw important temperatures and dashed lines ===
  tft.setFont(&FreeSerif9pt7b);
  tft.setTextColor(PINK);

  // peak temp
  int peak_temp = y+margin_y;
  tft.setCursor(x, peak_temp);
  tft.println((int)Profiles[selected_profile][REFLOW_PEAK_TEMP]);
  for(int i=margin_x+axis_width; i<(w); i+=(dashed_line_len*2)) tft.drawFastHLine(i, peak_temp, dashed_line_len, GREEN);

  // reflow temp (liquidus)
  int reflow_temp = y+margin_y+((h-2*margin_y)-(Profiles[selected_profile][REFLOW_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP])*(h-2*margin_y));
  tft.setCursor(x, reflow_temp);
  tft.println((int)Profiles[selected_profile][REFLOW_START_TEMP]);
  for(int i=margin_x+axis_width; i<(w); i+=(dashed_line_len*2)) tft.drawFastHLine(i, reflow_temp, dashed_line_len, GREEN);

  // soak temp
  int soak_temp = y+margin_y+((h-2*margin_y)-(Profiles[selected_profile][SOAK_START_TEMP]/Profiles[selected_profile][REFLOW_PEAK_TEMP])*(h-2*margin_y));
  tft.setCursor(x, soak_temp);
  tft.println((int)Profiles[selected_profile][SOAK_START_TEMP]);
  for(int i=margin_x+axis_width; i<(w); i+=(dashed_line_len*2)) tft.drawFastHLine(i, soak_temp, dashed_line_len, GREEN);

  // zero
  int zero_temp = y+margin_y+((h-2*margin_y)-(0/Profiles[selected_profile][REFLOW_PEAK_TEMP])*(h-margin_y));
  tft.setCursor(x, zero_temp);
  tft.println("   0");
  for(int i=margin_x+axis_width; i<(w); i+=(dashed_line_len*2)) tft.drawFastHLine(i, zero_temp, dashed_line_len, GREEN);
}
