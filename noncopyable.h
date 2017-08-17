//
// Created by frank on 17-8-13.
//

#ifndef FDP_NONCOPYABLE_H
#define FDP_NONCOPYABLE_H

class noncopyable
{
public:
	noncopyable(const noncopyable&) = delete;
	void operator=(const noncopyable&) = delete;

protected:
	noncopyable() = default;
	~noncopyable() = default;
};

#endif //FDP_NONCOPYABLE_H
