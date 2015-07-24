// Teensy 2.0 RTTY Modulator
// Uses Fast PWM to produce ~8kHz 8bit audio

#include "baudot.h"
#include "pwmsine.h"

// Yeah, I really need to get rid of these globals.
// Thats next on the to-do list

char pflag = 0;
unsigned int sampleRate = 7750;
unsigned int tableSize = sizeof(sine)/sizeof(char);
unsigned int pstn = 0;
int sign = -1;
unsigned int change = 0;
unsigned int count = 0;
int sym = 0x00;

int fmark = 870;
int fspac = 700;
int baud = 45;
int bits = 8;
char lsbf = 0;

unsigned char bitPstn = 0;
int bytePstn = 0;
unsigned char tx = 1;

unsigned char charbuf = 0;
unsigned char shiftToNum = 0;
unsigned char justshifted = 0;

char msg[] = "\n\nCQ CQ CQ DE KG4SGP KG4SGP KG4SGP KN\n\n";

// compute values for tones.
unsigned int dmark = (unsigned int)((2*(long)tableSize*(long)fmark)/((long)sampleRate));
unsigned int dspac = (unsigned int)((2*(long)tableSize*(long)fspac)/((long)sampleRate));
int msgSize = sizeof(msg)/sizeof(char);
unsigned int sampPerSymb = (unsigned int)(sampleRate/baud);

void setup() {

  // stop interrupts for setup
  cli();

  // setup pins for output
  // "B7" Pin on Teensy 2.0
  DDRB = _BV(7);

  // teensy
  TCCR0A = _BV(COM0A1) | _BV(WGM01) | _BV(WGM00);
  TCCR0B = _BV(CS01);
  TIMSK0 = _BV(TOIE0); /* maybe also: _BV(OCIE0A) */

  // begin serial communication
  Serial.begin(9600);
  Serial.println("Started");

  // re-enable interrupts
  sei();
}

// int main
void loop() {
  if(pflag == 1){
    pflag = 0;
    Serial.println(sym);
  }
}

char calcAmp(){
  pstn += change;

  // if the position rolls off, change sign and start where you left off
  if(pstn >= tableSize) {
    pstn = pstn%tableSize;
    sign *= -1;
  }

  // return the pwm value
  return (char)(128+(sign*sine[pstn]));
}

// sets the character buffer, the current character being sent
void setCbuff(){
    int i = 0;

    // Note: the <<2)+3 is how we put the start and stop bits in
    // the baudot table is MSB on right in the form 000xxxxx
    // so when we shift two and add three it becomes 0xxxxx11 which is
    // exactly the form we need for one start bit and two stop bits when read
    // from left to right

    // try to find a match of the current character in baudot
    for(i = 0; i < (sizeof(baudot_letters)/sizeof(char)); i++){

      // look in letters
      if(msg[bytePstn] == baudot_letters[i]) {

        // if coming from numbers, send shift to letters
        if(shiftToNum == 1){
          shiftToNum = 0;
          charbuf = ((baudot[31])<<2)+3;
          justshifted = 1;
        } else {
          charbuf = ((baudot[i])<<2)+3;
        }
      }

      //look in numbers
      if(msg[bytePstn] != ' ' && msg[bytePstn] != 10
		&& msg[bytePstn] == baudot_figures[i]) {
        if(shiftToNum == 0){
          shiftToNum = 1;
          charbuf = ((baudot[30])<<2)+3;
          justshifted = 1;
        } else {
          charbuf = ((baudot[i])<<2)+3;
        }
      }      
      
      // for printing the char to serial
      sym = charbuf;
      pflag = 1;
    }
}

void setSymb(char mve){

    // if its a 1 set change to dmark other wise set change to dspace
    if((charbuf&(0x01<<mve))>>mve) {
      change = dmark;
    } else {
      change = dspac;
    }
}

// This interrupt run on every sample (7128.5 times a second)
// though it should be every 7128.5 the sample rate had to be set differently
// to produce the correct baud rate and frequencies (that agree with the math)
// Why?! I'm going to figure that one out
ISR(TIMER2_OVF_vect) {
  count++;
  if (tx == 0) return;

  // if we've played enough samples for the symbol were transmitting, get next symbol
  if (count >= sampPerSymb){
    count = 0;
    bitPstn++;

    // if were at the end of the character return to the begining of a
    // character and grab the next one
    if(bitPstn > (bits-1)) {
      bitPstn = 0;
      
      // dont increment bytePstn if were transmitting a shift-to character
      if(justshifted != 1) {
        bytePstn++;
      } else {
        justshifted = 0;
      }
      
      setCbuff();
      // if were at the end of the message, return to the begining
      if (bytePstn > (msgSize-2)){
        // clear variables used here
        bitPstn = 0;
        bytePstn = 0;
        count = 0;
        return;
      }
    }

    unsigned char mve = 0;
    // endianness thing
    if (lsbf != 1) {
      // MSB first
      mve = (bits-1)-bitPstn;
    } else {
      // LSB first
      mve = bitPstn;
    }

    // get if 0 or 1 that were transmitting
    setSymb(mve);
  }

  // set PWM duty cycle
  OCR0A = calcAmp();
}
