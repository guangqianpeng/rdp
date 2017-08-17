//
// Created by frank on 17-8-13.
//

#ifndef FDP_RTTESTIMATOR_H
#define FDP_RTTESTIMATOR_H

#include "noncopyable.h"
#include "Timer.h"

#include <cstdint>
#include <algorithm>
#include <cassert>

#include <cstdio>

class RttEstimator: noncopyable
{
public:
	explicit
	RttEstimator(uint32_t granularity,
				 uint32_t minRto, uint32_t maxRto):
			srtt_(0), rttvar_(0), rto_(minRto),
			minRto_(minRto), maxRto_(maxRto),
			granularity_(granularity)
	{ assert(minRto < maxRto);}

	uint32_t srtt() const { return static_cast<uint32_t>(srtt_); }
	uint32_t rttvar() const { return static_cast<uint32_t>(rttvar_); }
	uint32_t rto() const { return static_cast<uint32_t>(rto_); }

	void update(int32_t rtt)
	{
		if (rtt == 0) rtt = 1;

		if (srtt_ == 0) {
			srtt_ = rtt;
			rttvar_ = rtt / 2;
		}
		else {
			int32_t err = rtt - srtt_;
			srtt_ += err / 8;
			if (err < 0) err = -err;
			rttvar_ += (err - rttvar_) / 4;
		}

		rto_ = srtt_ + std::max(4 * rttvar_, granularity_);
		if (rto_ < minRto_) rto_ = minRto_;
		if (rto_ > maxRto_) rto_ = maxRto_;
	}

	void update(uint32_t ts, uint32_t current)
	{
		int32_t rtt = Timer::timeDiff(current, ts);
		if (rtt >= 0) update(rtt);
	}

private:
	int32_t srtt_, rttvar_, rto_;
	const int32_t minRto_, maxRto_;
	const int32_t granularity_;
};


#endif //FDP_RTTESTIMATOR_H
