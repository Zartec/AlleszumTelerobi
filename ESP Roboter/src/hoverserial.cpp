// *******************************************************************
//  Arduino Nano 5V example code
//  for   https://github.com/EmanuelFeru/hoverboard-firmware-hack-FOC
//
//  Copyright (C) 2019-2020 Emanuel FERU <aerdronix@gmail.com>
//
// *******************************************************************
// INFO:
// • This sketch uses the the Serial Software interface to communicate and send commands to the hoverboard
// • The built-in (HW) Serial interface is used for debugging and visualization. In case the debugging is not needed,
//   it is recommended to use the built-in Serial interface for full speed perfomace.
// • The data packaging includes a Start Frame, checksum, and re-syncronization capability for reliable communication
//
// The code starts with zero speed and moves towards +
//
// CONFIGURATION on the hoverboard side in config.h:
// • Option 1: Serial on Right Sensor cable (short wired cable) - recommended, since the USART3 pins are 5V tolerant.
//   #define CONTROL_SERIAL_USART3
//   #define FEEDBACK_SERIAL_USART3
//   // #define DEBUG_SERIAL_USART3
// • Option 2: Serial on Left Sensor cable (long wired cable) - use only with 3.3V devices! The USART2 pins are not 5V tolerant!
//   #define CONTROL_SERIAL_USART2
//   #define FEEDBACK_SERIAL_USART2
//   // #define DEBUG_SERIAL_USART2
// *******************************************************************

// ########################## DEFINES ##########################
#define HOVER_SERIAL_BAUD 115200 // [-] Baud rate for HoverSerial (used to communicate with the hoverboard)
#define RXPIN 23                 // GPIO 4 => RX for Serial1
#define TXPIN 19                 // GPIO 5 => TX for Serial1
#define START_FRAME 0xABCD       // [-] Start frme definition for reliable serial communication
#define TIME_SEND 100            // [ms] Sending time interval
#define SPEED_MAX_TEST 1000      // [-] Maximum speed for testing
#define SPEED_STEP 40            // [-] Speed step

// #define DEBUG_RX                        // [-] Debug received data. Prints all bytes to serial (comment-out to disable)

#include "hoverserial.h"
#include <Arduino.h>

// Global variables
uint8_t idx = 0;        // Index for new data pointer
uint16_t bufStartFrame; // Buffer Start Frame
byte *p;                // Pointer declaration for the new received data
byte incomingByte;
byte incomingBytePrev;
bool shutdownTriggered = false;

typedef struct
{
    uint16_t start;
    int16_t steer;
    int16_t speed;
    uint16_t checksum;
} SerialCommand;
SerialCommand Command;
SerialFeedback Feedback;
SerialFeedback NewFeedback;

// ########################## SETUP ##########################
void serialSetup()
{
    // Serial.println("Hoverboard Serial v1.0");
    Serial1.begin(HOVER_SERIAL_BAUD, SERIAL_8N1, RXPIN, TXPIN);
}

// ########################## SEND ##########################
void Send(int16_t uSteer, int16_t uSpeed)
{
    // Create command
    Command.start = (uint16_t)START_FRAME;
    Command.steer = (int16_t)uSteer;
    Command.speed = (int16_t)uSpeed;
    Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);

    // Write to Serial
    Serial1.write((uint8_t *)&Command, sizeof(Command));
}

void sendShutdown()
{
    if (!shutdownTriggered)
    {
        Command.start = (uint16_t)0xCAFE;
        Command.steer = (int16_t)0;
        Command.speed = (int16_t)0;
        Command.checksum = (uint16_t)(Command.start ^ Command.steer ^ Command.speed);
        Serial1.write((uint8_t *)&Command, sizeof(Command));
        shutdownTriggered = true;
    }
}

// ########################## RECEIVE ##########################
void Receive()
{
    // Check for new data availability in the Serial buffer
    if (Serial1.available())
    {
        incomingByte = Serial1.read();                                      // Read the incoming byte
        bufStartFrame = ((uint16_t)(incomingByte) << 8) | incomingBytePrev; // Construct the start frame
    }
    else
    {
        return;
    }

// If DEBUG_RX is defined print all incoming bytes
#ifdef DEBUG_RX
    Serial.print(incomingByte);
    return;
#endif

    // Copy received data
    if (bufStartFrame == START_FRAME)
    { // Initialize if new data is detected
        p = (byte *)&NewFeedback;
        *p++ = incomingBytePrev;
        *p++ = incomingByte;
        idx = 2;
    }
    else if (idx >= 2 && idx < sizeof(SerialFeedback))
    { // Save the new received data
        *p++ = incomingByte;
        idx++;
    }

    // Check if we reached the end of the package
    if (idx == sizeof(SerialFeedback))
    {
        uint16_t checksum;
        checksum = (uint16_t)(NewFeedback.start ^ NewFeedback.cmd1 ^ NewFeedback.cmd2 ^ NewFeedback.speedR_meas ^ NewFeedback.speedL_meas ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

        // Check validity of the new data
        if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum)
        {
            // Copy the new data
            memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));

            // Print data to built-in Serial
            Serial.print("1: ");
            Serial.print(Feedback.cmd1);
            Serial.print(" 2: ");
            Serial.print(Feedback.cmd2);
            Serial.print(" 3: ");
            Serial.print(Feedback.speedR_meas);
            Serial.print(" 4: ");
            Serial.print(Feedback.speedL_meas);
            Serial.print(" 5: ");
            Serial.print(Feedback.batVoltage);
            Serial.print(" 6: ");
            Serial.print(Feedback.boardTemp);
            Serial.print(" 7: ");
            Serial.println(Feedback.cmdLed);
        }
        else
        {
            Serial.println("Non-valid data skipped");
        }
        idx = 0; // Reset the index (it prevents to enter in this if condition in the next cycle)
    }

    // Update previous states
    incomingBytePrev = incomingByte;
}

// ########################## LOOP ##########################

unsigned long iTimeSend = 0;
int iTest = 0;
int iStep = SPEED_STEP;

void serialLoop(void)
{
    unsigned long timeNow = millis();

    // Check for new received data
    Receive();

    // Send commands
    if (iTimeSend > timeNow)
        return;
    iTimeSend = timeNow + TIME_SEND;
    Send(0, iTest);

    // Calculate test command signal
    iTest += iStep;

    // invert step if reaching limit
    if (iTest >= SPEED_MAX_TEST || iTest <= -SPEED_MAX_TEST)
        iStep = -iStep;

    // Blink the LED
    // digitalWrite(LED_BUILTIN, (timeNow % 2000) < 1000);
}

// ########################## END ##########################
