
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

/*
  midi_cv
  MIDI to CV converter
  control voltage provided by MCP4821 SPI DAC  
  uses Arduino MIDI library
  this one uses the proper note off function, longer delay before turning on the gate to get the AR gen to re-trigger
  It's a bit simpler, just last note priority nothing complicated
  */

// inslude the SPI library:
#include <SPI.h>
//include the midi library:
MIDI_CREATE_DEFAULT_INSTANCE();

//low power stuff
#include <avr/power.h>
#include <avr/sleep.h>

//MIDI_CREATE_DEFAULT_INSTANCE();
#define GATE_PIN 9 //gate control
#define SLAVE_SELECT_PIN 10 //spi chip select

#define DAC_BASE -3000 //-3V offset 
#define DAC_SCALE_PER_SEMITONE 83.333333333
#define GATE_RETRIGGER_DELAY_US 10000 //time in microseconds to turn gate off in order to retrigger envelope

//MIDI variables
bool notePlaying = false;

void setup() {
  //reduce the power a bit
  power_adc_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_twi_disable();
  //enable pullup resistor on unused pins
  digitalWrite(2,HIGH);
  digitalWrite(3,HIGH);
  digitalWrite(4,HIGH);
  digitalWrite(5,HIGH);
  digitalWrite(6,HIGH);
  digitalWrite(7,HIGH);
  digitalWrite(8,HIGH);
  //MIDI stuff
  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);      
  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  // Do the same for NoteOffs
  MIDI.setHandleNoteOff(handleNoteOff);
  //SPI stuff
  // set the slaveSelectPin as an output:
  pinMode (SLAVE_SELECT_PIN, OUTPUT);
  digitalWrite(SLAVE_SELECT_PIN,HIGH); //set chip select high
  // initialize SPI:
  SPI.begin(); 
  //Serial.begin(9600); //for debug, can't use midi at the same time!
  pinMode (GATE_PIN, OUTPUT); //gate for monotron
  digitalWrite(GATE_PIN,LOW); //turn note off
  dacWrite(1000); //set the pitch just for testing
}

void loop() {
  MIDI.read();
}

void dacWrite(int value) {
  //write a 12 bit number to the MCP8421 DAC
  if ((value < 0) || (value > 4095)) {
    value = 0;
  }
  // take the SS pin low to select the chip:
  digitalWrite(SLAVE_SELECT_PIN,LOW);
  //send a value to the DAC
  SPI.transfer(0x10 | ((value >> 8) & 0x0F)); //bits 0..3 are bits 8..11 of 12 bit value, bits 4..7 are control data 
  SPI.transfer(value & 0xFF); //bits 0..7 of 12 bit value
  // take the SS pin high to de-select the chip:
  digitalWrite(SLAVE_SELECT_PIN,HIGH); 
}

void setNotePitch(int note) {
  //receive a midi note number and set the DAC voltage accordingly for the pitch CV
  dacWrite(DAC_BASE+(note*DAC_SCALE_PER_SEMITONE)); //set the pitch of the oscillator
}

void HandleNoteOn(byte channel, byte pitch, byte velocity) { 
  // this function is called automatically when a note on message is received 
  setNotePitch(pitch); //set the oscillator pitch
  if(notePlaying) {
    digitalWrite(GATE_PIN,LOW); //turn gate off momentarily to retrigger AR gen
    delayMicroseconds(GATE_RETRIGGER_DELAY_US);
  }
  digitalWrite(GATE_PIN,HIGH); //turn gate on
  notePlaying = true;
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  digitalWrite(GATE_PIN,LOW); //turn gate off
  notePlaying = false;
}


