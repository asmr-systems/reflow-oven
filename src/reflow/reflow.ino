#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

// These are 'flexible' lines that can be changed
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST -1 // RST can be set to -1 if you tie it to Arduino's reset

// Color Palette (RGB565) 
// Use this https://chrishewett.com/blog/true-rgb565-colour-picker/
// as a color picker.
#define PINK 0xfb14
#define GREEN 0x27a4
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


// Use hardware SPI (on Nano, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);


void setup() {
  // allow some time before initializing the display
  // otherwise it seems to invert the image on powerup.
  delay(50);
  
  tft.begin();

  tft.setRotation(0);  // make screen hamburger style

  loading_screen();
  
  tft.fillScreen(PINK);
  main_screen();
}

void loop() {
  // put your main code here, to run repeatedly:

}

void loading_screen() {
  tft.fillScreen(WHITE);

  int rect_x_origin = tft.width()/2 - 60;
  int rect_y_origin = tft.height()/2 - 30;
  tft.drawRect(rect_x_origin, rect_y_origin, 30, 30, PINK);

  int text_x_origin = tft.width()/2 - 25;
  int text_y_origin = tft.height()/2 - 10;
  tft.setCursor(text_x_origin, text_y_origin);
  tft.setFont(&FreeSerif18pt7b);
  tft.setTextColor(PINK);
  tft.println("ASMR");

  delay(100);

  
  delay(10000);
}

void main_screen() {
  int selected_profile = 0;
  main_screen_current_profile(selected_profile);
  profile_plot(0, 40, tft.width(), tft.height()*0.70, selected_profile);
  return;
}

void main_screen_current_profile(int selected_profile) {
  tft.setCursor(0, 20);
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
