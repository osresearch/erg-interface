/*
 * Make a websocket connection to the rower and then draw the data
 */

let new_data = null;
let data = [];
let total_work = 0;
let last_work = 0;

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
		if (e.data == "Connected")
			return;

		new_data = e.data.split(',');

		// fix up the spm colun
		new_data[4] /= 10.0;

		// fake out the last power measurement, just in case
		if (new_data[1] == 0)
		{
			last_work = int(total_work);
			new_data[3] = int(total_work);
		}
	};
}

let last_sx = 0;
let last_sy = 0;

function draw_stroke(d,w,h)
{
	// if the time stamp on the new data is 0, then this is
	// the start of a new stroke.  decay the old chart a little bit
	if (d[1] == 0) {
		noStroke();
		fill(0, 0, 0, 35);
		rect(0, 0, w, h);
		last_sx = 0;
		last_sy = 0;
		total_work = 0;

		strokeWeight(1);
		noFill();
		stroke(50);
		line(0,h/2, w, h/2);
	}

	let vel = d[2] / 5000;
	if (vel > 0)
	{
		let force = vel * vel;
		total_work += force;
	} else {
		force = -vel * vel;
	}

	strokeWeight(2);
	stroke(0,255,0);
	let sx = d[1] * w / 3e6; // up to 3 seconds of display
	let sy = -vel * h/2 / 10;

	if (sx > w)
		sx = w;
	if (sy > h/2)
		sy = h/2;
	if (sy < -h/2)
		sy = -h/2;

	line(last_sx, last_sy+h/2, sx, sy+h/2);
	last_sx = sx;
	last_sy = sy;

	noStroke();
	fill(0);
	rect(w-200,0,200,50);
	fill(0,255,0);
	textAlign(RIGHT, BOTTOM);
	textSize(50);
	//text(int(d[3] / 1e3), w, 50);
	text(int(total_work), w, 50);
}

function draw_strip(d,w,h,index,max,scale_step)
{
	let now = d[0];
	let start = data[0][0];
	let delta = now - start;
	let sx = w / (delta+1);
	let sy = h / max;

	fill(0);
	rect(0,0,w,h);

	noFill();
	stroke(50);
	strokeWeight(1);
	for(let v = 0 ; v < max ; v += scale_step)
		line(0, h -  v*sy, w, h -  v*sy);

	for(let t = 15e6 ; t < delta ; t += 15 * 1e6)
		line(t*sx, 0, t*sx, h);
	stroke(100);
	for(let t = 60e6 ; t < delta ; t += 60 * 1e6)
		line(t*sx, 0, t*sx, h);

	stroke(0xff);
	strokeWeight(2);
	beginShape();

	for(let p of data)
	{
		if (p[1] != 0)
			continue;
		let x = (p[0] - start) * sx;
		let y = p[index] * sy;
		vertex(x, h - y);
	}

	endShape();
}

function draw()
{
	// retrieve the most recent data
	if (!new_data)
		return;
	data.push(new_data);

	push();
	translate(0,0);
	draw_stroke(new_data, 400, 400);
	pop();

	// draw the strokes per minute
	push();
	translate(400,0);
	draw_strip(new_data, width-400, 200, 4, 60, 10);

	fill(0xff);
	noStroke();
	textSize(50);
	textAlign(RIGHT, BOTTOM);
	text(new_data[4], width-400, 100);

	pop();

	// draw the power per minute
	push();
	translate(400,200);
	draw_strip(new_data, width-400, 200, 3, 1000, 100);

	fill(0xff);
	noStroke();
	textSize(50);
	textAlign(RIGHT, BOTTOM);
	text(int(last_work), width-400, 100);

	pop();

	new_data = null;
}
