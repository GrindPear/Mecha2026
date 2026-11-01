/* AUTO_CONTROL */
/* Usage: ./AUTO_CONTROL <reference [rev]> <max time [s]> <encoder counts per rev> <P> [I] [D] */
/* Ex)    ./AUTO_CONTROL 10 6 216 10          (P)   */
/*        ./AUTO_CONTROL 10 6 216 10 1 0.5    (PID) */

#include <stdio.h>
#include <stdlib.h>     // atof()
#include <string.h>
#include <signal.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <time.h>



#define LOOPTIME 10  // Sampling Time [ms]
#define ENCODERA 17 // Hall Sensor A
#define ENCODERB 27 // Hall Sensor B

#define MOTOR1 19   // Motor DI 1
#define MOTOR2 26   // Motor DI 2

#define NUM_ROWS 1100   // Size of array (row)
#define NUM_COLUMNS 3   // Size of array (col): time, position, target
#define DAQ_TIME 10000  // Data logging for 10s (max time must be <= this)

#define SHOW_LOG FALSE   // Show position in terminal
#define AUTO_NAME TRUE  // Name CSV file with current date/time

int encA;
int encB;
int encoderPosition = 0;    // Position of Encoder [count]
float redGearPosition = 0;  // Position of Motor [rev]

// Set by arguments
float referencePosition = 0;    // [rev]
unsigned int maxTime = 0;       // Auto exit time [ms]
int encPerRev = 0;              // Encoder counts per 1 rev of output shaft
float pGain = 0;                // P gain
float iGain = 0;                // I gain (optional, 0 if not given)
float dGain = 0;                // D gain (optional, 0 if not given)

float errorPosition = 0;
float controlInput = 0;     // Output of controller (PWM duty)

unsigned int checkTime;         // [us]
unsigned int checkTimeBefore;   // [us]
unsigned int startTime;         // [us]

int dataIndex = 0;
float dataArray[NUM_ROWS][NUM_COLUMNS]; // 2D array (rows x cols)
FILE* file;
char name[80];          // File name without extension
char filename[100];     // CSV file name

void handle_sigint(int sig) {   // Ctrl+C or max time: stop motor, save CSV and exit
    softPwmWrite(MOTOR1, 0);
    softPwmWrite(MOTOR2, 0);

    fprintf(file, "Time (s), Position (rev), Target (rev)\n");   // Header (first row)
    for (int i = 0; i < dataIndex; i++)
    {
        fprintf(file, "%.4f, %.3f, %.3f\n", dataArray[i][0], dataArray[i][1], dataArray[i][2]);
    }
    fclose(file);
    printf("CSV file created: %s\n", filename);   // Last line: Python reads the file name

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
    redGearPosition = (float)encoderPosition / encPerRev;
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
    redGearPosition = (float)encoderPosition / encPerRev;
    errorPosition = referencePosition - redGearPosition;
}

void positionControl(float error)   // Position controller for motor PWM
{
    controlInput = pGain * error;   // P control ONLY

    if (controlInput > 0) {         // Direction by sign (+: MOTOR1, -: MOTOR2)
        softPwmWrite(MOTOR1, controlInput);
        softPwmWrite(MOTOR2, 0);
    }
    else {
        softPwmWrite(MOTOR2, -controlInput);
        softPwmWrite(MOTOR1, 0);
    }
}

int main(int argc, char *argv[])
// argc is number of arguments (+1 for program name)
// argv is array of arguments (CHAR)
{
    int numGains = argc - 4;    // 1: P, 2: PI, 3: PID

    if (numGains < 1 || numGains > 3) {
        // Exception
        printf("Arguments must be 4 to 6\n");
        printf("First arg: reference position (rev)\n");
        printf("Second arg: max time (sec, <= %d)\n", DAQ_TIME / 1000);
        printf("Third arg: encoder counts per 1 rev\n");
        printf("Fourth ~ sixth arg: P gain, I gain (optional), D gain (optional)\n");
        printf("Ex) %s 10 6 216 10          (P)\n", argv[0]);     // argv[0]: program name
        printf("    %s 10 6 216 10 1 0.5    (PID)\n", argv[0]);
        return 1;
    }

    referencePosition = atof(argv[1]);      // atof: char* -> float
    maxTime = atof(argv[2]) * 1000;         // [s] -> [ms]
    encPerRev = atoi(argv[3]);              // atoi: char* -> int
    pGain = atof(argv[4]);
    if (numGains >= 2) iGain = atof(argv[5]);
    if (numGains >= 3) dGain = atof(argv[6]);

    if (maxTime > DAQ_TIME) {
        printf("Max time must be <= %d sec\n", DAQ_TIME / 1000);
        return 1;
    }
    if (encPerRev <= 0) {
        printf("Encoder counts must be > 0\n");
        return 1;
    }
    printf("Reference: %.2f rev, Max time: %.2f s, Encoder: %d, Gain: P %g, I %g, D %g\n",
           referencePosition, maxTime / 1000.0, encPerRev, pGain, iGain, dGain);

    if (AUTO_NAME) {    // Date_time + gains, ex) 20261101_204818_P10_I0.5
        char timeText[20];
        time_t now = time(NULL);
        strftime(timeText, sizeof(timeText), "%Y%m%d_%H%M%S", localtime(&now));

        // %g prints numbers without extra zeros (10, 0.5)
        if (numGains == 1) sprintf(name, "%s_P%g", timeText, pGain);
        else if (numGains == 2) sprintf(name, "%s_P%g_I%g", timeText, pGain, iGain);
        else sprintf(name, "%s_P%g_I%g_D%g", timeText, pGain, iGain, dGain);
    }
    else {
        printf("Enter the file name: ");
        scanf("%s", name);
    }
    sprintf(filename, "%s.csv", name);   // Add extension
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

            if (SHOW_LOG) {
                printf("gearPos: %f errorPos: %f\n", redGearPosition, errorPosition);
            }

            updateDataArray();

            checkTimeBefore += LOOPTIME * 1000;     // Keep fixed sampling period

            if (checkTime - startTime >= maxTime * 1000)  // Time over
            {
                raise(SIGINT);  // Call handle_sigint (stop motor, save CSV, exit)
            }
        }
    }
    return 0;
}
