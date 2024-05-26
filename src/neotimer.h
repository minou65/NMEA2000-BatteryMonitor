// neotimer.h
#pragma once

#ifndef _NEOTIMER_h
#define _NEOTIMER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define NEOTIMER_INDEFINITE -1
#define NEOTIMER_UNLIMITED -1

class Neotimer {
public:
	//Methods
	Neotimer();
	Neotimer(unsigned long _t);      //Constructor
	~Neotimer();            //Destructor

	void init();            //Initializations
	boolean done();         //Indicates time has elapsed
	boolean repeat(int times);
	boolean repeat(int times, unsigned long _t);
	boolean repeat();
	void repeatReset();
	boolean waiting();		// Indicates timer is started but not finished
	boolean started();		// Indicates timer has started
	void start();			//Starts a timer
	void start(unsigned long t);
	unsigned long stop();	//Stops a timer and returns elapsed time
	unsigned long getEllapsed();	// Gets the ellapsed time
	void restart();
	void reset();           //Resets timer to zero
	void set(unsigned long t);
	unsigned long get();
	boolean debounce(boolean signal);
	int repetitions = NEOTIMER_UNLIMITED;

private:

	struct myTimer {
		unsigned long time;
		unsigned long last;
		boolean done;
		boolean started;
	};

	struct myTimer _timer;
	boolean _waiting;
};


#endif

