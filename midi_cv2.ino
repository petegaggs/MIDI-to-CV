/* 
  midi_cv
  MIDI to CV converter
  control voltage provided by MCP4821 or MCP4822 SPI DAC  
  uses Arduino MIDI library
  was origninally developed for a monotron, this version is general purpose
  OSC_3340_BUILD is for AS3340 based design, which uses 0.504V/Octave instead of the usual 1V
  
 * MIT License
 * Copyright (c) 2023 Peter Gaggs
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.  
  */

// comment out the line below for use with the original hardware and 1V/Octave
#define OSC_3340_BUILD

#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

// inslude the SPI library:
#include <SPI.h>
//include the midi library:
MIDI_CREATE_DEFAULT_INSTANCE();

//MIDI_CREATE_DEFAULT_INSTANCE();
#define GATE_PIN 9 //gate control
#ifdef OSC_3340_BUILD
// Settings for new hardware design
#define SLAVE_SELECT_PIN 7 //spi chip select
#define MIDI_BASE_NOTE 12 //C0
#define DAC_SCALE_PER_SEMITONE 42 // 0.504 volts per octave
#else
// Settings for old hardware
#define SLAVE_SELECT_PIN 10 //spi chip select
#define MIDI_BASE_NOTE 36 //C2
#define DAC_SCALE_PER_SEMITONE 83.333333333 // 1 volt per octave
#endif

//MIDI variables
uint8_t currentMidiNote; //the note currently being played
uint8_t keysPressedArray[128] = {0}; //to keep track of which keys are pressed
uint16_t midiNoteControl;
uint16_t midiPitchBendControl = 0; // not implemented yet

void setup() {
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
  pinMode (GATE_PIN, OUTPUT); //gate output
  digitalWrite(GATE_PIN,LOW); //turn note off
  dacWrite(1000); //set the pitch just for testing
}

void loop() {
  MIDI.read();
}

void dacWrite(uint16_t value) {
  //write a 12 bit number to the MCP4821/MCP4822 DAC
  // take the SS pin low to select the chip:
  digitalWrite(SLAVE_SELECT_PIN,LOW);
  //send a value to the DAC
  SPI.transfer(0x10 | ((value >> 8) & 0x0F)); //bits 0..3 are bits 8..11 of 12 bit value, bits 4..7 are control data 
  SPI.transfer(value & 0xFF); //bits 0..7 of 12 bit value
  // take the SS pin high to de-select the chip:
  digitalWrite(SLAVE_SELECT_PIN,HIGH); 
}

void updateNotePitch() {
  // update note pitch, taking into account midi note and midi pitchbend if implemented
  dacWrite(midiNoteControl + midiPitchBendControl);
}

void HandleNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) { 
  // this function is called automatically when a note on message is received 
  keysPressedArray[pitch] = 1;
  synthNoteOn(pitch);
}

void handleNoteOff(uint8_t channel, uint8_t pitch, uint8_t velocity)
{
  keysPressedArray[pitch] = 0; //update the array holding the keys pressed 
  if (pitch == currentMidiNote) {
    //only act if the note released is the one currently playing, otherwise ignore it
    int highestKeyPressed = findHighestKeyPressed(); //search the array to find the highest key pressed, will return -1 if no keys pressed
    if (highestKeyPressed != -1) { 
      //there is another key pressed somewhere, so the note off becomes a note on for the highest note pressed
      synthNoteOn(highestKeyPressed);
    }    
    else  {
      //there are no other keys pressed so proper note off
      synthNoteOff();
    }
  }  
}

int findHighestKeyPressed(void) {
  //search the array to find the highest key pressed. Return -1 if no keys are pressed
  int highestKeyPressed = -1; 
  for (int count = 0; count < 127; count++) {
    //go through the array holding the keys pressed to find which is the highest (highest note has priority), and to find out if no keys are pressed
    if (keysPressedArray[count] == 1) {
      highestKeyPressed = count; //find the highest one
    }
  }
  return(highestKeyPressed);
}

void synthNoteOn(uint8_t note) {
  //starts playback of a note
  midiNoteControl = (((int16_t) note) - MIDI_BASE_NOTE) * DAC_SCALE_PER_SEMITONE;
  updateNotePitch();
  digitalWrite(GATE_PIN,HIGH); //turn gate on
  currentMidiNote = note; //store the current note
}

void synthNoteOff(void) {
  digitalWrite(GATE_PIN,LOW); //turn gate off
}

