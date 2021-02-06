/*
 * Make a websocket connection to the rower and then draw the data
 */

let data = [];

function setup()
{
	let canvas = createCanvas(windowWidth-10, windowHeight-10);

	// Move the canvas so it’s inside our <div id="sketch-holder">.
	canvas.parent('sketch-holder');

	background(0);

	// connect to the rower's websocket and append any new data as it comes in
	let connection = new WebSocket('ws://192.168.178.50:81/', ['arduino']);
	connection.onopen = function () { connection.send('Connect ' + new Date()); };
	connection.onerror = function (error) { console.log('WebSocket Error ', error);};
	connection.onmessage = function (e) {
		console.log('Server: ', e.data);
		let cols = e.data.split(',');
		data.push(cols);
	};
}

let last_sx = 0;
let last_sy = 0;

function draw()
{
	//background(0);
	//textSize(128);
	//textAlign(RIGHT, BOTTOM);

	// retrieve the most recent data
	let d = data[data.length-1];
	if (!d)
		return;

	// if the time stamp on the new data is 0, then this is
	// the start of a new stroke
	if (d[1] == 0) {
		noStroke();
		fill(0, 0, 0, 10);
		rect(0, 0, 400, 200);
		last_sx = 0;
		last_sy = 0;
	}
	strokeWeight(2);
	stroke(0,255,0);
	let sx = d[1] * 100 / 1e6; // microseconds to ~100 pixels per second, up to 4 seconds
	let sy = -d[2] * 100 / 1e5;

	if (sx > 400)
		sx = 400;
	if (sy > 100)
		sy = 100;
	if (sy < -100)
		sy = -100;

	line(last_sx, last_sy+100, sx, sy+100);
	last_sx = sx;
	last_sy = sy;
}
