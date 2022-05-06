#include <SPI.h>
#include <PID_v1.h>
#include "MAX6675.h"
#include "AT24Cxx.h"
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "Adafruit_STMPE610.h"
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

// see https://circuitdigest.com/microcontroller-projects/arduino-pid-temperature-controller#:~:text=As%20the%20name%20suggests%20a,the%20current%20temperature%20and%20setpoint.
// TODO use relay style PID (see example in library)

// These are 'flexible' lines that can be changed
#define TFT_CS 1 // 10
#define TFT_DC 2 // 9
#define TFT_RST -1 // RST can be set to -1 if you tie it to Arduino's reset
#define STMPE_CS 3 // 8

// thermocouple pins (hardware SPI)
#define THERM_CS 0 // chip select (0)

// Use hardware SPI (on Nano, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

Adafruit_STMPE610 touch = Adafruit_STMPE610(STMPE_CS);

MAX6675 thermocouple;

// define EEPROM I2C address
#define EEPROM_IC_ADDR 0x50


void dashedHLine(int x, int y, int w, int dash_len, int color) {
    for(int i=x; i<(w); i+=(dash_len*2)) {
        tft.drawFastHLine(i, y, dash_len, color);
    }
}

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
  {1.4, 150, 175, 90, 0.84, 217, 250, 120, 4.4},
  {1, 100, 110, 40, 5, 138, 150, 20, 5},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
};
char *ProfileNames[3] = {"SAC305", "Sn/Bi57.6/Ag0.4", "bletch"};
int selected_profile = 0;

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
    const GFXfont* font;                    // font
    int alt_color = ORANGE;           // alt color
    int clear_color = WHITE;          // clear color

    struct {
        int x = 10;
        int y = 30;
    } txt_margin;    // text margin

    Button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, String txt, int color, const GFXfont* font = &FreeSerif18pt7b)
        : x(x), y(y), w(w), h(h), txt(txt), color(color), font(font)
        {}

    void update() {
        if (!pressed && is_pressed()) {
            render(alt_color, BLACK);
            return;
        }
        if (pressed && !is_pressed()) {
            render(color, BLACK);
            return;
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

    unsigned long start_time = 0;
    unsigned long delay_time = 100;

    bool is_pressed() {
        // enforce sampling interval
        if (millis() - start_time > delay_time) {
            start_time = millis();
        } else {
            return pressed;
        }

        if (disabled || !touch.touched()) {
            if (pressed) unpressed = true;
            pressed = false;
            return pressed;
        }

        uint16_t _x, _y;
        read_touch(_x, _y);
        if (is_within_bounds(_x, _y)) {
            pressed = true;
        } else {
            if (pressed) unpressed = true;
            pressed = false;
        }

        // if (pressed) {
        //     Serial.println("PRESSED");
        // } else {
        //     Serial.println("NOT PRESSED");
        // }

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

enum PHASE_IDX {
    STANDBY        = 0,
    PREHEAT        = 1,
    SOAK           = 2,
    REFLOW_PREHEAT = 3,
    REFLOW         = 4,
    COOLOFF        = 5,
    COMPLETE       = 6,
};
char* phase_names[7] = {"standby", "preheat", "soak", "preflow", "reflow", "cooloff", "done"};

class Phases {
public:
    // information about phase duration, start, end temp, etc.

    int txt_color = BLACK;
    int bg_color = WHITE;
    int x;
    int y;

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
    PHASE_IDX current_phase = STANDBY;

    Phases(int x, int y, int profile) : x(x), y(y) {
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
        reflow_rampup.start_temp = soak.end_temp;
        reflow_rampup.end_temp   = Profiles[prof][REFLOW_START_TEMP];
        reflow_rampup.rate       = Profiles[prof][REFLOW_RAMPUP];
        reflow_rampup.duration   = (reflow_rampup.end_temp - reflow_rampup.start_temp)/reflow_rampup.rate;
        reflow_rampup.peak_temp  = reflow_rampup.end_temp;

        // reflow
        reflow.start_temp = Profiles[prof][REFLOW_START_TEMP];
        reflow.end_temp   = Profiles[prof][REFLOW_START_TEMP];
        reflow.duration   = Profiles[prof][REFLOW_DURATION];
        reflow.peak_temp  = Profiles[prof][REFLOW_PEAK_TEMP];
        reflow.rate       = 2*(reflow.peak_temp - reflow.start_temp)/reflow.duration;

        // cooldown
        cooldown.start_temp = reflow.end_temp;
        cooldown.end_temp   = initial_temp;
        cooldown.rate       = Profiles[prof][COOLDOWN];
        cooldown.duration   = -(cooldown.end_temp - cooldown.start_temp)/cooldown.rate;
        cooldown.peak_temp  = reflow.end_temp;

        // total
        total_duration = preheat.duration + soak.duration + reflow_rampup.duration +reflow.duration + cooldown.duration;
    }

    void update(int time_elapsed) {
        PHASE_IDX current = PREHEAT;
        if (time_elapsed < preheat.duration) {
            current = PREHEAT;
        } else if (time_elapsed < preheat.duration + soak.duration) {
            current = SOAK;
        } else if (time_elapsed < preheat.duration + soak.duration + reflow_rampup.duration) {
            current = REFLOW_PREHEAT;
        } else if (time_elapsed < preheat.duration + soak.duration + reflow_rampup.duration + reflow.duration) {
            current = REFLOW;
        }  else if (time_elapsed < preheat.duration + soak.duration + reflow_rampup.duration + reflow.duration + cooldown.duration) {
            current = COOLOFF;
        } else if (time_elapsed > total_duration) {
            current = COMPLETE;
        }

        if (current_phase != current) {
            current_phase = current;
            render(current_phase);
        }

    }

    void render(int current_phase = 0) {
        tft.fillRect(x, y, 140, 40, bg_color);
        tft.setCursor(x + 10, y + 30);
        tft.setFont(&FreeSerif18pt7b);
        tft.setTextColor(txt_color);
        tft.println(phase_names[current_phase]);
    }
};

class Clock {
public:
    // count down clock
    int time_elapsed = 0;
    int time_remaining = 0;

    int txt_color = BLACK;
    int bg_color = WHITE;
    int x;
    int y;
    Clock(Phases* phases, int x, int y) : phases(phases), x(x), y(y) {
        reset();
    }

    void start() {
        reset();
        start_time = millis();
    }

    void reset() {
        time_elapsed = 0;
        time_remaining = phases->total_duration;
    }

    void update() {
        if (tick()) {
            render();
        }
    }

    bool tick() {
        if (time_remaining == 0) {
            return false;
        }

        if (millis() - start_time > 1000) {
            start_time = millis();
            time_elapsed++;
            time_remaining--;

            return true;
        }

        return false;
    }

    void render() {
        tft.fillRect(x, y, 80, 40, bg_color);
        tft.setCursor(x + 10, y + 30);
        tft.setFont(&FreeSerif18pt7b);
        tft.setTextColor(txt_color);
        tft.print(time_remaining/60);
        tft.print(":");
        if (time_remaining%60 < 10) tft.print("0");
        tft.print(time_remaining%60);
    }

private:
    Phases* phases;
    unsigned long start_time = 0;
};

// see ReflowWizard.h:261 from controleo code for important learned parameters
// see: https://www.whizoo.com/intelligent
// for description of scores and parameter learning.
enum Param {
    LearningComplete  = 0,
    LearnedPower      = 1, // duty cycle of all elements to keep oven at 120C (>20% is not good)
    LearnedInertia    = 2, // time needed to raise from 120C to 150C at 80% duty cycle
    LearnedInsulation = 3, // time needed to cool from 150C to 120C
    OvenScore         = 4,
};

class ParameterStore {
public:
    void init() {
        // initialize new EEPROM
        eep = new AT24Cxx(EEPROM_IC_ADDR, 32);  // 32 kB
    }

    uint8_t read(Param param) {
        uint8_t value;
        value = eep->read(param);
        Serial.print(param);
        Serial.print("\t");
        Serial.print(value, DEC);
        Serial.println();
    }

    void write(Param param, uint8_t value) {
        eep->update(param, value);
    }

private:
    AT24Cxx* eep;
};

class Temperature {
public:
    float current = 0;

    int txt_color = BLACK;
    int bg_color = WHITE;
    int x;
    int y;

    ParameterStore params;

    // temperature readings.
    Temperature(MAX6675* therm, int x, int y) : therm(therm), x(x), y(y) {}

    void begin() {
        pinMode(THERM_CS, OUTPUT);

        // begin thermocouple
        therm->begin(THERM_CS);

        // initialized parameter store (EEPROM)
        params.init();
    }

    bool read() {
        // according to documentation, give AT LEAST 250ms between reads.
        if (millis() - last_sample_time < 300) {
            return false;
        }

        // DEBUGGING
        // current = therm->readCelsius();
        therm->read();
        current = therm->getTemperature();

        last_sample_time = millis();

        return true;
    }

    void render() {
        tft.fillRect(x, y, 120, 40, bg_color);
        tft.setCursor(x + 10, y + 30);
        tft.setFont(&FreeSerif18pt7b);
        tft.setTextColor(txt_color);
        tft.print((int)current);
        tft.println(" C");
    }

    void adjust() {
        // TODO exit if 1 second has not yet elapsed.

        // TODO read current temp
        // TODO calculate expected temp

        // perform standard pid calculation: y(t) = (Kp*e) + (Ki*int(e(t), dt)) + (Kd*de/dt)
        // with a 1 second interval
        error = expectedTemp - actualTemp;
        integralTerm     = integralTerm + error;
        derivativeTerm   = error - proportionalTerm;
        proportionalTerm = error;

        // get base power output based on learned values
        output = getBaseOutputPower(expectedTemp, expectedTempDelta, bias, maxBias);

        // the heating elements in the oven are slow to respond to changes, so the most important term
        // in the PID equation will be the derivative term. The other terms will be lower.
        Kp = error < 0 ? 4 : 2;      // double the output proportionally to the error.
        Ki = 0.01;   // keep low to prevent oscillations.
        Kd = map(constrain(params.learnedInertia, 30, 100), 30, 100, 30, 75); // TODO base this off of the learned inertia. typically around 35.

        deltaOutput = kp*proportionalTerm + ki*integralTerm + kd*derivativeTerm;
        output += constrain(deltaOutput, -30, 30);
        output = constrain(output, 0, 100);

        // set duty cycle for each heating element
        for (uint8_t i = HEATING_ELEMENT_BOTTOM; i <= HEATING_ELEMENT_TOP; i++)
        {
            dutyCycle[i] = output * bias[i] / maxBias;
        }
    }

    void controlHeatingElements() {
        // TODO update heating elements based on duty cycles
    }

    void update() {
        adjust();  // adjust the power output for heating elements (run every 1 second)

        controlHeatingElements(); // turn power on/off for each heating element
    }

    uint16_t getBaseOutputPower(double temp, double incr, uint16_t *bias, uint16_t maxBias) {
        temp = constrain(temp, 20, 250);

        // determine power needed to maintain current temperature
        basePower = temp * 0.83 * params.learnedPower/100;
        insulationPower = map(params.learnedInsulation, 0, 300, map(temperature, 0, 400, 0, 20), 0);
        risePower = increment * basePower * 2;

        biasFactor = (float)2 * maxBias/(bias[HEATING_ELEMENT_BOTTOM] + bias[HEATING_ELEMENT_TOP]);

        totalPower = biasFactor * (basePower + insulationPower + risePower);
        return totalPower < 100 ? totalPower : 100;
    }
private:
    MAX6675* therm;
    unsigned long last_sample_time = 0;
};


class Plot {
public:
    int x, y, w, h;
    Phases* phases;

    struct {
        int margin_x = 18;
        int margin_y = 18;
        int width    = 15;
    } axes;

    struct {
        int bg               = WHITE;
        int axes             = BLACK;
        int thermal_profile  = BLUE;
        int measurements     = PINK;
        int current_phase_bg = YELLOW;
    } colors;

    struct {
        struct {
            float below_soak     = 0.3;
            float soak_to_reflow = 0.4;
            float reflow_to_peak = 0.3;
        } temp; // must sum to 1
        struct {
            float preheat        = 0.2;
            float soak           = 0.2;
            float reflow_preheat = 0.2;
            float reflow         = 0.2;
            float cooldown       = 0.2;
        } time; // must sum to 1
    } scaling;

    Plot(int x, int y, int w, int h, Phases* phases)
        : x(x), y(y), w(w), h(h), phases(phases) {}

    void render() {
        render_axes();
        render_thermal_profile();
    }

    void render_axes() {
        // make backgorund of axis
        tft.fillRect(x, y, w, h, colors.bg);

        int dash_len = 5;
        // === draw important temperatures and dashed lines ===
        tft.setFont(&FreeSerif9pt7b);
        tft.setTextColor(colors.axes);
        int font_offset = 5;

        // peak temp
        int peak_temp = scale_temp(phases->reflow.peak_temp);
        tft.setCursor(x, peak_temp + font_offset);
        tft.println((int)(phases->reflow.peak_temp));
        dashedHLine(axes.margin_x+axes.width, peak_temp, w - axes.width, dash_len, colors.axes);

        // // reflow temp (liquidus)
        int reflow_temp = scale_temp(phases->reflow.start_temp);
        tft.setCursor(x, reflow_temp + font_offset);
        tft.println((int)phases->reflow.start_temp);
        dashedHLine(axes.margin_x+axes.width, reflow_temp, w - axes.width, dash_len, colors.axes);

        // // soak temp
        int soak_temp = scale_temp(phases->soak.start_temp);
        tft.setCursor(x, soak_temp + font_offset);
        tft.println((int)phases->soak.start_temp);
        dashedHLine(axes.margin_x+axes.width, soak_temp, w - axes.width, dash_len, colors.axes);

        // // preheat
        int preheat_temp = scale_temp(phases->preheat.start_temp);
        tft.setCursor(x, preheat_temp + font_offset);
        tft.println((int)phases->preheat.start_temp);
        dashedHLine(axes.margin_x+axes.width, preheat_temp, w - axes.width, dash_len, colors.axes);

        // // preheat
        int zero_temp = scale_temp(0);
        tft.setCursor(x, zero_temp + font_offset);
        tft.println("  0");
        dashedHLine(axes.margin_x+axes.width, zero_temp, w - axes.width, dash_len, colors.axes);
    }

    void render_thermal_profile() {
        // start time
        int t = 0;

        // preheat phase
        tft.drawLine(scale_time(t),
                     scale_temp(0),
                     scale_time(t + phases->preheat.duration),
                     scale_temp(phases->preheat.end_temp),
                     colors.thermal_profile);
        t += phases->preheat.duration;

        // soak phase
        tft.drawLine(scale_time(t),
                     scale_temp(phases->soak.start_temp),
                     scale_time(t + phases->soak.duration),
                     scale_temp(phases->soak.end_temp),
                     colors.thermal_profile);
        t += phases->soak.duration;

        // reflow_rampup phase
        tft.drawLine(scale_time(t),
                     scale_temp(phases->reflow_rampup.start_temp),
                     scale_time(t + phases->reflow_rampup.duration),
                     scale_temp(phases->reflow_rampup.end_temp),
                     colors.thermal_profile);
        t += phases->reflow_rampup.duration;

        // reflow phase
        tft.drawLine(scale_time(t),
                     scale_temp(phases->reflow.start_temp),
                     scale_time(t + phases->reflow.duration/2),
                     scale_temp(phases->reflow.peak_temp),
                     colors.thermal_profile);
        t += phases->reflow.duration/2;
        tft.drawLine(scale_time(t),
                     scale_temp(phases->reflow.peak_temp),
                     scale_time(t + phases->reflow.duration/2),
                     scale_temp(phases->reflow.end_temp),
                     colors.thermal_profile);
        t += phases->reflow.duration/2;

        // cooldown phase
        tft.drawLine(scale_time(t),
                     scale_temp(phases->cooldown.start_temp),
                     scale_time(t + phases->cooldown.duration),
                     scale_temp(phases->cooldown.end_temp),
                     colors.thermal_profile);
    }

    void render_temperature_measurement(Temperature temp, Clock clk) {
        tft.fillCircle(scale_time(clk.time_elapsed), scale_temp(temp.current), 2, colors.measurements);
    }
private:
    int scale_temp(float temp) {
        float range_percent, range_scale, range_offset;
        float H = h - (2*axes.margin_y);
        if (temp < phases->soak.start_temp) {
            // preheat
            range_percent = temp / phases->soak.start_temp;
            range_scale = scaling.temp.below_soak;
            range_offset = 0;
        } else if (temp >= phases->soak.start_temp && temp < phases->reflow.start_temp) {
            // soak
            range_percent = (temp - phases->soak.start_temp) / (phases->reflow.start_temp - phases->soak.start_temp);
            range_scale = scaling.temp.soak_to_reflow;
            range_offset = scaling.temp.below_soak;
        } else if (temp >= phases->reflow.start_temp) {
            // reflow
            range_percent = (temp - phases->reflow.start_temp) / (phases->reflow.peak_temp - phases->reflow.start_temp);
            range_scale = scaling.temp.reflow_to_peak;
            range_offset = scaling.temp.below_soak + scaling.temp.soak_to_reflow;
        }

        temp = H * (1 - (range_percent*range_scale + range_offset));

        return y + axes.margin_y + temp;
    }

    int scale_time(float t) {
        return x + axes.margin_x + axes.width + ((w - (2*axes.margin_x + axes.width)) * (t/phases->total_duration));
    }
};

Button start_button(10, 430, 160, 40, "START", WHITE);
Button profile_button(0, 0, tft.width(), 40, ProfileNames[selected_profile], WHITE, &FreeSerif18pt7b);
Phases phases(10, 380, selected_profile);
Plot plot(0, 40, tft.width(), tft.height()*0.70, &phases);
Clock clock(&phases, 200, 380);
Temperature temperature(&thermocouple, 200, 430);


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

  temperature.begin();

  tft.setRotation(0);  // make screen hamburger style

  // configure gui stuff
  profile_button.txt_margin.x = 10;
  profile_button.txt_margin.y = 30;

  render_loading_screen();

  tft.fillScreen(WHITE);
  render_main_screen();

  // DEBUG
  // temperature.params.write(LearningComplete, 255);
  // temperature.params.write(LearnedPower, 88);
  // temperature.params.write(LearnedInertia, 99);
  // temperature.params.write(LearnedInsulation, 100);
  // temperature.params.write(OvenScore, 90);
  temperature.params.read(LearningComplete);
  temperature.params.read(LearnedPower);
  temperature.params.read(LearnedInertia);
  temperature.params.read(LearnedInsulation);
  temperature.params.read(OvenScore);

}

void loop() {
  // put your main code here, to run repeatedly:
  switch (state) {
    case MAIN_NOT_RUNNING:
        start_button.update();
        if (start_button.is_unpressed()) {
            plot.render();
            start_button.txt = "ABORT";
            state = MAIN_RUNNING;
            clock.start();
            start_button.render();
        }
        break;
    case MAIN_RUNNING:
        clock.update();
        start_button.update();
        temperature.update(); // TODO this is where PID code should go.
        phases.update(clock.time_elapsed);
        if (start_button.is_unpressed()) {
            plot.render();
            start_button.txt = "START";
            state = MAIN_NOT_RUNNING;
            clock.reset();
            clock.render();
            start_button.render();
            phases.render(0);
            phases.current_phase = STANDBY;
        }
        if (clock.time_remaining == 0) {
            start_button.txt = "RESTART";
            start_button.render();
            state = MAIN_NOT_RUNNING;
        }
        break;
  }

  if (temperature.read()) {
      temperature.render();
      plot.render_temperature_measurement(temperature, clock);
  }

}

void render_main_screen() {
    // int selected_profile = 0;
    main_screen_current_profile(selected_profile);
    plot.render();
    phases.render();
    clock.render();
    if (temperature.read()) {
        temperature.render();
        plot.render_temperature_measurement(temperature, clock);
    }

    if (state == MAIN_NOT_RUNNING) {
        start_button.render();
    }

    return;
}

void main_screen_current_profile(int selected_profile) {
    tft.setCursor(10, 30);
    tft.setFont(&FreeSerif18pt7b);
    tft.setTextColor(GREEN);
    tft.println(ProfileNames[selected_profile]);
}


//:::::: LOADING SCREEN
//::::::::::::::::::::::::::

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
