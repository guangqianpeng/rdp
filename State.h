//
// Created by frank on 17-8-16.
//

#ifndef FDP_STATE_H
#define FDP_STATE_H

#include "Timer.h"
#include "Logger.h"

enum
{
	STATE_ESTABLISHED,

	STATE_CLOSE_WAIT,
	STATE_LAST_ACK,

	STATE_FIN_WAIT_1,
	STATE_FIN_WAIT_2,
	STATE_CLOSING,
	STATE_TIME_WAIT,

	STATE_NORMAL_CLOSED,
	STATE_TIMEOUT_CLOSED
};

class State
{
public:
	State(const uint32_t* current, uint32_t msl) :
			timeWaitTimer_(current),
			current_(current),
			msl_(msl),
			state_(STATE_ESTABLISHED)
	{}

	int get() const
	{
		if (state_ == STATE_TIME_WAIT &&
			timeWaitTimer_.timeout() > 0) {
			state_ = STATE_NORMAL_CLOSED;
			LOG_INFO("%u TIME_WAIT -> CLOSED", *current_);
		}

		return state_;
	}

	bool closed() const
	{
		if (state_ == STATE_TIME_WAIT &&
			timeWaitTimer_.timeout() > 0) {
			state_ = STATE_NORMAL_CLOSED;
			LOG_INFO("%u TIME_WAIT -> CLOSED", *current_);
		}

		return state_ == STATE_NORMAL_CLOSED ||
			   state_ == STATE_TIMEOUT_CLOSED;
	}

	void sendFin()
	{
		switch (state_) {
			case STATE_ESTABLISHED:
				state_ = STATE_FIN_WAIT_1;
				LOG_INFO("    ESTABLISHED -> FIN_WAIT_1");
				break;
			case STATE_CLOSE_WAIT:
				state_ = STATE_LAST_ACK;
				LOG_INFO("    CLOSE_WAIT -> LAST_ACK");
				break;
			default:
				LOG_ERROR("    State::sendFin(): bad state transition %d", state_);
				break;
		}
	}

	void recvFin()
	{
		switch (state_) {
			case STATE_ESTABLISHED:
				state_ = STATE_CLOSE_WAIT;
				LOG_INFO("    ESTABLISHED -> CLOSE_WAIT");
				break;
			case STATE_FIN_WAIT_1:
				state_ = STATE_CLOSING;
				LOG_INFO("    FIN_WAIT_1 -> CLOSING");
				break;
			case STATE_FIN_WAIT_2:
				state_ = STATE_TIME_WAIT;
				timeWaitTimer_.set(2 * msl_, false);
				LOG_INFO("    FIN_WAIT_2 -> TIME_WAIT");
				break;
			default:
				LOG_ERROR("    State::recvFin(): bad state transition %d", state_);
				break;
		}
	}

	void recvAck()
	{
		switch (state_)
		{
			case STATE_FIN_WAIT_1:
				state_ = STATE_FIN_WAIT_2;
				LOG_INFO("    FIN_WAIT_1 -> FIN_WAIT_2");
				break;
			case STATE_LAST_ACK:
				state_ = STATE_NORMAL_CLOSED;
				LOG_INFO("    LAST_ACK -> CLOSED");
				break;
			case STATE_CLOSING:
				state_ = STATE_TIME_WAIT;
				timeWaitTimer_.set(2 * msl_, false);
				LOG_INFO("    CLOSING -> TIME_WAIT");
				break;
			default:
				LOG_ERROR("State::recvAck(): bad state transition %d", state_);
				break;
		}
	}

	void timeout() { state_ = STATE_TIMEOUT_CLOSED; }

	mutable Timer timeWaitTimer_;

private:
	const uint32_t* current_;
	const uint32_t msl_;
	mutable int state_;
};

#endif //FDP_STATE_H
