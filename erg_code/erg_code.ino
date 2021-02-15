/*
 * Rowing machine interface.
 *
 * Requires ArduinoWebSockets and Adafruit_SSD1306 libraries.
 *
 * Websocket outputs:
 * - Workout time in usec
 * - Stroke time in usec
 * - Drag force
 * - Power
 * - Cadence
 *
 * Quadrature decoder
 * Timestampped velocity output
 * Strokes/minute computation
 */
#define CONFIG_LEDS
#define CONFIG_WEBSERVER

#include <stdint.h>
#include <stdarg.h>

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "DinBold8pt7b.h"
//#include "DinBold12pt7b.h"
#include "DinBold15pt7b.h"
#define FONT_MED &DinBold8pt7b
#define FONT_BIG &DinBold15pt7b

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     16 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LEFT	0x00
#define RIGHT	0x01
#define CENTER	0x03
#define BOTTOM	0x00
#define TOP	0x10
#define VCENTER	0x30

void drawtext(int x, int y, int center, const char * fmt, ...)
{
	char buf[40];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	int16_t x1, y1;
	uint16_t w, h;
	display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
	//printf("'%s' %d %d %d %d\r\n", buf, x1, y1, w, h);

	if ((center & 0x0F) == RIGHT)
		x = x - w;
	else
	if ((center & 0x0F) == CENTER)
		x = x - w/2;

	if ((center & 0xF0) == TOP)
		y = y + h;
	else
	if ((center & 0xF0) == VCENTER)
		y = y + h/2;

	display.setCursor(x, y);
	display.write(buf);
}

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
#define QUAD_B 13

#define LED_A 0
#define LED_B 2

#include "config.h"
static char device_id[16];

#ifdef CONFIG_WEBSERVER
ESP8266WebServer server(80);
#endif
WebSocketsServer webSocket = WebSocketsServer(81);

static const uint8_t webpage[] PROGMEM = {
#include "index.html.h"
};

void
webSocketEvent(
	uint8_t num,
	WStype_t type,
	uint8_t * payload,
	size_t length
)
{
	switch(type) {
        case WStype_DISCONNECTED:
		Serial.printf("[%u] Disconnected!\n", num);
		break;
        case WStype_CONNECTED:
		{
		IPAddress ip = webSocket.remoteIP(num);
		Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

		// send message to client
		webSocket.sendTXT(num, "Connected");
		}
		break;
        case WStype_TEXT:
		Serial.printf("[%u] get Text: %s\n", num, payload);
		break;
	}
}

void setup()
{
	Serial.begin(115200);
	pinMode(QUAD_A, INPUT_PULLUP);
	pinMode(QUAD_B, INPUT_PULLUP);
	//pinMode(QUAD_A, INPUT);
	//pinMode(QUAD_B, INPUT);

#ifdef CONFIG_LEDS
	pinMode(LED_A, OUTPUT);
	pinMode(LED_B, OUTPUT);
#endif

	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
		Serial.println(F("SSD1306 allocation failed"));
		for(;;); // Don't proceed, loop forever
	}

	display.clearDisplay();

	display.setRotation(2);	// flipped 180
	display.setTextSize(1);      // Normal 1:1 pixel scale
	display.setTextColor(SSD1306_WHITE); // Draw white text
	display.setCursor(0, 0);     // Start at top-left corner
	display.cp437(true);         // Use full 256 char 'Code Page 437' font


	uint8_t mac_bytes[6];
	WiFi.macAddress(mac_bytes);
	snprintf(device_id, sizeof(device_id), "%02x%02x%02x%02x%02x%02x",
		mac_bytes[0],
		mac_bytes[1],
		mac_bytes[2],
		mac_bytes[3],
		mac_bytes[4],
		mac_bytes[5]
	);

	Serial.println(String(device_id) + " connecting to " + String(WIFI_SSID));

	display.println(device_id);
	display.println(WIFI_SSID);
	display.display();

	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	while (WiFi.status() != WL_CONNECTED ) {
#ifdef CONFIG_LEDS
		digitalWrite(LED_A, 1);
		delay(250);
		digitalWrite(LED_A, 0);
#else
		delay(250);
#endif
		Serial.print(".");
		display.print(".");
		display.display();
	}

	Serial.println("WiFi connected\r\n");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	display.setCursor(0, 24);
	display.println(WiFi.localIP());
	display.display();

	// start webSocket server
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

#ifdef CONFIG_WEBSERVER
	server.on("/", []() {
        	// send index.html (should be gzip'ed, but :whatever:)
        	server.send(200, "text/html", webpage, sizeof(webpage));
	});

	server.begin();
#endif
}

int last_a;
int fall_a;
int rise_a;

int last_b;
int fall_b;
int rise_b;

unsigned long last_update;
int do_output;

unsigned start_usec;
unsigned stroke_power;
int delta_usec;
int last_delta_usec;
unsigned spm;
unsigned ticks;
unsigned last_ticks;

void loop()
{
	webSocket.loop();
#ifdef CONFIG_WEBSERVER
	server.handleClient();
#endif

	int got_tick = 0;
	const int a = digitalRead(QUAD_A);
	const int b = digitalRead(QUAD_B);
	const unsigned now = micros();

#ifdef CONFIG_LEDS
	digitalWrite(LED_A, a);
	digitalWrite(LED_B, !b);
#endif

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
	{
		if (now - last_update < 1000000)
			return;
		// it has been a second since our update, send a zero-force notice
		delta_usec = 0;
		spm = 0;
		stroke_power = 0;
	}

	// we have received a new data point,
	// process it and output the update on the serial port

	// need a scaling factor to compute how much
	// force they are applying.
	const int vel = delta_usec ? 100000000L / delta_usec : 0;
	const int force = vel * vel;

	// on positive velocity pulls, check for a sign change
	// for tracking power and distance
	if (delta_usec > 0)
	{
		// if the delta_usec is too fast, this is probably an error
		// and we should discard this point.100000000
		if (delta_usec < 2500)
			return;

		if (last_delta_usec <= 0)
		{
			// sign change: this is the start of a new stroke
			// compute spm * 10
			const unsigned stroke_delta = now - start_usec;
			spm = 600000000L / stroke_delta;
			
			start_usec = now;
			stroke_power = 0;
			last_ticks = ticks;
			ticks = 0;

			// blank line to mark the log
			Serial.println();

		}

		stroke_power += force;
		ticks++;
	}

	last_delta_usec = delta_usec;
	last_update = now;

	String msg = String("") + now + "," + (now - start_usec) + "," + delta_usec + "," + force + "," + stroke_power + "," + spm;
	Serial.println(msg);
	webSocket.broadcastTXT(msg);

	display.clearDisplay();
	display.setFont();
	display.setCursor(0,0);
	display.write("SPM");
	display.setCursor(110,0);
	display.write("POW");

	display.setFont(FONT_BIG);
	drawtext(38, 31, BOTTOM|RIGHT, "%d", spm / 10);
	display.setFont(FONT_MED);
	drawtext(43, 31, BOTTOM|LEFT, "%d", spm % 10);

	display.setFont(FONT_BIG);
	drawtext(123, 31, BOTTOM|RIGHT, "%d", stroke_power / 1000000);

	const int tick_scale = 5;
	display.fillRect(32, 0, ticks * tick_scale, 4, 1);
	//display.drawFastVLine(32 + last_ticks * tick_scale, 0, 8, 1);
	display.fillTriangle(
		32 + last_ticks * tick_scale, 4,
		32 + last_ticks * tick_scale-2, 8,
		32 + last_ticks * tick_scale+2, 8,
		1
	);

	display.display();
}
