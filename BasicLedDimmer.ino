#include <EEPROM.h> // Storage of pot min/max values
#include "cie1931.h" // cie[256] = { 0...1023 } Look up table for human-friendly luminosity
/*
 * MOSFET Reminder
 *  Pins: (left to right): G,D,S (Gate, Drain, Source)
 *  Heatsink is attached to Drain.
 *  
 *  Gate goes to Analog out (PWM) on MCU.
 *  Drain goes to Resistor -- LED -- Vcc.
 *  Source goes to Ground.
 *  
 * Source: https://learn.adafruit.com/rgb-led-strips/usage
 *  
 */

//#define TARGET_TEENSY // Uncomment to TARGET_TEENSY for Teensy 4.0 else Arduino
#define DEBUG 1
#define TIMESTEP 100 // ms 

#ifdef TARGET_TEENSY

#define DEVICE_NAME "LEDdimmer 1.0 (teensy40)"
//#include <InternalTemperature.h> // TARGET_TEENSY 4 library:
// https://github.com/LAtimes2/InternalTemperature
// PWM Gating spec
#define PWM_FREQ 3000 // Hz? kHz? PWM frequency (default 4.482kHz on TARGET_TEENSY 4.0)
#define PWM_BITS 8 // bits PWM bit-depth (default 8-bit on TARGET_TEENSY 4.0)
#define PWM_MAX 255 // == 2**PWM_BITS-1
#define POT_MAX 535
// Gate PINs wired to the gates of three MOSFETs (R,G,B)
#define GATE_R LED_BUILTIN // pin 13
#define GATE_G 14
#define GATE_B 15
// Potmeters on Analog ins:
#define POT_R 16
#define POT_G 17
#define POT_B 18

#else // not TARGET_TEENSY

#define DEVICE_NAME "LEDdimmer 1.0 (arduino)"
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255
#define POT_MAX 688
// Gate PINs wired to the gates of three MOSFETs (R,G,B)
#define GATE_R LED_BUILTIN // pin 13 attached to on board LED
#define GATE_G 11
#define GATE_B 9
// Potmeters on Analog ins:
#define POT_R A0 // GATE_R 
#define POT_G A1 // GATE_G 
#define POT_B A2 // GATE_B 

#endif // not TARGET_TEENSY


void setup() {
  Serial.begin(115200);
#ifdef TARGET_TEENSY  
  analogWriteFrequency(GATE_R, PWM_FREQ); // Available in TARGET_TEENSY 
  analogWriteFrequency(GATE_G, PWM_FREQ);
  analogWriteFrequency(GATE_B, PWM_FREQ);
  analogWriteResolution(PWM_BITS); // integer duty-cycle range bit-depth // Available in TARGET_TEENSY
#endif
  pinMode(GATE_R, OUTPUT);  // Configure the pin as output
  pinMode(GATE_G, OUTPUT);
  pinMode(GATE_B, OUTPUT);

  analogWrite(GATE_R, (int) 1 * PWM_MAX / 100); // 1% duty cycle;
  analogWrite(GATE_G, (int) 1 * PWM_MAX / 100);
  analogWrite(GATE_B, (int) 1 * PWM_MAX / 100);

  delay(500); // Grace time
}

int romaddr = 0; // EEPROM memory start address
bool on_r = 1; // Enable LED?
bool on_g = 1;
bool on_b = 1;
int pwm_r = 0; // Value between 0-PWM_MAX
int pwm_g = 0;
int pwm_b = 0;
int remote_r = -1; // Value between 0 and 100 (%), -1 for disable
int remote_g = -1;
int remote_b = -1;
int pot_r = 0; // Value between pot_r_min and pot_r_max (depends...)
int pot_g = 0;
int pot_b = 0;
bool auto_pot_limits = 0; // Update limits automatically upon read?
int pot_r_min = 0;//999; // Initial minimum is big
int pot_g_min = 0;//999;
int pot_b_min = 0;//999;
int pot_r_max = 535;//0; // Initial maximum is small
int pot_g_max = 535;//0;
int pot_b_max = 535;//0;
char msgbuf[256]; // line buffer to print messages


