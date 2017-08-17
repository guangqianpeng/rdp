//
// Created by frank on 17-8-13.
//

#include <cstdio>
#include "../Congestion.h"

int main()
{
	uint32_t mss = 1400, icw = 1, inflight = 8;

	Congestion cong(mss, icw);
	assert(cong.cwnd() == 1);

	/* slow start */
	for (int i = 1; i <= 9; ++i) {
		cong.onGoodUna();
		assert(cong.cwnd() == icw + i);
	}

	/* fast recovery */
	cong.onFastResend(inflight);
	assert(cong.cwnd() == inflight / 2 + 3);

	/* now recovered */
	cong.onGoodUna();
	assert(cong.cwnd() <= inflight / 2 + 4);

	/* congestion avoid */
	for (int i = 1; i <= 200; ++i) {
		cong.onGoodUna();
		printf("cwnd %d\n", cong.cwnd());
	}

	/* timeout */
	cong.onTimeout();
	assert(cong.cwnd() == icw);
}