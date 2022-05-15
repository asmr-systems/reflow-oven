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

enum HEATING_ELEMENTS {
    HEATING_ELEMENT_BOOST  = 0,
    HEATING_ELEMENT_BOTTOM = 1,
    HEATING_ELEMENT_TOP    = 2,
};

uint8_t HEATING_ELEMENT_CS = 7;

// These are 'flexible' lines that can be changed
#define TFT_CS 2 // 10
#define TFT_DC 1 // 9
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
// NOTE: this is a modified version of the AT24Cxx object which has a
// begin() method.
// AT24Cxx eep = AT24Cxx(EEPROM_IC_ADDR, 32);

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
#define GREY 0xee5b
#define GOOD_RATING_COLOR 0x0d0c
#define OKAY_RATING_COLOR 0xedc7
#define BAD_RATING_COLOR 0xe883

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
float Profiles[4][9] = {
  {1.4, 150, 175, 90, 0.84, 217, 250, 120, 4.4},
  {1, 100, 110, 40, 5, 138, 150, 20, 5},
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {
      1,
      120,
      120,
      2400,
      4,
      120,
      150,
      360,
      360,
  },
};
char *ProfileNames[3] = {"SAC305", "Sn/Bi57.6/Ag0.4", "bletch"};
int selected_profile = 0;
int learning_profile = 3;

// finite state machine
enum STATES {
    STARTUP,
    MENU,
    LEARNING_DASHBOARD,
    LEARNING_CYCLE,
  MAIN_NOT_RUNNING,
  MAIN_RUNNING,
  CHOOSE_PROFILE,
  EDIT_PROFILE,
};
STATES state = STARTUP;

