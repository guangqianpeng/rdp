//
// Created by frank on 17-8-13.
//

#include "../RttEstimator.h"
#include <cstdio>

void printRte(RttEstimator &rte, const char *prefix, uint32_t rtt = 0)
{
	printf("%s: rtt %u, srtt %u, rttvar %u, rto %u\n",
		   prefix, rtt, rte.srtt(), rte.rttvar(), rte.rto());
}

int main()
{
	uint32_t grand = 10;
	RttEstimator rte(grand, 1000, 15000);

	uint32_t rtt, srtt = 1000, rttvar = 200;

	for (int i = 0; i < 100; ++i) {
		rtt = static_cast<uint32_t>(srtt - rttvar + drand48() * rttvar * 2);
		rte.update(rtt);
		printRte(rte, "ack", rtt);
	}
}