/*
 * CISC 340: Team Seven, Project One
 * 
 * This module is responsible for taking sound coming from the microphone 
 * and then sending it out to a Bluetooth 4.0 (BT LE) capable host for more 
 * complex signal processing that would be well beyond the capabilities of 
 * the Arduino Microcontroller.
 * 
 * This Code Requires the following hardware components:
 *    - Any ATmega 328P arduino Board or Better
 *    - Bluetooth LE module
 *    - Adafruit Electret Microphone Amplifier
 * 
 * Original Authors: Rocky Petkov, (Add your name here)
 */

#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BluefruitLE_UART.h>
#include <Adafruit_BLE.h>
#include <SPI.h>
#include <Adafruit_ATParser.h>
#include "BluefruitConfig.h"

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif


#define FACTORYRESET_ENABLE 1 // Allows us to do a factory reset to put device in known good state!
#define FFT_N 128             // Not really using the library so i'll just define a constant to save memory.
#define MIC_ADC_CHANNEL 6     // Testing this with ADC Channel 9 (Note: Unable to find mapping for Flora Series)
#define PIXEL_PIN 11          // Neo Pixels are plugged into the D11 pin

#define NUM_COLOURS 1         // We display a maximum of one colour at a time. 

// TODO: Definethe data in and data out pins as constants. 
int16_t capture[FFT_N];                   // Audio Capture Buffers
volatile byte samplePos = 0;              // Position Counter for the Buffer

// TODO: NeoPixel Variables
uint8_t colour[3 * NUM_COLOURS] = {0, 0, 0};    // Three values for each colour. Initialise it to 0. 

// Some Basic Bluetooth Things

Adafruit_BluefruitLE_UART ble(Serial1, BLUEFRUIT_UART_MODE_PIN);

/*
 * A small helper to let us know if there is an error
 */
 void error(const __FlashStringHelper *err) {
  Serial.println(err);
  while (1);
 }

/*
 * Presently, the Setup Function is mostly enabling the analogue-digital
 * conversion to run on it's own with out having to call analogue read every 
 * time. This would be much too slow to do real time audio processing, especially
 * when we are doing it over a network. 
 */
void setup() {

   // Waiting for the serial link to be established
   while(!Serial);
   delay(500);
  
   // Start Serial communications so we can do output. 
   Serial.begin(9600);
   Serial.println(F("Welcome to the Rave Light Show!"));
   
  
  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  ADMUX  = MIC_ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
  ADCSRA = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE)  | // Interrupt enable
           _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz ((SAMPLE RATE))
  ADCSRB = 0;                // Free run mode, no high MUX bit
  DIDR0  = 1 << MIC_ADC_CHANNEL; // Turn off digital input for ADC pin
  TIMSK0 = 0;                // Timer0 off

  sei();  // Enabling interrupts. 

  

  // Factory reset the bluetooth module so that everything is in a safe place

  
  Serial.println("Resetting Bluetooth Module:");

//  if (!ble.factoryReset()) {
//    error(F("Unable to factory reset"));
//  }

  ble.info();

  // Wait for the incomming bluetooth connexion
  Serial.println("Waiting for connection...");
  
  while (! ble.isConnected()) {
    Serial.println(F("Stallin like Stalin!"));
    delay(500);
  }

  // Disabling command echo from Bluefruit
  ble.echo(false);

  Serial.println("\n\nBeginning Main Loop...");
}

/*
 * This loop works by task switching between handling input and output.
 * Currently I am taking the naive approach where I simply switch between tasks 
 * with out using any of the fancier constructs of the language. 
 * 
 * Steps:
 *  1) Wait on input from Microphone
 *  2) Check Bluetooth Pin, if there's data, receive and store
 *  3) Send data recorded from microphone
 *  4) Display lights based upon the data received
 *  
 *  Things to be Wary Of:
 *    1) The fact that we are dealing with an interrupt might complicate things.
 *      still, the code I'm working from does a DFT in the time between interrupts 
 *      so we should be good
 *      
 *    2) Memory
 *      We have somewhat limited memory to work with. This is unfortunate. 
 */
void loop() {
  int i;                                          // A loop counter for some development display type stuff
  uint8_t bytesRead;                              // No. of bytes read to the buffer
  
  while(ADCSRA & _BV(ADIE));  // Wait for audio sampling to finish
  samplePos = 0;              // Reset Sample Counter
  ADCSRA |= _BV(ADIE);        // Resumes the sampling interrupt
  
  ble.println("AT+BLEUARTRX");  // Data receive command
  ble.readline();               // Reads lines into our buffer!

  // Check if there is data. If so write it to colours!
  if (strcmp(ble.buffer, "OK") != 0) {
      memcpy(colour, ble.buffer, 3 * NUM_COLOURS);    // Copy colour information to the colours buffer
  
      // Debug output to ensure that we've received the colours correctly!   
      Serial.println("Colours for this loop around");
      for(i = 0; i < 3 * NUM_COLOURS; i++) {
        Serial.println(ble.buffer[i], HEX);   // Hex printing makes colours easy to check!
      }
  }
  
      

//  // TODO: Verify that this approach is indeed efficient. We simply send one value
//  // at a bloody time. 
//  ble.println("AT+BLEUARTTX");
//  for (i = 0; i < FFT_N; i++) {
//    ble.print(capture[i]);    // Print the ith value of capture
//  }
//
//    // Waiting to ensure that we are okay w.r.t. seding our data
//    if (! ble.waitForOK()) {
//    Serial.println(F("Failed to send buffer"));
//    }
}


/*
 * Below is an interrupt service routine
 * which takes 128 audio samples and normalises them
 * around 0 for the DFT.
 */
ISR(ADC_vect) { 
    static const int16_t noiseThreshold = 4;  // We can play around with this. We don't want to have lights run on ambient nothing!
    int16_t sample = ADC; // Raw voltage over the wire. 0 corresponds to 0 volts. 1023 corresponds to ~5v.

    // Normalise our samples around 0 for the DFT
    capture[samplePos] = 
      ((sample > (512 - noiseThreshold)) &&
      sample < (512 + noiseThreshold)) ? 0 : 
      sample - 512;  

      // Here we check if our buffer is full. If so we turn off our interrupt. 
      if(++samplePos >= FFT_N) {
          ADCSRA &= ~_BV(ADIE);
      }
}

