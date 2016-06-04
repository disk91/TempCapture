#include <SoftI2CMaster.h>
#include <CAT24M01.h>
#include <math.h>
#include <LowPower.h>

CAT24M01 EEPROM1(0);
float crFactor =1.0;

/* Pour info */
#define SCLPIN  5
#define SDAPIN  4

#define LED 13



// =======================================
// Init
void setup() {
  long r0,r1;

  Serial.begin(9600);
  EEPROM1.begin();   
  initEprom();
  
  pinMode(LED,OUTPUT);

 
}


// =============================================================
// Measure VCC from a comparaison with 1.1V internal reference
// This will ensure we have a correct reference 10% true...
long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

// ====================================================================
// Get Temp
typedef struct s_temp {
  uint16_t ts;      // sequence number
  uint8_t  t[6];    // Temperatures
} t_temp;
t_temp temps;

#define convert2decdeg(x) (round((((x*v) / 1024)-500)/10))
void refreshTemps() {
  long v = readVcc();
  long  a;
  
  a = analogRead(A6);
  temps.t[0] = (uint8_t)convert2decdeg(a);
  delay(100);
  a = analogRead(A5);
  temps.t[1] = (uint8_t)convert2decdeg(a);
  delay(100);
  a = analogRead(A4);
  temps.t[2] = (uint8_t)convert2decdeg(a);
  delay(100);
  a = analogRead(A3);
  temps.t[3] = (uint8_t)convert2decdeg(a);
  delay(100);
  a = analogRead(A2);
  temps.t[4] = (uint8_t)convert2decdeg(a);
  delay(100);
  a = analogRead(A1);
  temps.t[5] = (uint8_t)convert2decdeg(a);
}

void printTemps(void * _void) {
  t_temp * _temps = (t_temp *)_void;
  Serial.print(_temps->ts);Serial.print(";");
  Serial.print(_temps->t[0]);Serial.print(";");
  Serial.print(_temps->t[1]);Serial.print(";");
  Serial.print(_temps->t[2]);Serial.print(";");
  Serial.print(_temps->t[3]);Serial.print(";");
  Serial.print(_temps->t[4]);Serial.print(";");
  Serial.print(_temps->t[5]);Serial.println();
}
  
// =================================================
// EPROM MANAGEMENT
typedef struct s_eprom_val {
  long readAdr;
  long writeAdr;
} t_eprom_val;
t_eprom_val ev;

void initEprom() {
/* --- First time use only
 ev.readAdr = 0x8;
 ev.writeAdr = 0x8;
 EEPROM1.write(0,(uint8_t*)&ev,sizeof(ev));
*/

 EEPROM1.read(0,(uint8_t*)&ev,sizeof(ev));
 Serial.print("Current Read Address :"); Serial.println(ev.readAdr);
 Serial.print("Current Write Address :"); Serial.println(ev.writeAdr);
 
}

void addTemp(void * _void) {
  t_temp * t = (t_temp *) _void;
  EEPROM1.write(ev.writeAdr,(uint8_t*)t,sizeof(t_temp));
  ev.writeAdr += sizeof(t_temp);
  EEPROM1.write(0,(uint8_t*)&ev,sizeof(ev));
}

void flushTemp() {
 ev.readAdr = 0x8;
 ev.writeAdr = 0x8;
 EEPROM1.write(0,(uint8_t*)&ev,sizeof(ev));
}

void dumpTemp() {
 long r = ev.readAdr; 
 t_temp _t;
 while (r < ev.writeAdr){
  EEPROM1.read(r,(uint8_t*)&_t,sizeof(t_temp));
  printTemps(&_t);
  r+=sizeof(t_temp);
 } 
}

// =================================================
//
uint16_t index = 0;
unsigned long currentMillis = 0;
#define LOOPCYCLE_MS  (300000L)
//#define LOOPCYCLE_MS (20000L)
void loop() {
  int i;
  long l;
  bool on;
   
  refreshTemps();
  temps.ts = index++;
  addTemp(&temps);
//  printTemps(&temps);

  l = 0;  
  currentMillis = 0;
  do {
    if (Serial.available()) {
      char c = Serial.read();
      switch (c) {
        case 'd': // dump temp in flash
           dumpTemp();
           break;
        case 'f' : // flush temp in flash
           flushTemp();
           break; 
      }
    }
    // sleep au max 250MS ... wake up on UART interrupt 
    LowPower.idle(SLEEP_250MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
                SPI_OFF, USART0_ON, TWI_OFF);
    currentMillis += 250;
    
    // blink the led every 10 seconds
    l+=250;
    if ( l > 10000 && l <= 10250 ) {
      digitalWrite(LED,HIGH);
    } else if ( l > 10250 ) {
      digitalWrite(LED,LOW);
      l = 0;
    } 
    
  } while ( currentMillis < LOOPCYCLE_MS );

}
