// 
// 
// 

#include "neotimer.h"

/*
 * Default constructor for the Neotimer class.
 * Initializes the timer with a default interval of 1000 milliseconds (1 second).
 * The timer is not started after construction.
 */
Neotimer::Neotimer() {
	this->_timer.time = 1000; // Default 1 second interval if not specified
	this->_timer.started = false;
}

/*
 * Constructor for the Neotimer class with a custom interval.
 * Initializes the timer with the specified interval in milliseconds.
 * The timer is not started after construction.
 */
Neotimer::Neotimer(unsigned long t) {
	this->_timer.time = t;
	this->_timer.started = false;
}

Neotimer::Neotimer(const Neotimer& other) {
	this->_timer = other._timer;
	this->_waiting = other._waiting;
	this->_lastSignal = other._lastSignal;
	this->_debouncedState = other._debouncedState;
	this->_repetitions = other._repetitions;
}

Neotimer& Neotimer::operator=(const Neotimer& other) {
	if (this != &other) {
		this->_timer = other._timer;
		this->_waiting = other._waiting;
		this->_lastSignal = other._lastSignal;
		this->_debouncedState = other._debouncedState;
		this->_repetitions = other._repetitions;
	}
	return *this;
}

/*
 * Destructor for the Neotimer class.
 * No special cleanup is required.
 */
Neotimer::~Neotimer() {

}

/*
 * Checks if the timer interval has elapsed.
 * This function returns true exactly once when the configured interval has passed
 * since the timer was started or last reset. After returning true, the timer remains
 * in the "done" state until it is reset or restarted.
 */
boolean Neotimer::done() {
	if (!this->_timer.started)
		return false;
	unsigned long _now = millis();
	if ((_now - this->_timer.last) >= this->_timer.time) {
		this->_timer.done = true;
		this->_waiting = false;
		return true;
	}
	return false;
}

/*
 * Repeats a timer indefinitely with the configured interval.
 * This function is useful for executing a task periodically, for example in the Arduino loop.
 */
boolean Neotimer::repeat() {
	bool ret = false;
	if (this->done()) {
		this->reset();
		ret = true;
	}
	if (!this->_timer.started) {
		this->_timer.last = millis();
		this->_timer.started = true;
		this->_waiting = true;
	}
	return ret;
}

/*
 * Repeats a timer for a specified number of times with the configured interval.
 * After the specified number of intervals, always returns false.
 */
boolean Neotimer::repeat(int times) {
	// Initialize repetitions on first call or after reset
	if (this->_repetitions == NEOTIMER_UNLIMITED) {
		this->_repetitions = times;
	}

	// Stop if repetitions are exhausted or times is zero
	if (this->_repetitions == 0 || times == 0) {
		return false;
	}

	// Only decrement and return true if timer interval elapsed
	if (this->repeat()) {
		this->_repetitions--;
		return true;
	}
	return false;
}

/*
 * Repeats a timer for a specified number of times with a custom interval.
 * This function is useful for executing a task periodically with a specific interval,
 * but only a limited number of times.
 */
boolean Neotimer::repeat(int times, unsigned long t) {
	this->_timer.time = t;
	return this->repeat(times);
}

/*
 * Resets the repetition counter for the repeat(int times) and repeat(int times, unsigned long t) functions.
 * After calling repeatReset(), the next call to repeat(int times) or repeat(int times, t) will start a new counting cycle
 * with the specified number of repetitions.
 */
void Neotimer::repeatReset() {
	this->_repetitions = NEOTIMER_UNLIMITED;
}

/*
 * Immediately stops the repeat functionality of the timer.
 *
 * After calling repeatStop(), the internal repetition counter (_repetitions) is set to 0.
 * As a result, repeat(int times) and repeat(int times, unsigned long t) will always return false,
 * even if the originally specified number of repetitions has not yet been reached.
 *
 * This is useful to abort an ongoing repeat sequence prematurely, for example if a certain
 * condition occurs and further repetitions should be prevented.
 *
 * Example:
 *   Neotimer timer(1000); // Timer with 1 second interval
 *   timer.repeat(10);     // Start 10 repetitions
 *   // ... during program execution ...
 *   timer.repeatStop();   // Immediately aborts the repetitions
 *
 * Note:
 *   - After repeatStop(), you can start a new repeat sequence by calling repeatReset().
 */
void Neotimer::repeatStop() {
	this->_repetitions = 0;
}

boolean Neotimer::repeatRunning() {
	if (this->_repetitions != NEOTIMER_UNLIMITED && this->_repetitions > 0) {
		return true;
	}
	return false;
}

