#include <EEPROM.h> // Storage of pot min/max values
#include "cie1931.h" // cie[256] = { 0...255 } Look up table for human-friendly luminosity scale
/*
   MOSFET Mini How To
      Pins: (left to right): G,D,S (Gate, Drain, Source)
      Heatsink is attached to Drain.

      Gate goes to Analog out (PWM) on MCU.
      Drain goes to Resistor -- LED -- Vcc.
      Source goes to Ground.

   Source: https://learn.adafruit.com/rgb-led-strips/usage
*/

/*
   Remote (serial port) vs Local (potmeter) control behavior. Let's have
   a simple rule: if remote is adjusted, do it and set the potmeter's guard value;
   if potmeter is adjusted, dont do it, unless it exceeds the threshold value
*/

#define DEBUG 0
#define TIMESTEP 10 // ms

#define DEVICE_NAME "LEDdimmer 1.0 (arduino, itsybitsy 32u4 5V)"
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255
#define GATE_R LED_BUILTIN // MOSFET GATE PWM out; pin 13 attached to on board LED
#define POT_R A0 // POTMETER Analog in

void setup() {
  Serial.begin(115200);
  pinMode(GATE_R, OUTPUT);  // Configure the pin as output
  analogWrite(GATE_R, (int) 1 * PWM_MAX / 100); // 1% duty cycle;
}

bool on_r = 1; // Enable LED?
int romaddr = 0; // EEPROM memory start address
int pwm_r = 0; // Value between 0 and PWM_MAX
int pot_r = 0; // Value between pot_r_min and pot_r_max (depends...)
int pot_r_min = 0;   // Initial minimum :
int pot_r_max = 512; // Initial maximum
bool pot_range_auto = 1; // Update limits automatically upon read?
int pot_guard_t = 2; // threshold value
int pot_guard_r = -1; // negative: means no guard set.
int rem_r = -1; // remote control value between 0 and 100 (%), -1 for disable
char msgbuf[256]; // line buffer to print messages

