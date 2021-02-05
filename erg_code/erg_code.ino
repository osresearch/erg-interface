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
#define QUAD_A 12
#define QUAD_B 14

#define LED_A 0
#define LED_B 2


void setup()
{
	Serial.begin(115200);
	pinMode(QUAD_A, INPUT_PULLUP);
	pinMode(QUAD_B, INPUT_PULLUP);
	//pinMode(QUAD_A, INPUT);
	//pinMode(QUAD_B, INPUT);

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

unsigned start_usec;
unsigned stroke_power;
int delta_usec;
int last_delta_usec;
unsigned spm;

void loop()
{
	int got_tick = 0;
	const int a = digitalRead(QUAD_A);
	const int b = digitalRead(QUAD_B);
	const unsigned now = micros();

	digitalWrite(LED_A, a);
	digitalWrite(LED_B, !b);

	if (a && b)
	{
		// back to idle, so prep for the next tick output
		do_output = 1;
	}

	if (!a && last_a)
	{
		if (now - rise_a > 20000)
			fall_a = now;

		if (!b && do_output)
		{
			// b is already low, so this is a negative speed
			do_output = 0;
			got_tick = 1;
			delta_usec = fall_b - now;
		}
	}
			
	if (a && !last_a)
	{
		rise_a = now;
	}

	if (!b && last_b)
	{
		if (now - rise_b > 20000)
			fall_b = now;

		if (!a && do_output)
		{
			// a is already low, so this is a positive speed
			do_output = 0;
			got_tick = 1;
			delta_usec = now - fall_a;
		}
	}

	if (b && !last_b)
	{
		rise_b = now;
	}

	last_a = a;
	last_b = b;

	if (!got_tick)
		return;

	// we have received a new data point,
	// process it and output the update on the serial port

	// need a scaling factor to compute how much
	// force they are applying.
	const int force = 1000000000L / delta_usec;

	// on positive velocity pulls, check for a sign change
	// for tracking power and distance
	if (delta_usec > 0)
	{
		if (last_delta_usec < 0)
		{
			// sign change: this is the start of a new stroke
			const unsigned stroke_delta = now - start_usec;
			spm = (600000000L / stroke_delta + spm * 3) / 4;
			
			start_usec = now;
			stroke_power = 0;

			// blank line to mark the log
			Serial.println();
		}

		stroke_power += force;
	}

	last_delta_usec = delta_usec;

	Serial.print(now);
	Serial.print(" ");
	Serial.print(now - start_usec);
	Serial.print(" ");
	Serial.print(force);
	Serial.print(" ");
	Serial.print(delta_usec > 0 ? stroke_power : 0);
	Serial.print(" ");
	Serial.print(spm);
	Serial.println();
}
