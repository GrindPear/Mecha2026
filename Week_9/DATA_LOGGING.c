/* DATA_LOGGING */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <time.h>

#define LOOPTIME 10  // Sampling Time [ms]
#define ENCODERA 17 // Hall Sensor A
#define ENCODERB 27 // Hall Sensor B
#define ENC2REDGEAR 216 // Encoder counts per 1 rev of output shaft

#define MOTOR1 19   // Motor DI 1
#define MOTOR2 26   // Motor DI 2
#define PGAIN 10    // P gain

#define NUM_ROWS 1100   // Size of array (row)
#define NUM_COLUMNS 3   // Size of array (col): time, position, target
#define DAQ_TIME 10000  // Data logging for 10s

int encA; 
int encB;   
int encoderPosition = 0;    // Position of Encoder [count]
float redGearPosition = 0;  // Position of Motor [rev]

float referencePosition = 10;
float errorPosition = 0;
float controlInput = 0;     // Output of controller (PWM duty)

unsigned int checkTime;         // [us]
unsigned int checkTimeBefore;   // [us]
unsigned int startTime;         // [us]

int dataIndex = 0;
float dataArray[NUM_ROWS][NUM_COLUMNS]; // 2D array (rows x cols)
FILE* file;

void handle_sigint(int sig) {   // Ctrl+C: stop motor, save CSV and exit
    printf("\nEXIT PROGRAM\n");
    softPwmWrite(MOTOR1, 0);
    softPwmWrite(MOTOR2, 0);

    fprintf(file, "Time (s), Position (rev), Target (rev)\n");   // Header (first row)
    for (int i = 0; i < dataIndex; i++)
    {
        fprintf(file, "%.4f, %.3f, %.3f\n", dataArray[i][0], dataArray[i][1], dataArray[i][2]);
    }
    fclose(file);

    delay(100);    // Wait for softPwm threads to set pins LOW before exit
    exit(0);
}

void updateDataArray()  // Save time [s], position [rev] and target [rev]
{
    if (dataIndex >= NUM_ROWS) return;
    if (checkTime - startTime > DAQ_TIME * 1000) return;

    dataArray[dataIndex][0] = (float)(checkTime - startTime) / 1000000.0;
    dataArray[dataIndex][1] = redGearPosition;
    dataArray[dataIndex][2] = referencePosition;
    dataIndex++;
}

void funcEncoderA()     // Interrupt: edge on encoder A
{
    encA = digitalRead(ENCODERA);
    encB = digitalRead(ENCODERB);
    if (encA == HIGH)   // Rising at A
    {
        if (encB == LOW) encoderPosition++;
        else encoderPosition--;
    }
    else                // Falling at A
    {
        if (encB == LOW) encoderPosition--;
        else encoderPosition++;
    }
    redGearPosition = (float)encoderPosition / ENC2REDGEAR;
    errorPosition = referencePosition - redGearPosition;
}

void funcEncoderB()     // Interrupt: edge on encoder B
{
    encA = digitalRead(ENCODERA);
    encB = digitalRead(ENCODERB);
    if (encB == HIGH)   // Rising at B
    {
        if (encA == LOW) encoderPosition--;
        else encoderPosition++;
    }
    else                // Falling at B
    {
        if (encA == LOW) encoderPosition++;
        else encoderPosition--;
    }
    redGearPosition = (float)encoderPosition / ENC2REDGEAR;
    errorPosition = referencePosition - redGearPosition;
}

void positionControl(float error)   // Position controller for motor PWM
{
    controlInput = PGAIN * error;   // P control ONLY

    if (controlInput > 0) {         // Direction by sign (+: MOTOR1, -: MOTOR2)
        softPwmWrite(MOTOR1, controlInput);
        softPwmWrite(MOTOR2, 0);
    }
    else {
        softPwmWrite(MOTOR2, -controlInput);
        softPwmWrite(MOTOR1, 0);
    }
}

int main(void)
{   
    char filename[100]; 

    printf("Enter the CSV file name: ");
    scanf("%s", filename);
    strcat(filename, ".csv");
    file = fopen(filename, "w+");

    signal(SIGINT, handle_sigint);
    wiringPiSetupGpio();
    pinMode(ENCODERA, INPUT);
    pinMode(ENCODERB, INPUT);

    softPwmCreate(MOTOR1, 0, 100);  // Create soft Pwm
    softPwmCreate(MOTOR2, 0, 100);  // Create soft Pwm

    // Call encoder functions on every edge of A and B
    wiringPiISR(ENCODERA, INT_EDGE_BOTH, funcEncoderA);
    wiringPiISR(ENCODERB, INT_EDGE_BOTH, funcEncoderB);

    errorPosition = referencePosition - redGearPosition;
    checkTimeBefore = micros();
    startTime = micros();

    while(1)
    {
        checkTime = micros();
        if (checkTime - checkTimeBefore >= LOOPTIME * 1000)  // Run every LOOPTIME
        {
            positionControl(errorPosition);

            printf("gearPos: %f errorPos: %f\n", redGearPosition, errorPosition);
            updateDataArray();

            checkTimeBefore += LOOPTIME * 1000;     // Keep fixed sampling period
        }
    }
    return 0;
}