void read_pot() {
  long tpot = 0;
  for (int i = 0; i < 5; i++) {
    tpot += analogRead(POT_R);
    delay(2);
  }
  pot_r = tpot / 5 ; // average pot meter value over 10ms
  if (pot_range_auto) {
    if (pot_r > pot_r_max) pot_r_max = pot_r; // Found new maximum
    if (pot_r < pot_r_min) pot_r_min = pot_r; // Found new minimum;
  }
  //  if (DEBUG) {
  //    sprintf(msgbuf, "%-16s: %04d;\n",
  //      "read_pot", pot_r  );
  //    Serial.write(msgbuf);
  //  }
}
void read_pot_rom() {
  Serial.println("Reading ROM");
  int tmp_r_min = EEPROM.read(romaddr + 0); // Read ROM address
  int tmp_r_max = EEPROM.read(romaddr + 1);
  // Only sane values are accepted (0..min..max..1024)
  if (tmp_r_min >= 0 && tmp_r_min < tmp_r_max && tmp_r_max < 1024) {
    pot_r_min = tmp_r_min ; pot_r_max = tmp_r_max;
  }
  if (DEBUG) {
    sprintf(msgbuf, "read_pot_rom: %04d,%04d;\n", pot_r_min, pot_r_max);
    Serial.write(msgbuf);
  }
}
void write_pot_rom() {
  Serial.println("Writing ROM");
  EEPROM.update(romaddr + 0, pot_r_min); // Store minimum pot value
  EEPROM.update(romaddr + 1, pot_r_max); // Store maximum pot value
  if (DEBUG) {
    sprintf(msgbuf, "write_pot_rom: %04d,%04d;\n", pot_r_min, pot_r_max );
    Serial.write(msgbuf);
  }
}
void set_pwm_by_pot() {
  // a) normalize pot range, then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  pwm_r = (PWM_MAX / CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * (long) abs(pot_r - pot_r_min)) ) / abs(pot_r_max - pot_r_min) ];
  // Update remote value (percent)
  rem_r = ( on_r * (100 * (long) abs(pot_r - pot_r_min)) ) / abs(pot_r_max - pot_r_min);
  //  if (DEBUG) {
  //    sprintf(msgbuf, "set_pwm_by_pot: r = %d;\n", pwm_r);
  //    Serial.write(msgbuf);
  //  }
}
void set_pwm_by_rem() {
  // a) normalize remote range (0..100), then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  pwm_r = (PWM_MAX / CIE_RANGE) * cie[ ( on_r * (CIE_SIZE * (long) rem_r ) ) / 100 ];
  //  if (DEBUG) {
  //    sprintf(msgbuf, "set_pwm_by_rem: r = %d;\n", pwm_r);
  //    Serial.write(msgbuf);
  //  }
}
void write_pwm() {
  analogWrite(GATE_R, pwm_r); // apply pwm duty cycle
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

  if (strcmp(tok, "?") == 0 || strcmp(tok, "name?") == 0) {
    sprintf(msgbuf, "%s\n", DEVICE_NAME);
    Serial.print(msgbuf);
  } else if (strcmp(tok, "on?") == 0 || strcmp(tok, "off?") == 0) {
    Serial.println(on_r ? "on" : "off");
  } else if (strcmp(tok, "on") == 0 || strcmp(tok, "off") == 0) {
    on_r = (strcmp(tok, "on") == 0);
    Serial.println(on_r ? "on" : "off");  // Always reply something
  } else if (strcmp(tok, "I") == 0) {
    // remote set intensity (0...100)
    tok = strtok(NULL, " \n");
    long val = strtol(tok, NULL, 10);
    if (val < 0 || val > 100) {
      Serial.println("intensity invalid (out of range 0-100)");  // Always reply something
    } else {
      rem_r = val; // update last known remote value
      pot_guard_r = pot_r; // enable remote control by setting last known pot value
      sprintf(msgbuf, "I %d\n", rem_r);
      Serial.write(msgbuf);
    }
  } else if (strcmp(tok, "I?") == 0) {
    // print intensity in percentage, use remote percentage setting
    sprintf(msgbuf, "I %03d\n", rem_r );
    Serial.write(msgbuf);
  } else if (strcmp(tok, "pot_range") == 0) {
    // set calibration potmeter_min ... potmeter_max
    tok = strtok(NULL, " \n");
    long tpot_min = strtol(tok, NULL, 10);
    tok = strtok(NULL, " \n");
    long tpot_max = strtol(tok, NULL, 10);
    if (0 <= tpot_min && tpot_min < tpot_max && tpot_max <= 1024) {
      pot_r_min = tpot_min;
      pot_r_max = tpot_max;
      sprintf(msgbuf, "pot_range: %03d,%03d;\n", pot_r_min, pot_r_max );
      Serial.write(msgbuf);
    } else {
      Serial.println("pot_range invalid (out of range 0-1024)");  // Always reply something
    }
  } else if (strcmp(tok, "pot_range?") == 0) {
    sprintf(msgbuf, "pot_range: %03d,%03d;\n", pot_r_min, pot_r_max );
    Serial.write(msgbuf);
  } else if (strcmp(tok, "read_pot_rom") == 0) {
    read_pot_rom();
  } else if (strcmp(tok, "write_pot_rom") == 0) {
    write_pot_rom();
  } else if (strcmp(tok, "pot_range_auto") == 0) {
    tok = strtok(NULL, " \n");
    long val = strtol(tok, NULL, 10);
    if (val == 1 || val == 0) {
      pot_range_auto = val;
    }
    sprintf(msgbuf, "pot_range_auto: %d;\n", pot_range_auto );
    Serial.write(msgbuf);
  } else if (strcmp(tok, "pot_range_auto?") == 0) {
    sprintf(msgbuf, "pot_range_auto: %d;\n", pot_range_auto );
    Serial.write(msgbuf);
  } else if (strlen(tok) > 2) {
    sprintf(msgbuf, "Unknown command: '%s'\n", tok);
    Serial.write(msgbuf);
  }
}


int bootcount = 0;
void loop() {
  if (bootcount == 5000 / TIMESTEP) {
    read_pot_rom(); // read ROM at 5s mark.
    sprintf(msgbuf, "%s Ready\n", DEVICE_NAME);
    Serial.print(msgbuf);
  }
  if (bootcount <= 10000 / TIMESTEP) bootcount++; // bootup time lasts no more than 10 second

  // Every time, behave:

  listen_command();

  read_pot(); // Read potmeter value

  if ( pot_guard_r < 0 ) // remote control disabled
    set_pwm_by_pot();   // set PWM by potmeter
  else {                // remote control enabled
    set_pwm_by_rem();   // set PWM by remote
    if ( abs(pot_r - pot_guard_r) > pot_guard_t ) { // pot different from guard value by more than threshold
      if ( (pot_r > pot_guard_r && pot_r > pot_r_max * (long)rem_r / 100) ||
           (pot_r < pot_guard_r && pot_r < pot_r_max * (long)rem_r / 100) ) { // pot increasing(decreasing and greater(less) than remote
        pot_guard_r = -pot_guard_t; // disable remote, listen to pot again.
      }
    }
  }

  //  if (DEBUG) {
  //    sprintf(msgbuf, "pot_r: %03d<%03d<%03d; guard: %03d; thres: %03d; rem_r: %03d; pwm_r: %03d;\n",
  //                   pot_r_min, pot_r, pot_r_max, pot_guard_r, pot_guard_t, (word)rem_r*pot_r_max/100, pwm_r);
  //    Serial.print(msgbuf);
  //  }

  write_pwm(); // update pwm

  delay(TIMESTEP);
}