class Button {
public:
    uint16_t x;                       // x origin
    uint16_t y;                       // y origin
    uint16_t w;                       // width
    uint16_t h;                       // height
    String txt;                       // text
    int color;                        // color
    const GFXfont* font;                    // font
    int alt_color = YELLOW;           // alt color
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
        if (is_within_bounds((int)_x, (int)_y)) {
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

    void disable() { disabled = true; }
    void enable() { disabled = false; }

private:
    bool pressed = false;
    bool unpressed = false;
    bool disabled = false;            // button is disabled

    unsigned long start_time = 0;
    unsigned long delay_time = 80;

    uint16_t y_offset = 20;
    uint16_t x_offset = 30;

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

    bool is_within_bounds(int _x, int _y) {
        if (_x > (x - x_offset) && _x < (x + w + x_offset) && _y > (y - y_offset) && _y < (y + y_offset +h)) return true;
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
    int total_duration;
    // count down clock
    int time_elapsed = 0;
    int time_remaining = 0;

    int txt_color = BLACK;
    int bg_color = WHITE;
    int x;
    int y;
    Clock(int total_duration, int x, int y) : total_duration(total_duration), x(x), y(y) {
        reset();
    }

    void start() {
        reset();
        start_time = millis();
    }

    void reset() {
        time_elapsed = 0;
        time_remaining = total_duration;
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
        tft.fillRect(x, y, 90, 40, bg_color);
        tft.setCursor(x + 10, y + 30);
        tft.setFont(&FreeSerif18pt7b);
        tft.setTextColor(txt_color);
        tft.print(time_remaining/60);
        tft.print(":");
        if (time_remaining%60 < 10) tft.print("0");
        tft.print(time_remaining%60);
    }

private:
    unsigned long start_time = 0;
};

// see ReflowWizard.h:261 from controleo code for important learned parameters
// see: https://www.whizoo.com/intelligent
// for description of scores and parameter learning.
// NOTE: looking at the latest Controleo code, it doesn't look like the parameters
// for the individual elements (power and inertia) are important or used for anything.
// mainly, we might just have to have a bias for each heating element (its minumum and max
// duty cycle)
enum Param {
    // Has the learning cycle fully completed?
    LearningComplete  = 0,

    // Percentage of time all elements (combination) stay on to maintain
    // a constant temperature of 120C.
    // =< 12% Good
    // 12% < && < 30% OKAY
    // => 30% BAD
    LearnedPower      = 1,

    // Time needed to increase temperature by 30C at 80% duty cycle (high duty cycle)
    // =< 36 seconds Good
    // 36 < && < 60 seconds OKAY
    // 60 =< BAD
    LearnedInertia    = 2,

    // Time (in seconds) it takes to cool from 150C to 120C (all elements off)
    // >= 130 seconds GOOD
    // < 130 && > 80 OKAY
    // <= 80 seconds BAD
    LearnedInsulation = 3,

    // Overall score for oven
    // weighting is:
    // 40% Power
    // 40% Inertia
    // 20% Insulation
    OvenScore         = 4,
};

enum ParamRating {
    GOOD_RATING  = 0,
    OKAY_RATING  = 1,
    BAD_RATING   = 2,
};

class ParameterStore {
public:
    // TODO fill in all parameters
    bool  learningComplete  = false;
    float learnedPower      = 0;
    float learnedInertia    = 0;
    float learnedInsulation = 0;
    float ovenScore         = 0;

    String name[5] = {
        "Is Complete",
        "Power",
        "Inertia",
        "Insulation",
        "Overall Score",
    };

    String units[5] = {
        "-",
        "%",
        "s",
        "s",
        "%",
    };

    float index[5];

    void init() {
        // initialize new EEPROM
        eep = new AT24Cxx(EEPROM_IC_ADDR, 32);  // 32 kB

        // populate parameters initially
        learningComplete  = (bool)(read(LearningComplete));
        learnedPower      = (float)(read(LearnedPower));
        learnedInertia    = (float)(read(LearnedInertia));
        learnedInsulation = (float)(read(LearnedInsulation));
        ovenScore         = (float)(read(OvenScore));

        // populate index
        index[LearningComplete] = 0; // dummy
        index[LearnedPower] = learnedPower;
        index[LearnedInertia] = learnedInertia;
        index[LearnedInsulation] = learnedInsulation;
        index[OvenScore] = ovenScore;
    }

    uint8_t read(Param param) {
        return eep->read(param);
    }

    void write(Param param, uint8_t value) {
        eep->update(param, value);
    }

    ParamRating rate(Param param) {
        return rate(param, index[param]);
    };

    ParamRating rate(Param param, float value) {
        float okay_threshold, good_threshold;

        switch (param) {
        case LearnedPower:
            good_threshold = 12;
            okay_threshold = 35;
            if (value <= good_threshold) return GOOD_RATING;
            if (value < okay_threshold) return OKAY_RATING;
            break;
        case LearnedInertia:
            good_threshold = 90;
            okay_threshold = 60;
            if (value > good_threshold) return GOOD_RATING;
            if (value > okay_threshold) return OKAY_RATING;
            break;
        case LearnedInsulation:
            good_threshold = 90;
            okay_threshold = 60;
            if (value > good_threshold) return GOOD_RATING;
            if (value > okay_threshold) return OKAY_RATING;
            break;
        case OvenScore:
            good_threshold = 90;
            okay_threshold = 60;
            if (value > good_threshold) return GOOD_RATING;
            if (value > okay_threshold) return OKAY_RATING;
            break;
        }

        return BAD_RATING;
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

    // power bias for each heating element
    uint16_t bias[3] = {
        0, // BOOST
        0, // BOTTOM
        0, // TOP
    };

    const uint16_t maxBias = 10;

    // duty cycle for each heating element
    double dutyCycle[3] = {
        50, // BOOST
        50, // BOTTOM
        50, // TOP
    };

    float powerPeriod = 1000; // 1000 ms

    // temperature readings.
    Temperature(MAX6675* therm, int x, int y) : therm(therm), x(x), y(y) {}

    void begin() {
        pinMode(THERM_CS, OUTPUT);
        pinMode(HEATING_ELEMENT_CS, OUTPUT);

        SPI.begin();

        // turn off all heaters.
        off();

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
        static double proportionalTerm = 0;

        // TODO exit if 1 second has not yet elapsed.

        // TODO read current temp
        double actualTemp = 0;
        // TODO calculate expected temp
        double expectedTemp = 0;
        // TODO calculate expectedTempDelta?
        double expectedTempDelta = 1;

        // perform standard pid calculation: y(t) = (Kp*e) + (Ki*int(e(t), dt)) + (Kd*de/dt)
        // with a 1 second interval
        double error = expectedTemp - actualTemp;
        double integralTerm     = integralTerm + error;
        double derivativeTerm   = error - proportionalTerm;
        proportionalTerm = error;

        // get base power output based on learned values
        double output = getBaseOutputPower(expectedTemp, expectedTempDelta, bias, maxBias);

        // the heating elements in the oven are slow to respond to changes, so the most important term
        // in the PID equation will be the derivative term. The other terms will be lower.
        double Kp = error < 0 ? 4 : 2;      // double the output proportionally to the error.
        double Ki = 0.01;   // keep low to prevent oscillations.
        double Kd = map(constrain(params.learnedInertia, 30, 100), 30, 100, 30, 75); // TODO base this off of the learned inertia. typically around 35.

        // compute PID equation
        double deltaOutput = Kp*proportionalTerm + Ki*integralTerm + Kd*derivativeTerm;
        output += constrain(deltaOutput, -30, 30);
        output = constrain(output, 0, 100);

        // set duty cycle for each heating element
        for (uint8_t i = HEATING_ELEMENT_BOOST; i <= HEATING_ELEMENT_TOP; i++)
        {
            dutyCycle[i] = output * bias[i] / maxBias;
        }
    }

    void controlHeatingElements() {
        // NOTE: Heating Elements turn on (SSRs Active) when
        // Heating Element Pins go LOW
        static int lastCycleStartTime = 0;

        if ((millis() - lastCycleStartTime) > powerPeriod) {
            lastCycleStartTime = millis();
            // turn on all elements
            setHeatingElements(false, false, false);
            return;
        }

        bool off[3] = {false, false, false};

        for (int i = 0; i < 3; i++) {
            if ((millis()-lastCycleStartTime) > (dutyCycle[i]*powerPeriod)) {
                off[i] = true;
            }
        }

        setHeatingElements(off[0], off[1], off[2]);
    }

    void setDutyCycles(float boost, float bottom, float top) {
        dutyCycle[0] = boost;
        dutyCycle[1] = bottom;
        dutyCycle[2] = top;
    }

    void update() {
        const int dt = 1000; // 1s
        static int start_time   = 0;

        if (millis() - start_time > dt)
        {
            start_time = millis();

            //adjust();  // adjust the power output for heating elements (run every 1 second)

            controlHeatingElements(); // turn power on/off for each heating element
        }
    }

    uint16_t getBaseOutputPower(double temp, double incr, uint16_t *bias, uint16_t maxBias) {
        temp = constrain(temp, 20, 250);

        // determine power needed to maintain current temperature
        double basePower = temp * 0.83 * params.learnedPower/100;
        double insulationPower = map(params.learnedInsulation, 0, 300, map(temp, 0, 400, 0, 20), 0);
        double risePower = incr * basePower * 2;

        float biasFactor = (float)2 * maxBias/(bias[HEATING_ELEMENT_BOTTOM] + bias[HEATING_ELEMENT_TOP]);

        double totalPower = biasFactor * (basePower + insulationPower + risePower);
        return totalPower < 100 ? totalPower : 100;
    }

    void setHeatingElements(bool boost, bool bottom, bool top) {
        // 0x03 is just top, 0x05 just bottom, 0x06 is just boost
        // It should work with all three on at the same time!
        uint8_t bits = 0x00;
        bits |= (boost  ? 0 : 1);
        bits |= (bottom ? 0 : 1) << 1;
        bits |= (top    ? 0 : 1) << 2;

        digitalWrite(HEATING_ELEMENT_CS, LOW);
        SPI.transfer(bits);
        digitalWrite(HEATING_ELEMENT_CS, HIGH);
    }

    void off() {
        setHeatingElements(false, false, false);
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
        int bg               = YELLOW; //WHITE;
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

    bool noPhases = false;

    Plot(int x, int y, int w, int h, Phases* phases)
        : x(x), y(y), w(w), h(h), phases(phases) {}

    Plot(int x, int y, int w, int h, Phases* phases, bool noPhases)
        : x(x), y(y), w(w), h(h), phases(phases), noPhases(noPhases) {}


    void render() {
        render_axes();
        if (!noPhases)
            render_thermal_profile();
    }

    void renderLineAt(int temp, int color = BLUE) {
        int dash_len = 5;
        int scaled_temp = scale_temp(temp);
        dashedHLine(axes.margin_x+axes.width, scaled_temp, w - axes.width, dash_len, color);
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

Button gotoMenuButton(280, 5, 40, 40, "X", GREY);
Button start_button(10, 430, 160, 40, "START", GREY);
Button profile_button(0, 0, tft.width(), 40, ProfileNames[selected_profile], WHITE, &FreeSerif18pt7b);
Phases phases(10, 380, selected_profile);
Plot plot(0, 40, tft.width(), tft.height()*0.70, &phases);
Clock clock(phases.total_duration, 200, 380);
Temperature temperature(&thermocouple, 200, 430);

class MainMenu {
public:
    Button bakeButton  = Button(150, 50, 160, 40, "Bake!", GREY);
    Button learnButton = Button(150, 140, 160, 40, "Learn!", GREY);

    void render() {
        tft.fillScreen(WHITE);

        bakeButton.render();
        learnButton.render();

        rerender = false;
    }

    void update() {
        if (rerender) render();

        draw_stuff();

        bakeButton.update();
        if (bakeButton.is_unpressed())
        {
            state = MAIN_NOT_RUNNING;
            render_main_screen(); // TODO turn this into a class like this.
            rerender = true;            return;
        }

        learnButton.update();
        if (learnButton.is_unpressed())
        {
            state = LEARNING_DASHBOARD;
            rerender = true;
            return;
        }
    }

    void draw_stuff() {
        static int x1 = random(bounds.xmin, bounds.xmax);
        static int y1 = random(bounds.ymin, bounds.ymax);
        static int xx1 = random(bounds.xmin, bounds.xmax);
        static int yy1 = random(bounds.ymin, bounds.ymax);


        const int dt = 50; // 10ms
        static int start_time   = 0;

        if (millis() - start_time > dt)
        {
            int x2 = x1 + random(-seg_length, seg_length);
            int y2 = y1 + random(-seg_length, seg_length);

            // draw line
            if (x2 > bounds.xmin && x2 < bounds.xmax && y2 > bounds.ymin && y2 < bounds.ymax)
            {
                tft.drawLine(x1, y1, x2, y2, GREEN);
            } else {
                if (x2 < bounds.xmin) { x2 = bounds.xmax - (bounds.xmin - x2); }
                else if (x2 > bounds.xmax) { x2 = bounds.xmin + (x2 - bounds.xmax); }
                if (y2 < bounds.ymin) { y2 = bounds.ymax - (bounds.ymin - y2); }
                else if (y2 > bounds.ymax) { y2 = bounds.ymin + (y2 - bounds.ymax); }
            }

            x1 = x2; y1 = y2;

            start_time = millis();
        }
    }
private:
    bool rerender = true;

    struct {
        int ymin = 210;
        int ymax = 460;
        int xmin = 0;
        int xmax = 320;
    } bounds;

    int seg_length = 8;
};

class ParamScreen {
public:
    Button relearnButton = Button(2, 400, 310, 40, "Re-Learn Parameters", GREY);

    void render() {
        tft.fillScreen(WHITE);

        // TODO put learned Params here!
        tft.setCursor(20, 40);
        printRatedParam(LearnedPower);

        tft.setCursor(20, 100);
        printRatedParam(LearnedInertia);

        tft.setCursor(20, 160);
        printRatedParam(LearnedInsulation);

        tft.setCursor(20, 220);
        printRatedParam(OvenScore);

        gotoMenuButton.render();
        relearnButton.render();

        rerender = false;
    }

    void printRatedParam(Param param) {
        tft.setTextColor(BLACK);
        tft.setFont(&FreeSerif18pt7b);

        tft.print(temperature.params.name[param]);
        tft.print(": ");
        ParamRating rating = temperature.params.rate(param);
        switch (rating) {
        case GOOD_RATING:
            tft.setTextColor(0x0d0c); // green
            break;
        case OKAY_RATING:
            tft.setTextColor(0xedc7); // gold
            break;
        case BAD_RATING:
            tft.setTextColor(0xe883); // red
            break;
        }
        tft.print((int)temperature.params.index[param]);
        tft.print(temperature.params.units[param]);
    }

    void update() {
        if (rerender) render();

        relearnButton.update();
        if (relearnButton.is_unpressed())
        {
            rerender = true;
            state = LEARNING_CYCLE;
            return;
        }

        gotoMenuButton.update();
        if (gotoMenuButton.is_unpressed())
        {
            state = MENU;
            rerender = true;
            return;
        }
    }
private:
    bool rerender = true;
};

enum LearningStage {
    NOTICE_STAGE    = 0,
    MAYBE_ABORT     = 1,
    POWER_STAGE     = 2,
};

LearningStage learningStage = NOTICE_STAGE;

// learning cycle time in seconds
const int learning_time = 3600; // 1 hour
Clock learning_clock = Clock(learning_time, 0, 0);
Phases learningPhases(10, 380, learning_profile);

class LearningScreen {
public:
    class Notice {
    public:
        Button continueButton = Button(100, 300, 140, 40, "continue", GREEN);

        void render() {
            tft.fillScreen(WHITE);
            gotoMenuButton.render();

            tft.setCursor(0, 200);
            tft.setTextColor(BLACK);
            tft.print("Learning will take ~1 hour");

            continueButton.render();

            rerender = false;
        }
        void update() {
            if (rerender) render();

            continueButton.update();
            if (continueButton.is_unpressed())
            {
                learningStage = POWER_STAGE;
                rerender = true;
            }

            gotoMenuButton.update();
            if (gotoMenuButton.is_unpressed())
            {
                state = MENU;
                rerender = true;
            }
        }
    private:
        bool rerender = true;
    };
    Notice notice;

    class PowerStage {
    public:
        Plot plot = Plot(0, 200, tft.width(), 250, &learningPhases, true);
        Temperature temperature = Temperature(&thermocouple, 0, 160);

        void render() {
            tft.fillScreen(WHITE);

            gotoMenuButton.render();

            // print remaining global time
            learning_clock.render();

            tft.setCursor(0, 80);
            tft.setTextColor(BLACK);
            tft.print("Learning Power");

            tft.setCursor(0, 120);
            tft.setTextColor(BLACK);
            tft.setFont(&FreeSerif9pt7b);
            tft.print("maintaining 120C");

            // print current duty cycle (color coded for rating);
            updateRelearnedPower();
            drawPowerUsage();

            // plot
            plot.render();
            plot.renderLineAt(120);

            rerender = false;
        }

        void update() {
            if (rerender) render();

            learning_clock.update();
            // plot temperature points
            if (temperature.read()) {
                temperature.render();
                plot.render_temperature_measurement(temperature, learning_clock);
            }

            updateRelearnedPower();

            drawPowerUsage();

            // control heating elements
            temperature.setDutyCycles(relearnedPower, relearnedPower, relearnedPower);
            temperature.controlHeatingElements();

            gotoMenuButton.update();
            if (gotoMenuButton.is_unpressed())
            {
                // really abort?

                rerender = true;
            }

            // TODO select which phase we will be in
        }

        void updateRelearnedPower() {
            static int lastTimeOverTemp = 0;
            static bool isHeatingUp = true;
            static int powerIntegral = 0;
            if (reset)
            {
                // start from what exists, but constrain (12 = amazing, 35 = terrible)
                relearnedPower = constrain(temperature.params.learnedPower, 0, 100);
                reset = false;

                // reset static vars
                lastTimeOverTemp = 0;
                isHeatingUp = true;
                powerIntegral = 0;
            }

            // check current temp
            temperature.read();

            if (rampingInitially)
            {
                // TODO if we are 30 degrees away from soak temp, set duty cycle to 30
                if ((LEARNING_SOAK_TEMP-temperature.current) < 30) {
                    relearnedPower = 30;
                } else if ((LEARNING_SOAK_TEMP-temperature.current) < 15) {
                    rampingInitially = false;
                    relearnedPower = 15;
                } else {
                    relearnedPower = 100;
                }

                return;
            }

            // at this point we are not in the initial ramp up.
            // we are trying to maintain a constant temp.

            if (temperature.current > LEARNING_SOAK_TEMP) {
                if (isHeatingUp) {
                    isHeatingUp = false;

                    // only decrease the duty cycle every 30 seconds
                    if (millis()-lastTimeOverTemp > (30*1000)) {
                        lastTimeOverTemp = millis();
                        if (relearnedPower > 0)
                            relearnedPower--;
                    }

                    // reset power integral
                    powerIntegral = 0;
                }
            }
            else {
                isHeatingUp = true;

                if (LEARNING_SOAK_TEMP - temperature.current > 1.0)
                    powerIntegral++;
                if (LEARNING_SOAK_TEMP - temperature.current > 5.0)
                    powerIntegral++;

                if (powerIntegral > 30) {
                    powerIntegral = 0;

                    if (relearnedPower < 100) {
                        relearnedPower++;
                    }
                }
            }

        }

        void drawPowerUsage() {
            // get current duty cycle for all heating elements
            // rate the current duty cycle
            ParamRating rating = temperature.params.rate(LearnedPower, relearnedPower);

            switch (rating) {
            case GOOD_RATING:
                tft.setTextColor(GOOD_RATING_COLOR);
                break;
            case OKAY_RATING:
                tft.setTextColor(OKAY_RATING_COLOR);
                break;
            case BAD_RATING:
                tft.setTextColor(BAD_RATING_COLOR);
                break;
            }

            tft.setCursor(220, 190);
            tft.setFont(&FreeSerif18pt7b);
            tft.print((int)relearnedPower);
            tft.print("%");
        }

    private:
        bool reset = true;
        bool rerender = true;
        float relearnedPower;
        float LEARNING_SOAK_TEMP = 120;
        bool rampingInitially = true;
    };
    PowerStage powerStage;

    void render() {
        tft.fillScreen(WHITE);
        gotoMenuButton.render();

        switch (learningStage) {
        case NOTICE_STAGE:
            notice.render();
            break;
        case POWER_STAGE:
            powerStage.render();
            break;
        }

        rerender = false;
    }

    void update() {
        if (rerender) render();

        switch (learningStage) {
        case NOTICE_STAGE:
            notice.update();
            break;
        case POWER_STAGE:
            powerStage.update();
            break;
        }
    }

private:
    bool rerender = true;
};

//:::: Screens
MainMenu menu;
ParamScreen learningDashboard;
LearningScreen learningPhaseScreen;

void setup() {
  // allow some time before initializing the display
  // otherwise it seems to invert the image on powerup.
  delay(50);

  // DEBUG
  // NOTE: MAKE SURE THIS ISN'T ON WHEN USING EXTERNAL POWER SUPPLY!!!
  // program WILL hang!
  Serial.begin(9600);

  // get a random voltage to randomly seed the RNG
  // pin 6 is unconnected.
  randomSeed(analogRead(6));

  // I don't know why this needs to be pin 10.
  // if it is gone, it doesn't work. or if it is another pin
  // it doens't work.
  // pinMode(10, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(STMPE_CS, OUTPUT);
  pinMode(THERM_CS, OUTPUT);
  pinMode(HEATING_ELEMENT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(STMPE_CS, HIGH);
  digitalWrite(THERM_CS, HIGH);
  digitalWrite(HEATING_ELEMENT_CS, HIGH);

  delay(50);

  // begin capacitive touch sensor and lcd screen
  tft.begin();
  while (!touch.begin())
  {
      delay(50);
      tft.fillScreen(RED); // maybe do something better....?
  };


  temperature.begin();

  tft.setRotation(0);  // make screen hamburger style


  // configure gui stuff
  profile_button.txt_margin.x = 10;
  profile_button.txt_margin.y = 30;


  // DEBUG
  // temperature.params.write(LearningComplete, 255);
  // temperature.params.write(LearnedPower, 88);
  // temperature.params.write(LearnedInertia, 99);
  // temperature.params.write(LearnedInsulation, 100);
  // temperature.params.write(OvenScore, 90);

  // DEBUGGING: for some reason, we can't read from this.
  temperature.params.read(LearningComplete);
  temperature.params.read(LearnedPower);
  temperature.params.read(LearnedInertia);
  temperature.params.read(LearnedInsulation);
  temperature.params.read(OvenScore);

  // DEBUGGING TAKE OUT
  // temperature.controlHeatingElements();
}

void loop() {
    switch (state) {
    case STARTUP:
        render_loading_screen();
        // state = MAIN_NOT_RUNNING;
        // render_main_screen();
        menu.render();
        state = MENU;
        break;
    case MENU:
        menu.update();
        break;
    case LEARNING_DASHBOARD:
        learningDashboard.update();
        break;
    case LEARNING_CYCLE:
        learningPhaseScreen.update();
        break;
    case MAIN_NOT_RUNNING:
        if (temperature.read()) {
            temperature.render();
            plot.render_temperature_measurement(temperature, clock);
        }
        gotoMenuButton.update();
        if (gotoMenuButton.is_pressed()) return;
        if (gotoMenuButton.is_unpressed())
        {
            menu.render();
            state = MENU;
            return;
        }
        start_button.update();
        if (start_button.is_pressed()) return;
        if (start_button.is_unpressed()) {
            plot.render();
            start_button.txt = "ABORT";
            state = MAIN_RUNNING;
            clock.start();
            start_button.render();
            return;
        }
        break;
    case MAIN_RUNNING:
        clock.update();
        start_button.update();
        temperature.update(); // TODO this is where PID code should go.
        phases.update(clock.time_elapsed);
        if (temperature.read()) {
            temperature.render();
            plot.render_temperature_measurement(temperature, clock);
        }
        if (start_button.is_unpressed()) {
            plot.render();
            start_button.txt = "START";
            state = MAIN_NOT_RUNNING;
            clock.reset();
            clock.render();
            start_button.render();
            phases.render(0);
            phases.current_phase = STANDBY;
            temperature.off();
        }
        if (clock.time_remaining == 0) {
            start_button.txt = "RESTART";
            start_button.render();
            state = MAIN_NOT_RUNNING;
            temperature.off();
        }
        break;
  }

}

void render_main_screen() {
    tft.fillScreen(WHITE);

    // int selected_profile = 0;
    main_screen_current_profile(selected_profile);
    gotoMenuButton.render();
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
