//
// Created by frank on 17-8-13.
//

#ifndef FDP_TIMER_H
#define FDP_TIMER_H

#include "noncopyable.h"

#include <cstdint>
#include <cassert>

class Timer: noncopyable
{
public:
	explicit
	Timer(const uint32_t* current):
			current_(current),
			start_(0),
			interval_(0),
			repeat_(false),
			running_(false)
	{}

	void set(uint32_t interval, bool repeat)
	{
		assert(interval > 0);

		start_ = *current_;
		interval_ = interval;
		repeat_ = repeat;

		running_ = true;
	}

	void reset() { running_ = false; }

	uint32_t timeout()
	{
		if (!running_)
			return 0;

		int32_t diff = timeDiff(*current_, start_ + interval_);

		if (diff < 0)
			return 0;
		else if (!repeat_) {
			running_ = false;
			return 1;
		}
		else {
			uint32_t ret = diff / interval_ + 1;
			start_ += ret * interval_;
			return ret;
		}
	}

	uint32_t timeLeft() const
	{
		if (!running_)
			return UINT32_MAX;

		int32_t diff = timeDiff(start_ + interval_, *current_);
		return diff < 0 ? 0 : static_cast<uint32_t>(diff);
	}

	static int32_t timeDiff(uint32_t lhs, uint32_t rhs)
	{
		return static_cast<int32_t>(lhs - rhs);
	}

private:
	const uint32_t* current_;
	uint32_t start_;
	uint32_t interval_;
	bool repeat_;
	bool running_;
};


#endif //FDP_TIMER_H
