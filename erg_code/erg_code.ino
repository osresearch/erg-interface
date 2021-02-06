/*
 * Rowing machine interface.
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
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>


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

#include "config.h"
static char device_id[16];

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

static const uint8_t webpage[] = {
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

	pinMode(LED_A, OUTPUT);
	pinMode(LED_B, OUTPUT);

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
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print(".");

	while (WiFi.status() != WL_CONNECTED ) {
		digitalWrite(LED_A, 1);
		delay(100);
		digitalWrite(LED_A, 0);
		Serial.print(".");
	}

	Serial.println("WiFi connected\r\n");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// start webSocket server
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

	MDNS.begin(WIFI_HOSTNAME);

	server.on("/", []() {
        	// send index.html (should be gzip'ed, but :whatever:)
        	server.send(200, "text/html", webpage, sizeof(webpage));
	});

    server.begin();

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
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
	webSocket.loop();
	server.handleClient();

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
	const int vel = 100000000L / delta_usec;
	const int force = vel * vel;

	// on positive velocity pulls, check for a sign change
	// for tracking power and distance
	if (delta_usec > 0)
	{
		// if the delta_usec is too fast, this is probably an error
		// and we should discard this point.100000000
		if (delta_usec < 2500)
			return;

		if (last_delta_usec < 0)
		{
			// sign change: this is the start of a new stroke
			// compute spm * 10
			const unsigned stroke_delta = now - start_usec;
			spm = 600000000L / stroke_delta;
			
			start_usec = now;
			stroke_power = 0;

			// blank line to mark the log
			Serial.println();
		}

		stroke_power += force;
	}

	last_delta_usec = delta_usec;

	String msg = String("") + now + "," + (now - start_usec) + "," + delta_usec + "," + force + "," + stroke_power + "," + spm;
	Serial.println(msg);
	webSocket.broadcastTXT(msg);
}
