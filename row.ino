/*
 * Rowing machine interface.
 *
 * Quadrature decoder
 * Timestampped velocity output
 * Strokes/minute computation
 * BLE "Fitness Machine Service" (FMTS) interface
 */

/*
 * Quadrature modes:
 * Input Pullup, pulled low when the magnet passes.
 * Time stamp on first falling edge
 * Time stamp on second falling edge
 * No second falling edge? 
 * Reversing on the same edge?
 * Weird blip in the center of the window.
 */
#define QUAD_A 14
#define QUAD_B 12

#define LED_A 0
#define LED_B 2


void setup()
{
	Serial.begin(115200);
	//pinMode(QUAD_A, INPUT_PULLUP);
	//pinMode(QUAD_B, INPUT_PULLUP);
	pinMode(QUAD_A, INPUT);
	pinMode(QUAD_B, INPUT);

	pinMode(LED_A, OUTPUT);
	pinMode(LED_B, OUTPUT);
}

int last_a;
int fall_a;
int rise_a;

int last_b;
int fall_b;
int rise_b;

int do_output;

void loop()
{
	const int a = digitalRead(QUAD_A);
	const int b = digitalRead(QUAD_B);
	const unsigned now = micros();

	digitalWrite(LED_A, a);
	digitalWrite(LED_B, !b);

	if (a && b)
	{
		// back to idle, so prep for the next output
		do_output = 1;
	}

	if (!a && last_a)
	{
		if (now - rise_a > 20000)
			fall_a = now;

/*
		Serial.print(now);
		Serial.println(" A-");
*/

		if (!b && do_output)
		{
			// b is already low, so this is a negative speed
			do_output = 0;
			Serial.print(now);
			Serial.print(" ");
			Serial.println((int)(fall_b - now));
		}
	}
			
	if (a && !last_a)
	{
		rise_a = now;
/*
		Serial.print(now);
		Serial.println(" A+");
*/
	}

	if (!b && last_b)
	{
		if (now - rise_b > 20000)
			fall_b = now;
/*
		Serial.print(now);
		Serial.println(" B-");
*/

		if (!a && do_output)
		{
			// a is already low, so this is a positive speed
			do_output = 0;
			digitalWrite(LED_B, 1);
			Serial.print(now);
			Serial.print(" ");
			Serial.println((int)(now - fall_a));
		}
	}

	if (b && !last_b)
	{
		rise_b = now;
/*
		Serial.print(now);
		Serial.println(" B-");
*/
	}

	last_a = a;
	last_b = b;
}
