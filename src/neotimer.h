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
    // Constructors and destructor
    Neotimer();
    Neotimer(unsigned long t);
    Neotimer(const Neotimer& other); // Kopierkonstruktor
    Neotimer& operator=(const Neotimer& other); // Zuweisungsoperator
    ~Neotimer();

    // Timer control
    void start();
    void start(unsigned long t);
    unsigned long  stop();
    void reset();
    void set(unsigned long t);
    unsigned long get();
    unsigned long getElapsed();
    void restart();
    void pause();
    void resume();

    // Timer status
    boolean done();
    boolean waiting();
    boolean started();


    // Repeat functionality
    boolean repeat();
    boolean repeat(int times);
    boolean repeat(int times, unsigned long t);
    void repeatReset();
    void repeatStop();
    boolean repeatRunning();

    // Debounce functionality
    boolean debounce(boolean signal);
    boolean getDebouncedState() const;

private:
    struct myTimer {
        unsigned long time;
        unsigned long last;
        boolean done;
        boolean started;
    };

    struct myTimer _timer;
    boolean _waiting;
    bool _lastSignal = false;
    bool _debouncedState = false;
    int _repetitions = NEOTIMER_UNLIMITED;
    unsigned long _pausedAt = 0;
};


#endif