/*
 * Returns whether a repeat sequence is currently active.
 *
 * This function checks if the timer is in the middle of a repeat sequence
 * started by repeat(int times) or repeat(int times, unsigned long t).
 * It returns true if the internal repetition counter (_repetitions) is greater than 0
 * and not set to NEOTIMER_UNLIMITED, indicating that there are still repetitions left.
 * Otherwise, it returns false.
 *
 * This is useful to determine if a limited repeat operation is still running.
 *
 * Example:
 *   timer.repeat(5);
 *   while (timer.repeatRunning()) {
 *     // Do something while repetitions are active
 *   }
 *
 * Note:
 *   - If repeatReset() was called or repeat() is used without a limit, repeatRunning() returns false.
 */
boolean Neotimer::waiting() {
	return (this->_timer.started && !this->done()) ? true : false;
}

/*
 * Indicates if the timer has started.
 */
boolean Neotimer::started() {
	return this->_timer.started;
}

/*
 * Starts the timer with the currently configured interval.
 * The timer is reset and started immediately. The elapsed time is set to zero.
 */
void Neotimer::start() {
	this->reset();
	this->_timer.started = true;
	this->_waiting = true;
}

/*
 * Starts the timer with a specific interval in milliseconds.
 * The timer is reset, the interval is set to the given value, and the timer is started immediately.
 */
void Neotimer::start(unsigned long t) {
	this->set(t);
	this->reset();
	this->_timer.started = true;
	this->_waiting = true;
}

/*
 * Stops the timer and returns the elapsed time in milliseconds since the last start or reset.
 * After calling stop(), the timer is no longer running until start() or reset() is called again.
 */
unsigned long Neotimer::stop() {
	this->_timer.started = false;
	this->_waiting = false;
	return this->getElapsed();
}

/*
 * Returns the elapsed time in milliseconds since the timer was last started or reset.
 * This function does not affect the timer's running state.
 */
unsigned long Neotimer::getElapsed() {
	return millis() - this->_timer.last;
}

/*
 * Restarts a stopped timer without resetting the elapsed time.
 * If the timer is not yet done, this function resumes the timer from where it was stopped.
 * If the timer has already elapsed (done), this function does nothing.
 */
void Neotimer::restart() {
	if (!this->done()) {
		this->_timer.started = true;
		this->_waiting = true;
	}
}

/*
 * Pauses the timer, preserving the elapsed time.
 * After calling pause(), the timer stops counting but remembers how much time has already elapsed.
 * You can resume the timer later using resume(), and it will continue from where it was paused.
 *
 * Usage example:
 *   timer.start(5000); // Start a 5 second timer
 *   // ... some code ...
 *   timer.pause(); // Pause the timer
 *   // ... some code ...
 *   timer.resume(); // Resume the timer, continues counting the remaining time
 *
 * Note:
 *   - If the timer is not running, pause() has no effect.
 *   - Use resume() to continue the timer after pausing.
 */
void Neotimer::pause() {
	if (this->_timer.started) {
		this->_pausedAt = millis();
		this->_timer.started = false;
	}
}

/*
 * Resumes a previously paused timer.
 * The timer continues counting from where it was paused, preserving the remaining time.
 *
 * Usage example:
 *   timer.pause();
 *   // ... some code ...
 *   timer.resume(); // Continues the timer from where it was paused
 *
 * Note:
 *   - If the timer was not paused, resume() has no effect.
 *   - If the timer has already finished, resume() has no effect.
 */
void Neotimer::resume() {
	if (!this->_timer.started && this->_pausedAt != 0) {
		unsigned long pausedDuration = millis() - this->_pausedAt;
		this->_timer.last += pausedDuration;
		this->_timer.started = true;
		this->_pausedAt = 0;
	}
}

/*
 * Resets the timer to zero and clears the done state.
 * After calling reset(), the timer is stopped and the elapsed time is set to zero.
 * Use start() to start the timer again.
 */
void Neotimer::reset() {
	this->stop();
	this->_timer.last = millis();
	this->_timer.done = false;
}

/*
 * Sets the timer interval (preset) in milliseconds.
 * This function allows you to change the timer's interval at runtime.
 */
void Neotimer::set(unsigned long t) {
	this->_timer.time = t;
}

/*
 * Gets the current timer interval (preset) in milliseconds.
 * This function returns the currently configured timer interval.
 */
unsigned long Neotimer::get() {
	return this->_timer.time;
}

/*
 * Debounces a digital signal (e.g. from a button or switch).
 * Call this function repeatedly with the current raw signal value.
 * When the signal changes, a timer is started. Only if the signal remains
 * stable for the configured debounce interval, the debounced state is updated.
 * The function returns true only when the debounced state changes.
 */
boolean Neotimer::debounce(boolean signal) {
	if (signal != this->_lastSignal) {
		this->start();
		this->_lastSignal = signal;
	}

	if (this->done()) {
		if (signal != this->_debouncedState) {
			this->_debouncedState = signal;
			return true; // State has changed and is stable
		}
	}
	return false;
}

/*
 * Returns the current debounced state of the digital input.
 * This function should be used in conjunction with debounce().
 * It always provides the last stable (debounced) value, regardless of whether
 * a change has just occurred.
 */
boolean Neotimer::getDebouncedState() const {
	return this->_debouncedState;
}