
#include <serialize.h>
#include <stdarg.h>
#include <math.h>
#include "packet.h"
#include "constants.h"

/* Alex's State Variables & configuration constants */
#define ALEX_LENGTH         26.3
#define ALEX_BREADTH        15.5
#define Pi                  3.141592654 
#define COUNTS_PER_REV      4           // Number of ticks per revolution from the wheel encoder
#define PIN18 (1 << PD3)
#define PIN19 (1 << PD2)
#define WHEEL_CIRC          20.4 // in CM We will use this to calculate forward/backward distance traveled by taking revs * WHEEL_CIRC
float alexDiagonal = 0.0;
float alexCirc = 0.0;
/* Timer Variables */
#define COL_FREQ (1 << PL1) //pin 48 ICP 4
#define S2 (1 << PL2) // pin 47
#define S3 (1 << PG2) // pin 39
#define S1 (1 << PL6) //pin 43
#define S0 (1 << PL7) //pin 42
volatile long period = 0;
volatile uint8_t rising_edge_count = 0;
long avg_period = 0;
float red_frequency = 0;
float green_frequency = 0;
volatile bool done;
volatile bool off;
/* Alex's Arm Variables */

/* Distance & Motion Variable */
volatile unsigned long leftForwardTicks; // Store the forward ticks from Alex's left and right encoder
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; // Store the reverse ticks from Alex's left and right encoder
volatile unsigned long rightReverseTicks;
volatile unsigned long leftForwardTicksTurns; //  Store the reverse turns from Alex's left and right encoder
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns;
volatile unsigned long rightReverseTicksTurns;
volatile unsigned long forwardDist; //store the forward and reverse ticks
volatile unsigned long reverseDist; 
unsigned long deltaDist;
unsigned long newDist;
unsigned long deltaTicks;
unsigned long targetTicks;
unsigned long computeDeltaTicks(float ang){
  unsigned long ticks = (unsigned long) ((ang * alexCirc * COUNTS_PER_REV) / (360.0 * WHEEL_CIRC));
  return ticks; }
void left(float ang, float speed) {
  if(ang == 0)      
  deltaTicks=99999999;
  else 
    deltaTicks= computeDeltaTicks(ang); 
   
  targetTicks = leftReverseTicksTurns + deltaTicks; 

  ccw(ang, speed);
}

void right(float ang, float speed) {
  if(ang == 0)      
  deltaTicks=99999999;
  else 
    deltaTicks= computeDeltaTicks(ang); 
   
  targetTicks = rightReverseTicksTurns + deltaTicks; 
 cw(ang, speed);
}
volatile TDirection dir;

/* Alex Communication Routines. */
TResult readPacket(TPacket *packet) // Reads in data from the serial port and deserializes it.Returns deserialized data in "packet".
{   char buffer[PACKET_SIZE];
    int len;
    len = readSerial(buffer);
    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
}

void sendStatus() // send back a packet containing key information
{
  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;
  statusPacket.params[0] = leftForwardTicks;  // Use the params array to store this information
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;
  sendResponse(&statusPacket);
}

void sendMessage(const char *message)  // Sends text messages back to the Pi
{ TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket); }

void dbprintf(const char *format, ...) 
{    va_list args;
     char buffer[128];
     va_start(args, format);
     vsprintf(buffer, format, args);
     sendMessage(buffer); }

void sendBadPacket()
{ TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);}

void sendBadChecksum()
{ TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);   }

void sendBadCommand()  // Tell the Pi that we don't understand
{  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand); }

void sendBadResponse()
{ TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse); }

void sendOK()
{ TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  }

void sendResponse(TPacket *packet)  // Takes a packet, serializes it then sends it out
{ char buffer[PACKET_SIZE];
  int len;
  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len); }

/* Setup and start codes for external interrupts */

void enablePullups() // Enable pull up resistors on pins 18 and 19
{ DDRD &= ~(PIN18 | PIN19);  //sets bits 2&3 in DDRD to input
  PORTD |= (PIN18 | PIN19); // Enables Pullup resistors 
}

void leftISR() // Functions to be called by INT2 and INT3 ISRs.
{ if (dir == FORWARD){
    leftForwardTicks++;
  } else if ( dir == BACKWARD){
    leftReverseTicks++;
  } else if (dir == LEFT){
    leftReverseTicksTurns++;
  } else if (dir == RIGHT){
    leftForwardTicksTurns++;}
}

