#include <Encoder.h>
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
 * Remote (serial port) vs Local (encoder) control behavior.
 * Let's have a simple rule: if remote is adjusted, do it.
 * If encoder is adjusted, do it (it's a relative value).
*/

/*
 *  Two channels, each with a different attenuation setting
 *  Channel 0 (A) goes to 35% of full duty cycle.
 *  Channel 1 (B) goes to 5% of full duty cycle.
 */

#define DEBUG 0
#define DUALCHAN 1 // 0:single 1:dual chan mode
#define TIMESTEP 10 // ms

#define DEVICE_NAME "LEDdimmer 1.5.1 (arduino, itsybitsy 32u4 5V)"
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255
#define GATE_A LED_BUILTIN // MOSFET GATE PWM out; pin 13 attached to on board LED
#define GATE_B 11 // MOSFET GATE PWM out; pin 11 (12 has no PWM support)

#define ENCODER_USE_INTERRUPTS
Encoder myEnc(1, 7); // PINS 1 7 are bound to Interrupts #3 #4
long pos_prev = 0;

const byte togglepin = 0; // PIN 0 Interrupt #2
volatile bool ch = 0; // Toggle between channels 0 and 1 (false and true)

volatile uint64_t togglestamp = 0;
void toggleaction() {
  if (millis() > togglestamp + 10) { // last event was a pause ago
    togglestamp = millis(); // timestamp this event
    if (DUALCHAN) ch = !ch; // toggle channel
    if (DEBUG) Serial.println(ch ? "ch B" : "ch A"); // Always reply something
  }
}

int get_encoder_delta() {
  // return the size of the step of the encoder (positive or negative)
  long pos_curr = myEnc.read();
  int delta = pos_curr - pos_prev; // calculate position difference
  if (delta != 0) {
    pos_prev = pos_curr; // update previous value to current
  }
  return delta;
}


void setup() {
  Serial.begin(115200);
  pinMode(GATE_A, OUTPUT);  // Configure the pin as output
  analogWrite(GATE_A, (int) 1 * PWM_MAX / 100); // 1% duty cycle;
  pinMode(GATE_B, OUTPUT);  // Configure the pin as output
  analogWrite(GATE_B, (int) 1 * PWM_MAX / 100); // 1% duty cycle;
  pinMode(togglepin, INPUT_PULLUP); // Press to bring low, release to trigger RISING event:
  attachInterrupt(digitalPinToInterrupt(togglepin), toggleaction, RISING);
}

bool on_rg[] = {1,1}; // Enable LEDs?
int pwm_rg[] = {0,0}; // Value between 0 and PWM_MAX
// Custom hard cap on PWM output: 35% and 5%, since luminosity isnt linear:
// Look up in CIE array what index has 35% of 255 = 89.25 = 89 -> 168
//                        and which has 5% of 255 = 12.75 = 12 -> 68
int pwm_rg_max[] = {168,69};
int pos_rg[] = {0,0}; // Channel value in encoder units (0-100 ~ 96=24 ticks*4 delta per tick)
int rem_rg[] = {0,0}; // Channel value in remote control units (0-100%)
char msgbuf[256]; // line buffer to print messages

void set_pwm_by_enc() {
  // Local adjusts pwm
  // a) normalize encoder range, then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  pwm_rg[ch] = pwm_rg_max[ch] * cie[ on_rg[ch] * CIE_SIZE * pos_rg[ch] / 100 ] / CIE_RANGE;
  // Update remote value (scale pos range to rem range (0-100))
  rem_rg[ch] = on_rg[ch] * pos_rg[ch] ;
  if (DEBUG) {
    sprintf(msgbuf, "set_pwm_by_enc: (ch %s) = %d, pos: %d\n", (ch?"B":"A"), pwm_rg[ch], pos_rg[ch]);
    Serial.write(msgbuf);
  }
}
void set_pwm_by_rem() {
  // Remote adjusts pwm
  // a) normalize remote range, then
  // b) rescale to CIE_SIZE (table length), then
  // c) look up CIE 1931 luminosity, then
  // d) rescale CIE table output to PWM_MAX range
  int prev_pwm = pwm_rg[ch];
  pwm_rg[ch] = pwm_rg_max[ch] * cie[ on_rg[ch] * CIE_SIZE * rem_rg[ch] / 100 ] / CIE_RANGE;
  // Update local pos
  pos_rg[ch] = on_rg[ch] * (long) rem_rg[ch] ;
  if (DEBUG && prev_pwm != pwm_rg[ch]) {
    sprintf(msgbuf, "set_pwm_by_rem: (ch %s) = %d\n", (ch?"B":"A"), pwm_rg[ch]);
    Serial.write(msgbuf);
  }
}
void write_pwm() {
  analogWrite(GATE_A, pwm_rg[0]); // apply pwm duty cycle
  analogWrite(GATE_B, pwm_rg[1]); // apply pwm duty cycle
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
  } else if (strcmp(tok, "ch?") == 0 || strcmp(tok, "ch?") == 0) {
    Serial.println(ch ? "ch B" : "ch A"); // channel true is B, false is A
  } else if (strcmp(tok, "on?") == 0 || strcmp(tok, "off?") == 0) {
    Serial.print(on_rg[0] ? "on " : "off ");
    Serial.println(on_rg[1] ? "on " : "off ");
  } else if (strcmp(tok, "A") == 0 || strcmp(tok, "B") == 0) {
    if (DUALCHAN) ch = (strcmp(tok, "B") == 0); // channel B is ch=1
    Serial.println(ch ? "ch B" : "ch A"); // Always reply something
  } else if (strcmp(tok, "on") == 0 || strcmp(tok, "off") == 0) {
    on_rg[ch] = (strcmp(tok, "on") == 0);
    Serial.print(on_rg[0] ? "on" : "off"); // Always reply something
    if (DUALCHAN) Serial.print(on_rg[1] ? "  on" : " off");
    Serial.println();
  } else if (strcmp(tok, "I") == 0) {
    // remote set intensity (0...100)
    tok = strtok(NULL, " \n");
    long val = strtol(tok, NULL, 10);
    if (val < 0 || val > 100) {
      Serial.println("intensity invalid (out of range 0-100)");  // Always reply something
    } else {
      rem_rg[ch] = val; // update remote value
      sprintf(msgbuf, "I %d\n", rem_rg[ch]);
      Serial.write(msgbuf);
    }
  } else if (strcmp(tok, "I?") == 0) {
    // print intensity in percentage, use remote percentage setting
    sprintf(msgbuf, "I %03d\n", rem_rg[ch] );
    Serial.write(msgbuf);
  } else if (strlen(tok) > 2) {
    sprintf(msgbuf, "Unknown command: '%s'\n", tok);
    Serial.write(msgbuf);
  }
}

void loop() {

  listen_command(); // Listen and process remote control command, if any

  int delta = get_encoder_delta(); // Collect encoder change, if any

  if (delta!=0) { // encoder did step!
    pos_rg[ch] += delta; // adjust pos value accordingly, but
    if (pos_rg[ch]>100) pos_rg[ch] = 100; // clip to range maximum
    if (pos_rg[ch]<0) pos_rg[ch] = 0; // clip to range minimum
    set_pwm_by_enc();   // set PWM value by encoder position
  }
  else { // encoder was untouched, allow remote change, if any
    set_pwm_by_rem();   // set PWM value by remote value
  }

  write_pwm(); // sync pwm value, update hardware PWM setting.

  delay(TIMESTEP);
}