void read_pot() {
  pot_r = analogRead(POT_R); // analog read of potentiometer
  pot_g = analogRead(POT_G);
  pot_b = analogRead(POT_B);
  if (auto_pot_limits) {
    if (pot_r > pot_r_max) pot_r_max = pot_r; // Found new maximum
    if (pot_g > pot_g_max) pot_g_max = pot_g;
    if (pot_b > pot_b_max) pot_b_max = pot_b;
    if (pot_r < pot_r_min) pot_r_min = pot_r; // Found new minimum;
    if (pot_g < pot_g_min) pot_g_min = pot_g;
    if (pot_b < pot_b_min) pot_b_min = pot_b;
  }
  
  if (DEBUG) {
    sprintf(msgbuf, "%-16s: %04d; %04d; %04d;\n",
      "read_pot", pot_r, pot_g, pot_b  );
    Serial.write(msgbuf);    
  }
}
void read_pot_rom() {
  Serial.println("Reading ROM");
  int tmp_r_min = EEPROM.read(romaddr+0); // Read ROM address
  int tmp_g_min = EEPROM.read(romaddr+1);
  int tmp_b_min = EEPROM.read(romaddr+2);
  int tmp_r_max = EEPROM.read(romaddr+3);
  int tmp_g_max = EEPROM.read(romaddr+4);
  int tmp_b_max = EEPROM.read(romaddr+5);
  // Only sane values are accepted (0..min..max..1024)
  if (tmp_r_min>=0 && tmp_r_min < tmp_r_max && tmp_r_max < 1024) {
    pot_r_min = tmp_r_min ; pot_r_max = tmp_r_max;
  }
  if (tmp_g_min>=0 && tmp_g_min < tmp_g_max && tmp_g_max < 1024) {
    pot_g_min = tmp_g_min ; pot_g_max = tmp_g_max;
  }
  if (tmp_b_min>=0 && tmp_b_min < tmp_b_max && tmp_b_max < 1024) {
    pot_b_min = tmp_b_min ; pot_b_max = tmp_b_max;
  }
  if (DEBUG) {
    sprintf(msgbuf, "read_pot_rom: r_min,r_max; r_min,r_max; r_min,r_max:\n");Serial.write(msgbuf);
    sprintf(msgbuf, "read_pot_rom: %04d,%04d; %04d,%04d; %04d,%04d.\n",
      pot_r_min, pot_r_max, pot_g_min, pot_g_max, pot_b_min, pot_b_max );
    Serial.write(msgbuf);    
  }
}
void write_pot_rom() {
  Serial.println("Writing ROM");
  EEPROM.update(romaddr+0,pot_r_min); // Store minimum pot value
  EEPROM.update(romaddr+1,pot_g_min);
  EEPROM.update(romaddr+2,pot_b_min);
  EEPROM.update(romaddr+3,pot_r_max); // Store maximum pot value
  EEPROM.update(romaddr+4,pot_g_max);
  EEPROM.update(romaddr+5,pot_b_max);
  if (DEBUG) {
    sprintf(msgbuf, "write_pot_rom: r_min,r_max; r_min,r_max; r_min,r_max:\n");Serial.write(msgbuf);
    sprintf(msgbuf, "write_pot_rom: %04d,%04d; %04d,%04d; %04d,%04d.\n",
      pot_r_min, pot_r_max, pot_g_min, pot_g_max, pot_b_min, pot_b_max );
    Serial.write(msgbuf);    
  }
}
void calc_pwm(){
  // normalize pot range, then
  // rescale to CIE_SIZE (table length), then
  // look up CIE 1931 luminosity, then
  // rescale CIE table output to PWM_MAX range
  pwm_r = (PWM_MAX/CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * (long) abs(pot_r-pot_r_min)) )/abs(pot_r_max-pot_r_min) ];
  pwm_g = (PWM_MAX/CIE_RANGE) * cie[ ( on_g * (CIE_SIZE * (long) abs(pot_g-pot_g_min)) )/abs(pot_g_max-pot_g_min) ];
  pwm_b = (PWM_MAX/CIE_RANGE) * cie[ ( on_b * (CIE_SIZE * (long) abs(pot_b-pot_b_min)) )/abs(pot_b_max-pot_b_min) ];
