//
// Created by frank on 17-8-15.
//

#include "../Timer.h"

int main()
{
	uint32_t current = 0;
	Timer t(&current);

	assert(t.timeLeft() == UINT32_MAX);
	assert(t.timeout() == 0);

	t.set(10, false);
	assert(t.timeLeft() == 10);
	assert(t.timeout() == 0);

	current = 9;
	assert(t.timeLeft() == 1);
	assert(t.timeout() == 0);

	current = 10;
	assert(t.timeLeft() == 0);
	assert(t.timeout() == 1);

	t.set(10, true);
	assert(t.timeLeft() == 10);
	assert(t.timeout() == 0);

	current = 100;
	assert(t.timeLeft() == 0);
	assert(t.timeout() == 9);

	current = 109;
	assert(t.timeLeft() == 1);
	assert(t.timeout() == 0);

	current = 110;
	assert(t.timeLeft() == 0);
	assert(t.timeout() == 1);
}