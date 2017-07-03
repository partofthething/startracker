
/*
 * Skytracker by Nick Touran for ESP8266 and Stepper Motors
 * 
 * partofthething.com
 * 
 * This accelerates the motor to correct the tangent error. It can rewind too!
 * 
 * Motors are 28BYJ-48 5V + ULN2003 Driver Board from Amazon
 * Hook up power and ground and then hook inputs 1-4 up to GPIO pins. 
 * 
 * See Also: http://www.raspberrypi-spy.co.uk/2012/07/stepper-motor-control-in-python/
 * This motor has a 1:64 gear reduction ratio and a stride angle of 5.625 deg (1/64th of a circle). 
 * 
 * So it takes 64*64 = 4096 single steps for one full rotation, or 2048 double-steps.
 * with 3 ms timing, double-stepping can do a full rotation in 2048*0.003 =  6.144 seconds 
 * so that's a whopping 1/6.144 * 60 = 9.75 RPM. But it has way more torque than I expected. 
 * 
 * Can get 2ms timing going with double stepping on ESP8266. Pretty fast!
 * Should power it off of external 5V. 
 * 
 * Shaft diameter is 5mm with a 3mm inner key thing. Mounting holes are 35mm apart. 
 * 
 */


#define NUM_PINS 4
#define NUM_STEPS 8
#define RADS_PER_SEC 7.292115e-05
#define LENGTH_CM 28.884 // fill in with precise measured value
// For theta zero, I used relative measurement between two boards w/ level.
// Got 0.72 degrees, which is 0.012566 radians
#define THETA0 0.012566 // fill in with angle at fully closed position (radians)
#define ROTATIONS_PER_CM 7.8740157  // 1/4-20 thread
#define DOUBLESTEPS_PER_ROTATION 2048.0  
#define CYCLES_PER_SECOND 80000000

//modes
#define NORMAL 0
#define REWINDING 1
#define STOPPED 2


int allPins[NUM_PINS] = {D1, D2, D3, D4};
int MODE_PIN = D7;

// from manufacturers datasheet
int STEPPER_SEQUENCE[NUM_STEPS][NUM_PINS] = {{1,0,0,1},
                                             {1,0,0,0},
                                             {1,1,0,0},
                                             {0,1,0,0},
                                             {0,1,1,0},
                                             {0,0,1,0},
                                             {0,0,1,1},
                                             {0,0,0,1}};

int step_delta; 
int step_num = 0;
double total_seconds = 0.0;
long totalsteps = 0;
double step_interval_s=0.001;
int *current_step;
volatile unsigned long next; // next time to trigger callback
volatile unsigned long now; // volatile keyword required when things change in callbacks
volatile unsigned long last_toggle; // for debounce
volatile short current_mode=NORMAL;
bool autostop=true;  // hack for allowing manual rewind at boot

float ypt(float ts) {
   // bolt insertion rate in cm/s: y'(t) 
   // Note, if you run this for ~359 minutes, it goes to infinity!!
   return LENGTH_CM * RADS_PER_SEC/pow(cos(THETA0 + RADS_PER_SEC * ts),2);
}

void inline step_motor(void) {
  /*  This is the callback function that gets called when the timer
   *  expires. It moves the motor, updates lists, recomputes
   *  the step interval based on the current tangent error, 
   *  and sets a new timer. 
   */
  switch(current_mode) {
    case NORMAL:
      step_interval_s = 1.0/(ROTATIONS_PER_CM * ypt(total_seconds)* 2 * DOUBLESTEPS_PER_ROTATION);
      step_delta = 1;  // single steps while filming for smoothest operation and highest torque
      step_num %= NUM_STEPS;
      break;
    case REWINDING:
      // fast rewind
      step_interval_s = 0.0025;  // can often get 2ms but gets stuck sometimes.
      step_delta = -2;  // double steps going backwards for speed.
      if (step_num<0) {
        step_num+=NUM_STEPS;  // modulus works here in Python it goes negative in C. 
      }
      break;
    case STOPPED:
      step_interval_s = 0.2;  // wait a bit to conserve power. 
      break;
  }

  if (current_mode!=STOPPED) {
    total_seconds += step_interval_s; // required for tangent error
    current_step = STEPPER_SEQUENCE[step_num];
    do_step(current_step);
    step_num += step_delta;  // double-steppin' 
    totalsteps += step_delta;
  }
  
  // Serial.println(totalsteps);
  // Before setting the next timer, subtract out however many
  // clock cycles were burned doing all the work above. 
  now = ESP.getCycleCount();
  next = now + step_interval_s * CYCLES_PER_SECOND - (now-next); // will auto-rollover.
  timer0_write(next);  // see you next time!
}

void do_step(int *current_step) {
   /* apply a single step of the stepper motor on its pins. */
   for (int i=0;i<NUM_PINS+1;i++) {
     if (current_step[i] == 1) {
        digitalWrite(allPins[i], HIGH);
     }
     else {
        digitalWrite(allPins[i], LOW);
     }
   }
}

void setup() {
  Serial.begin(115200);
  setup_gpio();
  setup_timer();


  // Convenient Feature: Hold button down during boot to do a manual rewind.
  // Press button again to set new zero point. 
  int buttonUp = digitalRead(MODE_PIN);
  if(not buttonUp) {
    Serial.println("Manual REWIND!");
    autostop=false;
    current_mode=REWINDING;
  }
}

void setup_timer() {
  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(step_motor);  // call this function when timer expires
  next=ESP.getCycleCount()+1000;
  timer0_write(next);  // do first call in 1000 clock cycles. 
  interrupts();
}

void setup_gpio() {
  
  for (int i=0;i<NUM_PINS+1;i++) {
     pinMode(allPins[i], OUTPUT);
  }
  all_pins_off();
  
  // Setup toggle button for some user input. 
  pinMode(MODE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MODE_PIN), toggle_mode, FALLING);
}

void all_pins_off() {
  for (int i=0;i<NUM_PINS+1;i++) {
    digitalWrite(allPins[i], LOW); 
  }
}

void toggle_mode() {
  /*  We have several modes that we can toggle between with a button, 
   *  NORMAL, REWIND, and STOPPED.
   */
  if(ESP.getCycleCount() - last_toggle < 0.2*CYCLES_PER_SECOND) //debounce
  {
    return; 
  }
  if (current_mode == REWINDING){
    Serial.println("STOPPING");
    current_mode = STOPPED;
    all_pins_off();
    if (not autostop) {
      // Reset things after a manual rewind.
      step_num = 0;
      total_seconds = 0.0;
      totalsteps=0;
      autostop=true;
    }
  }
  else if (current_mode == NORMAL) {
    Serial.println("Rewinding.");
    current_mode = REWINDING;
  }
  else {
    Serial.println("Restarting.");
    current_mode = NORMAL;
  }
  last_toggle = ESP.getCycleCount();
}


void loop() {

  if(current_mode == REWINDING) {
      // we've reached the starting point. stop rewinding.
      if(totalsteps < 1 and autostop==true){
        Serial.println("Ending the rewind and stopping.");
        current_mode=STOPPED;
        all_pins_off();
        total_seconds = 0.0;
      }
  }
  else {
    // no-op. just wait for interrupts. 
    yield();  
  }
}




