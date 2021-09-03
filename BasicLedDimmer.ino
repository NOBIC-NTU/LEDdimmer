//#include <EEPROM.h> // Storage of calibration values


// TEENSY4 PINS
// Gate PINs wired to the gates of three MOSFETs (R,G,B)
#define GATE_R LED_BUILTIN // pin 13
#define GATE_G 14
#define GATE_B 15
// PWM Gating spec
#define PWM_FREQ 375 // kHz PWM frequency (default 4.482kHz on Teensy 4.0)
#define PWM_BITS 8 // bits PWM bit-depth (default 8-bit on Teensy 4.0)
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255
// Potmeters on Analog ins:
#define POT_R 16
#define POT_G 17
#define POT_B 18

// TODO: POT minima and maxima could be calibrated and ROM-stored
#define POT_MAX 540 // 1k:1k potmeter voltage divider, 1023 ADC Max
#define POT_MIN 10 // not zero

void setup() {
  Serial.begin(115200);
  analogWriteFrequency(GATE_R, PWM_FREQ); // Available in Teensy 
  analogWriteFrequency(GATE_G, PWM_FREQ);
  analogWriteFrequency(GATE_B, PWM_FREQ);
  analogWriteResolution(PWM_BITS); // integer duty-cycle range bit-depth // Available in Teensy
  pinMode(GATE_R, OUTPUT);  // sets the pin as output
  pinMode(GATE_G, OUTPUT);
  pinMode(GATE_B, OUTPUT);
  // start up intensity TODO: replace by EEPROM readout
  analogWrite(GATE_R, (int)1 * PWM_MAX / 100); // 32/256 = 1/8th = 12.5% duty-cycle
  analogWrite(GATE_G, (int)1 * PWM_MAX / 100); // 64/256 = 1/4
  analogWrite(GATE_B, (int)1 * PWM_MAX / 100); // 128/256 = 1/2
}

int pwm_r = 0; // Value between 0-PWM_MAX
int pwm_g = 0;
int pwm_b = 0;
void loop() {
  int pot_r, pot_g, pot_b;
  pot_r = analogRead(POT_R); // analog read of potentiometer
  pot_g = analogRead(POT_G);
  pot_b = analogRead(POT_B);
  pwm_r = PWM_MAX*abs(pot_r-POT_MIN)/abs(POT_MAX-POT_MIN); // escale pot meter to pwm value
  pwm_g = PWM_MAX*abs(pot_g-POT_MIN)/abs(POT_MAX-POT_MIN);
  pwm_b = PWM_MAX*abs(pot_b-POT_MIN)/abs(POT_MAX-POT_MIN);
  Serial.print("\t"); // display readout & settings
  Serial.print(pot_r);  Serial.print(" -> ");  Serial.print(pwm_r);
  Serial.print("\t");
  Serial.print(pot_g);  Serial.print(" -> ");  Serial.print(pwm_g);
  Serial.print("\t");
  Serial.print(pot_b);  Serial.print("-> ");  Serial.print(pwm_b);
  Serial.println("");
  analogWrite(GATE_R, (int) pwm_r); // apply pwm duty cycle
  analogWrite(GATE_G, (int) pwm_g);
  analogWrite(GATE_B, (int) pwm_b);
  delay(250);
}

//#include <InternalTemperature.h> // Teensy 4 library:
// https://github.com/LAtimes2/InternalTemperature
