/* Argument EXAMPLE*/

#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>	// softPwm header
#include <stdlib.h>		// atoi(), atof()

#define LED 18

int main(int argc, char *argv[])
// argc is number of arguments
// argv is array of arguments (CHAR)
{
	wiringPiSetupGpio();
	softPwmCreate(LED, 0, 100);
	
	if (argc == 2) {
	// If input argument is 1, just LED ON
		int bright = atoi(argv[1]);
		// atoi: char* -> int

		softPwmWrite(LED, bright);
		while(1) {
			delay(1000);
		}
	}
	else if (argc == 3) {
	// If input argument is 2, LED blinks
		while(1)
		{
			int bright = atoi(argv[1]);
			int period = (int) (atof(argv[2]) * 1000);
			// atof: char* -> float

			softPwmWrite(LED, bright);
			delay(period);

			softPwmWrite(LED, 0);
			delay(period);
		}
	}
	else {
		// Exception
		printf("Arguments must be 1 or 2\n");
		printf("First arg: bright (0-100)\n");
		printf("Second arg: blink period (sec)\n");
	}

	return 0;
}