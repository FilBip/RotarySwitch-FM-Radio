/*
 rotary-switch-radio-FM
  The receiver is an RDA5807M module driven by an Arduino Pro Mini.
 
 Philippe Leclercq 2020
 */
 
#define DEBUG 0

#include <Wire.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

unsigned prevChan=0, prevVol=0;


// ----- Register Definitions -----

#define FM_WIDTH 210
#define FM_MIN  870
#define FM_MAX 1080

#define RADIO_REG_CHIPID  0x00

#define RADIO_REG_CTRL    0x02
#define RADIO_REG_CTRL_OUTPUT 0x8000
#define RADIO_REG_CTRL_UNMUTE 0x4000
#define RADIO_REG_CTRL_MONO   0x2000
#define RADIO_REG_CTRL_BASS   0x1000
#define RADIO_REG_CTRL_SEEKUP 0x0200
#define RADIO_REG_CTRL_SEEK   0x0100
#define RADIO_REG_CTRL_RDS    0x0008
#define RADIO_REG_CTRL_NEW    0x0004
#define RADIO_REG_CTRL_RESET  0x0002
#define RADIO_REG_CTRL_ENABLE 0x0001

#define RADIO_REG_VOL     0x05
#define RADIO_REG_VOL_VOL   0x000F

#define RDA5807M_REG_RSSI 0x0B
#define RDA5807M_RSSI_MASK 0xFE00
#define RDA5807M_RSSI_SHIFT 10

// I2C-Address RDA Chip for sequential  Access
#define I2C_RDA_SEQ  0x10

// I2C-Address RDA Chip for Index  Access
#define I2C_RDA_INDX  0x11

void setup() 
{
#if DEBUG
  Serial.begin(115200); 
  Serial.println("\nRadio...");
#endif

  wdt_disable(); //  Disable the watchdog timer first thing, in case it is misconfigured
  setup_watchdog(WDTO_500MS); //  Run ISR(WDT_vect) every x ms:

  // Initialize the Radio 
  Wire.begin();   
  WriteReg(RADIO_REG_CTRL, RADIO_REG_CTRL_RESET); 
  WriteReg(RADIO_REG_CTRL, RADIO_REG_CTRL_OUTPUT|RADIO_REG_CTRL_UNMUTE|RADIO_REG_CTRL_MONO|RADIO_REG_CTRL_NEW|RADIO_REG_CTRL_ENABLE); 
  WriteReg(RADIO_REG_VOL, 0x9080 | 2 ); // Max vol (1 - 15) External pot to amp
}

void loop()
{
  unsigned chan;
  byte d1,d2,d3;
  d1 = readRotary(A0)+8;
  d2 = readRotary(A1);
  d3 = readRotary(A2);
  
  chan = d1*100+d2*10+d3;
     
  if (chan < FM_MIN)
    chan = FM_MIN;
  else
  if (chan > FM_MAX)
    chan = FM_MAX;

  if (chan != prevChan) {
#if DEBUG
     Serial.print (" Tuned to ");
     Serial.println (prevChan);
    Serial.println (chan);
#endif
     prevChan = chan;
     TuneTo(chan);
  }
  
 //  delay (1000); // pause!
  
  sleeping(); //  Go to sleep forever... or until the watchdog timer fires!
}

byte readRotary(byte rotary) {
  byte rotaryVal = 9;
  int analogVal = analogRead(rotary);

  rotaryVal = (analogVal + 30) / 100;
  if (rotaryVal > 9) rotaryVal = 9;
 
#if DEBUG
     Serial.print (" val = ");
     Serial.print (analogVal);
     Serial.print (" rot = ");
     Serial.println (rotaryVal);
#endif

  return(rotaryVal);
}

void TuneTo( unsigned ch)
{
       byte numH,numL;

       ch -= FM_MIN;
       numH=  ch>>2;
       numL = ((ch&3)<<6 | 0x10); 
       Wire.beginTransmission(I2C_RDA_INDX);
       Wire.write(0x03);
       Wire.write(numH);                     // write frequency into bits 15:6, set tune bit         
       Wire.write(numL);
       Wire.endTransmission();
}

//RDA5807_addr=0x11;       
// I2C-Address RDA Chip for random access
void WriteReg(byte reg,unsigned int val)
{
  Wire.beginTransmission(I2C_RDA_INDX);
  Wire.write(reg); Wire.write(val >> 8); Wire.write(val & 0xFF);
  Wire.endTransmission();
}

 
ISR(WDT_vect) {
//  Anything you use in here needs to be declared with "volatile" keyword
//  Track the passage of time.
}

void sleeping() {

  byte adcsra = ADCSRA;          //save the ADC Control and Status Register A
  ADCSRA = 0;                    //disable the ADC to reduce power during sleep

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();

  ADCSRA = adcsra;               //restore ADC Control and Status Register A
}

void setup_watchdog(int timerPrescaler)
{
  if (timerPrescaler > 9 ) timerPrescaler = 9; // Correct incoming amount if need be
  byte bb = timerPrescaler & 7;
  if (timerPrescaler > 7) bb |= (1<<5); // Set the special 5th bit if necessary

  MCUSR &= ~(1<<WDRF); // Clear the watch dog reset
  WDTCSR |= (1<<WDCE) | (1<<WDE); // Set WD_change enable, set WD enable
  WDTCSR = bb; // Set new watchdog timeout value
  WDTCSR |= _BV(WDIE);  // Set the interrupt enable, this will keep unit from resetting after each int
}
