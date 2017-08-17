//
// Created by frank on 17-8-13.
//

#ifndef FDP_CONGESTION_H
#define FDP_CONGESTION_H

#include "noncopyable.h"

#include <cstdint>
#include <algorithm>
#include <cassert>

class Congestion: noncopyable
{
public:
	explicit
	Congestion(const uint32_t& mss, uint32_t icw):
			mss_(mss), icw_(icw),
			cwnd_(icw), incr_(mss * icw),
			ssthresh_(2048)
	{}

	uint32_t cwnd() const { return cwnd_; }

	void onGoodUna()
	{
		if (cwnd_ <= ssthresh_) {
			cwnd_++;
			incr_ += mss_;
		}
		else {
			/* a little more aggressive */
			incr_ += mss_ * mss_ / incr_ +  mss_ / 16;
			if ((cwnd_ + 1) * mss_ <= incr_) {
				cwnd_++;
			}
		}
	}


	void onFastResend(uint32_t inflight)
	{
		ssthresh_ = std::max(inflight / 2, icw_);
		cwnd_ = ssthresh_ + 3;
		incr_ = cwnd_ * mss_;
	}

	void onTimeout()
	{
		cwnd_ = icw_;
		incr_ = cwnd_ * mss_;
		ssthresh_ = INT32_MAX / mss_;
	}

private:
	const uint32_t& mss_;
	const uint32_t icw_;
	uint32_t cwnd_;		/* in packets */
	uint32_t incr_;		/* in bytes */
	uint32_t ssthresh_;
};

#endif //FDP_CONGESTION_H
