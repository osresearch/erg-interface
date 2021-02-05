const int RED_LED = 14;
const int GREEN_LED = 12; 
//const int DRIVE_PIN = ; // lights first during the drive
//const int RECOVERY_PIN = ; // lights first during the recovery

void setup() {
	// put your setup code here, to run once:
	pinMode(RED_LED, OUTPUT);	
	pinMode(GREEN_LED, OUTPUT);	
}

void loop() {
	// put your main code here, to run repeatedly:
	digitalWrite(RED_LED, HIGH);
	delay(300);
	digitalWrite(GREEN_LED, HIGH);
	delay(1200);
	digitalWrite(RED_LED, LOW);
	delay(300);
	digitalWrite(GREEN_LED, LOW);
	delay(600);
}