void rightISR()
{ if (dir == FORWARD){
    rightForwardTicks++;
    forwardDist = (unsigned long) ((float) rightForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
  } else if ( dir == BACKWARD){
    rightReverseTicks++;
    reverseDist = (unsigned long) ((float) rightReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
  } else if (dir == LEFT){
    rightForwardTicksTurns++;
  } else if (dir == RIGHT){
    rightReverseTicksTurns++;}
} 

void setupEINT() // Set up the external interrupt pins INT2 and INT3 for falling edge triggered.
{ EICRA |= 0b10100000; //sets both INT2 and INT3 trigger on the falling ed
  EIMSK |= 0b00001100; //enables INT2 and INT3
}

ISR(INT2_vect) // INT3 ISR should call leftISR while INT2 ISR
{rightISR();}
ISR(INT3_vect)
{leftISR();}

void setup_OCR_and_DDR()
{DDRL |= (S1 | S2 | S0); // setting PIN 47, 45, 43, 42 to output
 DDRG |= S3; //white wire
 DDRL &= ~COL_FREQ;  // set ICP pin to input
 DDRL |= (1 << PL3) | (1 << PL4) | (1 << PL5); // OC5A/B/C outputs
 OCR5A = 2000; // 1.5 ms pulse → center
 OCR5B = 2000; // 1.0 ms pulse → left
 OCR5C = 4000; // 2.0 ms pulse → right
}

void resetTimer5()
{
  TCCR5A = 0;
  TCCR5B = 0;
}
void setupTimer5_Colour()
{
  cli();
  resetTimer5();
  TCCR5B = (1 << ICNC5) | (1 << ICES5) | (1 << CS50); // Rising edge, no prescaler
  ICR5 = 0;
  TIMSK5 &= ~(1 << ICIE5);
  off = true;
  sei();
}

void setupTimer5_Arm() {
  cli();
  resetTimer5();
  TCCR5A = (1 << COM5A1) | (1 << COM5B1) | (1 << COM5C1) | (1 << WGM51);
  TCCR5B = (1 << WGM53) | (1 << WGM52) | (1 << CS51); // Prescaler = 8
  ICR5 = 40000; // 20 ms period
  off = false;
  sei();
}

void colour()
{ PORTL &= ~S2;
  PORTG &= ~S3; // starts red photodiode
  detect_frequency('R');
  char numBuffer[16];
dtostrf(red_frequency, 6, 2, numBuffer);  // width=6, precision=2
dbprintf("red is %s", numBuffer);
  PORTL |= S2;
  PORTG |= S3;
  detect_frequency('G');
dtostrf(green_frequency, 6, 2, numBuffer);  // width=6, precision=2
dbprintf("green is %s", numBuffer);

  if (red_frequency > green_frequency)
  { sendMessage("red"); }
  else { sendMessage("green"); }
}

void detect_frequency(char col)
{
  PORTL |= (S1 | S0); // sets it to 100% mode
  TIMSK5 |= (1 << ICIE5) ;
  wait_for_period();
  TIMSK5 &= ~(1 << ICIE5);
  avg_period =  period/6;
  if(col == 'R' ) {
    red_frequency = 16000000.0 / (float) avg_period;
  } else {
    if(col == 'G'){ green_frequency = 16000000.0 / (float) avg_period; }
}}

void wait_for_period(){
  done = false;
  TCNT5 = 0;
  rising_edge_count = 0;
  while (!done);
}

ISR(TIMER5_CAPT_vect){
  if (rising_edge_count == 0)
  {
   TCNT5 = 0; //reset count start of first period
  }
  if (rising_edge_count == 2){
    period = ICR5; // count for 3 periods
    TCNT5 = 0; // reset count
  }

  if (rising_edge_count > 2 && rising_edge_count < 6)
  {
    period += ICR5;
    TCNT5 = 0;
  }
  if (rising_edge_count == 6){
    period += ICR5; // add count for 3 more periods
    done = true; //donege
  }
    rising_edge_count++;
  } 

void smoothMove(volatile uint16_t &ocr, uint16_t target, uint8_t step, uint16_t delayTime) {
  if (ocr < target) {
    for (uint16_t val = ocr; val <= target; val += step) {
      ocr = val;
      delay(delayTime);
    }
  } else {
    for (uint16_t val = ocr; val >= target; val -= step) {
      ocr = val;
      delay(delayTime);
      if (val < step) break; // avoid underflow
    }
  }
}

void close_claw() {
  smoothMove(OCR5A, 2500, 10, 5); // Mid
  smoothMove(OCR5B, 3300, 10, 5);

  // TODO: Move start moving outer claw once inner claw reaches 3800;
}

void open_claw(){ 
  OCR5B = 1000;
  delay(1000);
  OCR5A = 5000;
}

void dispense_medpack(){ // DONE
  OCR5C = 1000;
  delay(3000);
  OCR5C = 4000;
}
 /* Setup and start codes for serial communications */

void setupSerial()                      
{                             // Set up the serial connection. For now we are using 
  Serial.begin(9600);         // Arduino Wiring, you will replace this later with bare-metal code.  
  // Change Serial to Serial2/Serial3/Serial4 in later labs when using the other UARTs
}

void startSerial()            // Start the serial connection. For now we are using
{                             // Arduino wiring and this function is empty. We will
}

int readSerial(char *buffer) // Read the serial port. Returns the read character in ch
{                            // ch if available. Also returns TRUE if ch is valid. 
  int count=0;               //replace with BARE-METAL CODEE LATER ON
  while(Serial.available())  // Change Serial to Serial2/Serial3/Serial4 in later labs when using other UARTs
    buffer[count++] = Serial.read();
  return count;
}

void writeSerial(char *buffer, int len) // Write to the serial port. Replaced later with Bare-Metal Code
{
  Serial.write(buffer, len);          // Change Serial to Serial2/Serial3/Serial4 in later labs when using other UARTs
}

/*  Alex's setup and run codes */
void clearCounters() // Clears all our counters
{
  leftForwardTicks=0; 
  rightForwardTicks=0;
  leftReverseTicks=0;
  rightReverseTicks=0;
  leftForwardTicksTurns=0;
  rightForwardTicksTurns=0;
  leftReverseTicksTurns=0;
  rightReverseTicksTurns=0;
  forwardDist=0;
  reverseDist=0;
}

void clearOneCounter(int which) // Clears one particular counter
{
  clearCounters();
  return;
  /*
  switch(which)
  {
    case 0:
      clearCounters();
      break;

    case 1:
      leftTicks=0;
      break;

    case 2:
      rightTicks=0;
      break;

    case 3:
      leftRevs=0;
      break;

    case 4:
      rightRevs=0;
      break;

    case 5:
      forwardDist=0;
      break;

    case 6:
      reverseDist=0;
      break;
  }*/
}
// Intialize Alex's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        forward((float) command->params[0], (float) command->params[1]);
        sendOK();
      break;

    case COMMAND_REVERSE:
        backward((float) command->params[0], (float) command->params[1]);
        sendOK();
      break;

    case COMMAND_TURN_LEFT:
        left((float) command->params[0], (float) command->params[1]);
        sendOK();
      break;

    case COMMAND_TURN_RIGHT:
        right((float) command->params[0], (float) command->params[1]);
        sendOK();
      break;

   case COMMAND_STOP:
        sendOK();
        stop();
      break;

    case COMMAND_GET_STATS:
        sendStatus();

      break;
      
    case COMMAND_CLEAR_STATS:
        sendOK();
        clearOneCounter(command->params[0]);
      break;
      
    case COMMAND_GET_COLOUR: 
        setupTimer5_Colour();
        colour();
        sendOK();
      break;

    case COMMAND_CLOSE_CLAW:
       setupTimer5_Arm();
       close_claw();
      break;

    case COMMAND_OPEN_CLAW:
       setupTimer5_Arm();
        open_claw();
        break;

    case COMMAND_DISPENSE_MEDPACK:
       setupTimer5_Arm();
        dispense_medpack();
        break;

    case COMMAND_KEEP_GOING_FORWARD:
      forward1(50, 100);
      break;
      
    case COMMAND_KEEP_GOING_BACKWARD:
      backward1(50, 100);
      break;
    case COMMAND_KEEP_GOING_RIGHT:
      right1(20, 60);
      break;
    case COMMAND_KEEP_GOING_LEFT:
      left1(20, 60);
      break;

      
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1; 
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:
     alexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH)); 
 
     alexCirc = Pi  * alexDiagonal; 

  cli();
  setupEINT();
  setup_OCR_and_DDR();
  setupSerial();
  startSerial();
  enablePullups();
  initializeState();
  sei();
  waitForHello();
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}

void loop() {
 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);
  
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else if(result == PACKET_CHECKSUM_BAD)
    {
      sendBadChecksum();
      while (Serial.available()) {
        Serial.read();
      }
      sendMessage("fixedchecksum?");
    } 
      
if(deltaDist > 0) 
   { 
    if(dir==FORWARD) 
     { 
        if(forwardDist > newDist) 
        { 
          deltaDist=0;
          newDist=0; 
          stop(); 
        } 
      } 
    else if(dir == BACKWARD) 
    { 
      if(reverseDist > newDist) 
      { 
        deltaDist=0;            
        newDist=0;            
        stop(); 
      } 
    } 
    else
    { 
      deltaDist=0;
      newDist=0; 
      stop(); 
    } 
  }
   if(deltaTicks > 0)
   {
    if(dir == LEFT && leftReverseTicksTurns >= targetTicks)
    {
      deltaTicks=0;
      targetTicks=0;
      stop();
    }
    else if(dir == RIGHT && rightReverseTicksTurns >= targetTicks)
    {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
    }
    else
    {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
    }
   } 
   
 }
  
