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

/*
 * Remote (serial port) vs Local (potmeter) control behavior. Let's have 
 * a simple rule: if remote is adjusted, do it and set the potmeter's guard value; 
 * if potmeter is adjusted, dont do it, unless it is threshold away the guard value;
 * 
 * 
 */

#define DEBUG 1
#define TIMESTEP 100 // ms

#define DEVICE_NAME "LEDdimmer 1.0 (arduino, itsybitsy 32u4 5V)"
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255
#define GATE_R LED_BUILTIN // MOSFET GATE PWM out; pin 13 attached to on board LED
#define POT_R A0 // POTMETER Analog in

void setup() {
  Serial.begin(115200);
  pinMode(GATE_R, OUTPUT);  // Configure the pin as output
  analogWrite(GATE_R, (int) 1 * PWM_MAX / 100); // 1% duty cycle;
  delay(500); // Grace time
}

bool on_r = 1; // Enable LED?
int romaddr = 0; // EEPROM memory start address
int pwm_r = 0; // Value between 0 and PWM_MAX
bool auto_pot_limits = 0; // Update limits automatically upon read?
int pot_r = 0; // Value between pot_r_min and pot_r_max (depends...)
int pot_r_min = 0;   // Initial minimum
int pot_r_max = 535; // Initial maximum
int pot_guard_t = 10; // threshold value
int pot_guard_r = -10; // -threshold: means no guard set.
int rem_r = -1; // remote control value between 0 and 100 (%), -1 for disable
char msgbuf[256]; // line buffer to print messages

void read_pot() {
  pot_r = analogRead(POT_R); // update last value of potentiometer
  if (auto_pot_limits) {
    if (pot_r > pot_r_max) pot_r_max = pot_r; // Found new maximum
    if (pot_r < pot_r_min) pot_r_min = pot_r; // Found new minimum;
  }
  if (DEBUG) {
    sprintf(msgbuf, "%-16s: %04d;\n",
      "read_pot", pot_r  );
    Serial.write(msgbuf);    
  }
}
void read_pot_rom() {
  Serial.println("Reading ROM");
  int tmp_r_min = EEPROM.read(romaddr+0); // Read ROM address
  int tmp_r_max = EEPROM.read(romaddr+1);
  // Only sane values are accepted (0..min..max..1024)
  if (tmp_r_min>=0 && tmp_r_min < tmp_r_max && tmp_r_max < 1024) {
    pot_r_min = tmp_r_min ; pot_r_max = tmp_r_max;
  }
  if (DEBUG) {
    sprintf(msgbuf, "read_pot_rom: %04d,%04d;\n", pot_r_min, pot_r_max);
    Serial.write(msgbuf);    
  }
}
void write_pot_rom() {
  Serial.println("Writing ROM");
  EEPROM.update(romaddr+0,pot_r_min); // Store minimum pot value
  EEPROM.update(romaddr+1,pot_r_max); // Store maximum pot value
  if (DEBUG) {
    sprintf(msgbuf, "write_pot_rom: %04d,%04d;\n", pot_r_min, pot_r_max );
    Serial.write(msgbuf);    
  }
}
void set_pwm_by_pot(){
  // a) normalize pot range, then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  pwm_r = (PWM_MAX/CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * (long) abs(pot_r-pot_r_min)) )/abs(pot_r_max-pot_r_min) ];
//  if (DEBUG) {
//    sprintf(msgbuf, "set_pwm_by_pot: r = %d;\n", pwm_r);
//    Serial.write(msgbuf);  
//  }
}
void set_pwm_by_rem(){
  // a) normalize remote range (0..100), then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  pwm_r = (PWM_MAX/CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * (long) rem_r ) )/100 ];
//  if (DEBUG) {
//    sprintf(msgbuf, "set_pwm_by_rem: r = %d;\n", pwm_r);
//    Serial.write(msgbuf);  
//  }
}
void write_pwm(){
  analogWrite(GATE_R, pwm_r); // apply pwm duty cycle
  if (DEBUG) {
    sprintf(msgbuf, "%-16s: %04d;\n", "write_pwm", pwm_r );
    Serial.write(msgbuf);  
  }
}
void listen_command() {
  // Fetch command if avaiable
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
  } else if (strcmp(tok, "on") == 0) {
    on_r = true;
    Serial.println("on"); // Always reply something
  } else if (strcmp(tok, "off") == 0) {
    on_r = false;
    Serial.println("off"); // Always reply something
  } else if (strcmp(tok, "I") == 0 || strcmp(tok, "intensity_percent") == 0) {
    // remote set intensity (0...100)
    tok = strtok(NULL, " \n");
    long intensity = strtol(tok, NULL, 10);
    if (intensity<0 || intensity>100) {
      sprintf(msgbuf, "intensity invalid (out of range 0-100)\n");
      Serial.write(msgbuf);
    } else {
      rem_r = intensity; // update last known remote value
      pot_guard_r = pot_r; // update guard value of pot
      sprintf(msgbuf, "I %d\n", rem_r);
      Serial.write(msgbuf);
    }
  } else if (strcmp(tok, "I?") == 0 || strcmp(tok, "intensity_percent?") == 0) {
    // print intensity in percentage, use actual pwm setting
    sprintf(msgbuf, "I %03d;\n", ((long) pwm_r * 100)/PWM_MAX );
    Serial.write(msgbuf);    
  } else if (strcmp(tok, "pot_range") == 0) {
    // set calibration potmeter_min ... potmeter_max
    tok = strtok(NULL, " \n");
    long tpot_min = strtol(tok, NULL, 10);
    tok = strtok(NULL, " \n");
    long tpot_max = strtol(tok, NULL, 10);
  } else if (strcmp(tok, "pot_range?") == 0) {
    sprintf(msgbuf, "pot_range: r_min,r_max;\n");
    Serial.write(msgbuf);
    sprintf(msgbuf, "pot_range: %04d,%04d;\n", pot_r_min, pot_r_max );
    Serial.write(msgbuf);    
  } else if (strlen(tok) > 2) {
    sprintf(msgbuf, "Unknown command: '%s'\n", tok);
    Serial.write(msgbuf);
  }
}

int bootcount = 0;
void loop() {
  if (bootcount == 5000/TIMESTEP) read_pot_rom(); // read ROM at 5s mark.
  if (bootcount == 10000/TIMESTEP) {
    sprintf(msgbuf, "Finished bootup (%s)\n", DEVICE_NAME);
    Serial.print(msgbuf);
  } 
  if (bootcount < 10000/TIMESTEP) bootcount++; // bootup time lasts up to 10 second
  
  // Every time, behave:

  listen_command();
  
  read_pot(); // Read potmeter value, 
  
  if (pot_guard_r < 0) {
    // no guard value : use pot.
    set_pwm_by_pot();
  } else if ( 
       ( (pot_r-pot_guard_r) > 0 && (pot_r-rem_r)>pot_guard_t ) || 
       ( (pot_r-pot_guard_r) < 0 && (pot_r-rem_r)<pot_guard_t )
    ) {
    // Pot was increased from guard value, and it is bigger than remote? => do it
    // Pot was decreased from guard value, and it is smaller than remote? => do it
    pot_guard_r = -1; // Unset guard
    set_pwm_by_pot(); // use pot if no guard set.
  } else {
    // (keep) using remote if guard set.
    set_pwm_by_rem(); 
  }
  
  write_pwm(); // update pwm
  
  delay(TIMESTEP);
}
