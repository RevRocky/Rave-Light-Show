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
 * Original Authors: Rocky Petkov, Jack Qiao
 */

#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BluefruitLE_UART.h>
#include <Adafruit_BLE.h>
#include <SPI.h>
#include <Adafruit_ATParser.h>

#include "Codebase_Arduino.h"
#include "BluefruitConfig.h"

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif


#define FACTORYRESET_ENABLE 1 // Allows us to do a factory reset to put device in known good state!
#define FFT_N 128             // Not really using the library so i'll just define a constant to save memory.
#define MIC_ADC_CHANNEL 6     // Testing this with ADC Channel 9 (Note: Unable to find mapping for Flora Series)

#define PIXEL_PIN 12          // Neo Pixels are plugged into the D11 pin
#define NUM_COLOURS 1         // We display a maximum of one colour at a time. 
#define PIXEL_COUNT 24        // Number of pixels on our ring!

#define BLUEFRUIT_HWSERIAL_NAME Serial1

// A couple globals which are useful for processing incomming colours. 
const char *delimiters = {"[,]"};    // Delimiters for splitting strings
char       *pch;                     // Pointer to the lead character in our string
uint8_t colour[3 * NUM_COLOURS] = {0, 0, 0};    // Three values for each colour. Initialise it to 0.

// Initialise our NeoPixel object
Adafruit_NeoPixel ring = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
 
// Some Basic Bluetooth Things
Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

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
   char       *pch;                            // Pointer to the lead character in our string

   // Waiting for the serial link to be established
   while(!Serial);
   delay(500);
  
   // Start Serial communications so we can do output. 
   Serial.begin(115200);
   Serial.println(F("Welcome to the Rave Light Show!"));
   
  // Factory reset the bluetooth module so that everything is in a safe place
  // First need to begin verbose mode, or factory reset wont work.
  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  
  Serial.println( F("OK!") );

  if (FACTORYRESET_ENABLE)
  {
    Serial.println("Resetting Bluetooth Module:");
    if (!ble.factoryReset()) {
      error(F("Unable to factory reset"));
    }
  }
  /* Disable command echo from Bluefruit 
     Need to disable this to establish connection. */
  ble.echo(false);
  ble.verbose(false);  // debug info is a little annoying after this point!
  Serial.println("Requesting Bluefruit info:");
  ble.info();

   Serial.println(F("Initialising the lights"));
  ring.begin();
  ring.show();

  // Wait for the incomming bluetooth connexion
  Serial.println("Waiting for connection...");
  
  while (! ble.isConnected()) {
    Serial.println(F("Stallin like Stalin!"));
    delay(500);
  }


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
  
  int i, j;                                       // Loop counters
  // Check for incoming characters from Bluefruit
  ble.println("AT+BLEUARTRX");
  ble.readline();
  Serial.println(ble.buffer);
  if (strcmp(ble.buffer, "OK") == 0) {
  }
  else {
      
      // Converting byte string into integers
      pch = strtok(ble.buffer, delimiters);
      i = 0;

     
      // While loop terminates when we run out of tokens or we have read more than 3 tokens
      while (pch != NULL && i < (3 * NUM_COLOURS)) {
        colour[i] = atoi(pch);    // Translate pch into an int and store in colour
        //Serial.println(pch);
        pch = strtok(NULL, delimiters);
        //Serial.println(colour[i]);
        i++;
      }
    ble.waitForOK();
    }

   if ((colour[0] | colour[1] | colour[2])) {
     blockPattern(colour);  
   }   
}  


/*
 * The simplest pattern. Naively displays the same colour 
 * for all of the lights in the ring.
 */
void blockPattern(uint8_t *colour) {
  int light;
  for(light = 0; light < PIXEL_COUNT; light++) {
    // Set the ring colour
    ring.setPixelColor(light, colour[0], colour[1], colour[2], 255);
  }
  ring.show();  // And latch them in!
}


