#include <InternalTemperature.h> // Teensy 4 library:
// https://github.com/LAtimes2/InternalTemperature
#include <EEPROM.h> // Storage of last dimmer value

// Gate PINs wired to the gates of three MOSFETs (R,G,B)
#define GATE_R LED_BUILTIN // pin 13
#define GATE_G 14
#define GATE_B 15 

#define PWM_FREQ 375 // kHz PWM frequency (default 4.482kHz on Teensy 4.0)
#define PWM_BITS 8 // bits PWM bit-depth (default 8-bit on Teensy 4.0)
#define PWM_MAX 255 // i.e. 2**PWM_BITS = 255



// start reading from the first byte (address 0) of the EEPROM
int address = 0;
int counter = 0;

byte value;
float tempmin = 50;
float tempmax = 40;


void setup()
{
	Serial.begin(115200);
	analogWriteFrequency(GATE_R,PWM_FREQ);
  analogWriteFrequency(GATE_G,PWM_FREQ);
  analogWriteFrequency(GATE_B,PWM_FREQ);
  analogWriteResolution(PWM_BITS); // integer duty-cycle range bit-depth
	pinMode(GATE_R, OUTPUT);  // sets the pin as output
	pinMode(GATE_G, OUTPUT);
	pinMode(GATE_B, OUTPUT);
	// start up intensity TODO: replace by EEPROM readout
	analogWrite(GATE_R, (int)25*PWM_MAX/100); // 32/256 = 1/8th = 12.5% duty-cycle
	analogWrite(GATE_G, (int)50*PWM_MAX/100); // 64/256 = 1/4
	analogWrite(GATE_B, (int)75*PWM_MAX/100); // 128/256 = 1/2
}

void loop()
{
	// Readout current temperature
	float temp = InternalTemperature.readTemperatureC();
	// Adjust history min max temperature
	if ( temp > tempmax )	tempmax = temp;
	if ( temp < tempmin )	tempmin = temp;
	// Express current temperature in percent
	float temppc = 100.0*(temp-tempmin)/(tempmax-tempmin);
  
  analogWrite(GATE_R, (int)temppc*PWM_MAX/100); //  percent * MAX / 100%
  for (int i = 0 ; i<(int)temppc; i++) {
    analogWrite(GATE_G, (int)i*PWM_MAX/99); //  percent * MAX / 100%
    delay( 10 ); 
  }
//	
//	
//  // read a byte from the current address of the EEPROM
//  value = EEPROM.read(address);
//  
//  Serial.print(address);
//  Serial.print("\t");
//  Serial.print(value, DEC);
//  Serial.print("\t");
//  Serial.print((int)address*PWM_MAX/511);
//  Serial.print("\t");
  Serial.print(tempmin,2);
  Serial.print("\t");
  Serial.print(temp,2);
  Serial.print("\t");
  Serial.print(tempmax,2);
  Serial.print("\t");
  Serial.print(temppc,2);
  Serial.println();
//  
//  // advance to the next address of the EEPROM
//  address = address + 1;
//  
//  // there are only 512 bytes of EEPROM, from 0 to 511, so if we're
//  // on address 512, wrap around to address 0
//  if (address == 512)
//    address = 0;
//  // Teensy 1.0 has 512 bytes
//  // Teensy 2.0 has 1024 bytes
//  // Teensy++ 1.0 has 2048 bytes
//  // Teensy++ 2.0 has 4096 bytes
//    
    
  delay(5);
}
