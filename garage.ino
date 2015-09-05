#include <avr/sleep.h>
#include <avr/power.h>
#include <math.h>
#include <pitches.h>

/* Signals */
// Visual
int ledPin = 13;

// Auditory
int speakerPin = 3;
int start_tune[] = {NOTE_C4, NOTE_C2, NOTE_C6};
int start_tune_timing[] = {750, 250, 500};
int status_tone = NOTE_G4;
int klaxon_tune[] = {
  NOTE_D3, NOTE_G4, NOTE_C5, NOTE_F6};
int klaxon_timing[] = {200, 175, 150, 200};

/* Interrupts */
// Door open switch
int doorPin = 2;

// Motion Sensor
int motionPin = 3;
int motionSensorWarmup = 30000;  // ms needed to get accurate reading

// garage door control pin
int doorControlPin = 4;

/* Other variables */
// variable to keep track of the delay, can be set by any interrupt
// to start, stop or reset the countdown timer.
volatile long start_ms;

// Run an initialization of we are freshly started.
boolean first_time;

// for the sleep timer, interrupt when clock overflows.
volatile int f_timer=0;
ISR(TIMER1_OVF_vect)
{
  /* set the flag. */
   if(f_timer == 0)
   {
     f_timer = 1;
   }
}

// Inactivity will trigger door to close.
const long doorDelay_ms = 3000;

void setup() {
  // disable the ADC, as it is uneccesary, and set the sleep mode.
  ADCSRA = 0;

  pinMode(ledPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  pinMode(doorPin, INPUT);
  digitalWrite(doorPin, HIGH);

  pinMode(motionPin, INPUT);
  digitalWrite(motionPin, HIGH);

  pinMode(doorControlPin, OUTPUT);
  digitalWrite(doorControlPin, LOW);

  /*** Configure the timer.***/  
  /* Normal timer operation.*/
  TCCR1A = 0x00; 

  /* Clear the timer counter register.
   * You can pre-load this register with a value in order to 
   * reduce the timeout period, say if you wanted to wake up
   * ever 4.0 seconds exactly.
   */
  TCNT1=0x0000; 

  /* Configure the prescaler for 1:1024, giving us a 
   * timeout of 4.09 seconds.
   */
  TCCR1B = 0x05;

  /* Enable the timer overlow interrupt. */
  TIMSK1=0x01;

  /* Disable all of the unused peripherals. This will reduce power
   * consumption further and, more importantly, some of these
   * peripherals may generate interrupts that will wake our Arduino from
   * sleep!
   */
  power_adc_disable();  // Analog Digital Converter
  power_spi_disable();  // Serial Peripheral Interface
  power_twi_disable();  // Two Wire Interface

  Serial.begin(9600);
  first_time = false;
}

void Initialize() {
  if (Serial) {
    Serial.println("Waiting for motion sensor to warmup.");
  }
  delay(motionSensorWarmup);
  if (Serial) {
    Serial.println("Ready");
  }
  first_time = false;
}

void loop() {
  if (first_time) {
    Initialize();
  }
  // wait until the door is closed to proceed
  while (is_door_open()) {
    light_and_sound(100, 10, 1);
    enterTimedSleep();
  }
  enableSleepMode();

  // At this point an interrupt has been seen, so we start the timer
  // and change the interrupt ISR to doorClosed().
  // The timer will return true when it has naturally reached 0,
  // false if the timer was stopped prematurely. If true, then
  // close the garage door.
  start_ms = millis();
  start_countdown();
  if (is_door_open()) {
    close_door();
  }
}

void enableSleepMode() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, doorOpen, LOW);
  cli();
  // sleep_bod_disable();
  sei();
  sleep_mode();
}

void enterTimedSleep(void)
{
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  attachInterrupt(1, resetTimer, HIGH);

  power_timer0_disable();
  power_timer2_disable();

  /* Now enter sleep mode. */
  sleep_mode();

  /* The program will continue from here after the timer or
     when the motion sensor picks up movementtimeout        */
  sleep_disable(); /* First thing to do is disable sleep. */
  detachInterrupt(1);
  f_timer = 0;

  /* Re-enable the peripherals. */
  power_timer0_enable();
  power_timer2_enable();
}

void start_countdown() {
  long now = millis();
  long delay = start_ms + doorDelay_ms;
  while (now < delay) {
    light_and_sound(100, 10, 1);
    enterTimedSleep();
    if (!is_door_open()) {
      return;
    }
    now = millis();
    // reset the delay counter in case start_ms was reset.
    delay = start_ms + doorDelay_ms;
  }
}


/*
 * Returns false if the pin is reading as HIGH, meaning closed.
 */
boolean is_door_open() {
  return (digitalRead(doorPin) != HIGH);
}

void doorOpen() {
  sleep_disable();
  detachInterrupt(0);
}

void resetTimer() {
  sleep_disable();
  detachInterrupt(1);
  start_ms = millis();
  f_timer = 0;
}

void close_door() {
  // Garage door goes LOW for door operations for at least 30ms.
  // Probably use a pull-down resistor to automatically drop
  // the voltage. Need to sink 20V.
  if (Serial) {
    Serial.println("Closing Door.");
  }
  light_and_sound(2000, 100, 2);
  digitalWrite(doorControlPin, HIGH);
  delay(35);
  digitalWrite(doorControlPin, LOW);
}

void light_and_sound(int duration_ms, int led_blink_speed, int sound_type) {
  int num_led_blinks = round(duration_ms/led_blink_speed);
  {for (int l=0;l<num_led_blinks;l++) {
    digitalWrite(ledPin, HIGH);
    delay(led_blink_speed);
    digitalWrite(ledPin, LOW);
    delay(led_blink_speed);}}
  {if (sound_type == 0) {
     for (int t=0;t<3;t++) {
       tone(speakerPin, start_tune[t], start_tune_timing[t]);
       delay(start_tune_timing[t] * 1.30);
     }
   } else if (sound_type == 1) {
     for (int i=0;i<duration_ms/200;i++) {
       tone(speakerPin, status_tone, 100);
       delay(100);
     }
   } else {
     for (int i=0;i<5;i++) {
       for (int t=0;t<4;t++) {
         tone(speakerPin, klaxon_tune[t], klaxon_timing[t]);
         delay(50);
         }
         delay((duration_ms-725)/5);
       }
   }}
}