//  if (DEBUG) {
//    sprintf(msgbuf, "calc_pwm: r,g,b = %d; %d; %d;\n", pwm_r, pwm_g, pwm_b);
//    Serial.write(msgbuf);  
//  }
}
void write_pwm(){
  analogWrite(GATE_R, pwm_r); // apply pwm dut/y cycle
  analogWrite(GATE_G, pwm_g);
  analogWrite(GATE_B, pwm_b);
  if (DEBUG) {
    sprintf(msgbuf, "%-16s: %04d; %04d; %04d;\n",
      "write_pwm", pwm_r, pwm_g, pwm_b );
    Serial.write(msgbuf);  
    
  }
}
void listen_command() {
  // Listen for some 
  if (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    char *cmd = (char *)str.c_str();
    parse_command(cmd);
  }
}
void parse_command(char *cmd) {
  char *tok;
  tok = strtok(cmd, " \n");
  
  if (strcmp(tok, "?") == 0) {
    sprintf(msgbuf, "%s\n", DEVICE_NAME);
    Serial.print(msgbuf);
  } else if (strcmp(tok, "update") == 0) {
    read_pot();
    calc_pwm();
    write_pwm();
    Serial.println("updated"); // Always reply something
  } else if (strcmp(tok, "on") == 0) {
    on_r = true;
    on_g = on_r;
    on_b = on_r;
    Serial.println("on"); // Always reply something
  } else if (strcmp(tok, "off") == 0) {
    on_r = false;
    on_g = on_r;
    on_b = on_r;
    Serial.println("off"); // Always reply something
  } else if (strcmp(tok, "intensity_percent") == 0) {
    // intensity_percent <0-100>
    tok = strtok(NULL, " \n");
    long percent = strtol(tok, NULL, 10);
    if (percent>=0 && percent <=100) {
      // Accepted
      
    }
  } else if (strcmp(tok, "calc?") == 0) {
    Serial.println("pwm_r = (int) (PWM_MAX/CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * abs(pot_r-pot_r_min)))/abs(pot_r_max-pot_r_min) ];");
    sprintf(msgbuf, "%04d = (int) (%04d/%04d) * cie[ ( %04d * (%04d * abs(%04d-%04d)))/abs(%04d-%04d) ];\n",
                     pwm_r, PWM_MAX, CIE_RANGE, on_r, CIE_SIZE, pot_r, pot_r_min, pot_r_max, pot_r_min);
    Serial.write(msgbuf); 
    sprintf(msgbuf, "%04d = (int) (%04d) * cie[ ( %04d * (%04d * abs(%04d)))/abs(%04d) ];\n",
                     pwm_r, PWM_MAX/CIE_RANGE, on_r, CIE_SIZE, pot_r-pot_r_min, pot_r_max-pot_r_min);
    Serial.write(msgbuf); 
    sprintf(msgbuf, "%04d = (int) (%04d) * cie[ ( %04d * (%04l))/abs(%04d) ];\n",
                     pwm_r, PWM_MAX/CIE_RANGE, on_r, CIE_SIZE*  (long)  abs(pot_r-pot_r_min), abs(pot_r_max-pot_r_min));
    Serial.write(msgbuf); 
    sprintf(msgbuf, "%04d = (int) (%04d) * cie[ ( %04l )/abs(%04d) ];\n",
                     pwm_r, PWM_MAX/CIE_RANGE, on_r*CIE_SIZE* (long) abs(pot_r-pot_r_min), abs(pot_r_max-pot_r_min));
    Serial.write(msgbuf); 
    sprintf(msgbuf, "%04d = (int) (%04d) * cie[ ( %04l ) ];\n",
                     pwm_r, PWM_MAX/CIE_RANGE, (on_r*CIE_SIZE* (long) abs(pot_r-pot_r_min))/abs(pot_r_max-pot_r_min));
    Serial.write(msgbuf); 
//    sprintf(msgbuf, "%04d = (int) (%04d) * ( %04l ) ;\n",
//                     pwm_r, PWM_MAX/CIE_RANGE, cie[ (on_r*CIE_SIZE*abs(pot_r-pot_r_min))/abs(pot_r_max-pot_r_min) ]);
//    Serial.write(msgbuf); 
//    sprintf(msgbuf, "%04d = (int) ( %04l ) ;\n",
//                     pwm_r, (PWM_MAX/CIE_RANGE) * cie[ (on_r*CIE_SIZE*abs(pot_r-pot_r_min))/abs(pot_r_max-pot_r_min) ]);
//    Serial.write(msgbuf); 
  } else if (strcmp(tok, "pot_range") == 0) {
    // calibration potmeter@min ..@max
    tok = strtok(NULL, " \n");
    long tpot_min = strtol(tok, NULL, 10);
    tok = strtok(NULL, " \n");
    long tpot_max = strtol(tok, NULL, 10);
  } else if (strcmp(tok, "pot_range?") == 0) {
    sprintf(msgbuf, "pot_range: r_min,r_max; r_min,r_max; r_min,r_max:\n");Serial.write(msgbuf);
    sprintf(msgbuf, "pot_range: %04d,%04d; %04d,%04d; %04d,%04d.\n",
      pot_r_min, pot_r_max, pot_g_min, pot_g_max, pot_b_min, pot_b_max );
    Serial.write(msgbuf);    
  } else if (strlen(tok) > 2) {
    sprintf(msgbuf, "Unknown command: '%s'\n", tok);
    Serial.write(msgbuf);
  }
}

int bootcount = 0;
void loop() {
  if (bootcount == 5000/TIMESTEP) read_pot_rom(); // read ROM at 5s mark.
  if (bootcount <= 10000/TIMESTEP) bootcount++; // bootup time lasts up to 10 seconds
  if (bootcount == 10000/TIMESTEP) {
    sprintf(msgbuf, "Finished bootup (%s)\n", DEVICE_NAME);
    Serial.print(msgbuf);
  } 
  // Every time, do:
    read_pot();
    calc_pwm();
    write_pwm();
  listen_command();
  
  delay(TIMESTEP);
}
