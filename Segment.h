//
// Created by frank on 17-8-13.
//

#ifndef FDP_SEGMENT_H
#define FDP_SEGMENT_H

#include "noncopyable.h"

#include <cstdint>
#include <memory>
#include <vector>

struct Segment: noncopyable
{
	uint32_t	ident;
	uint32_t	cmd;
	uint32_t 	frg;
	uint32_t 	wnd;
	uint32_t 	ts;
	uint32_t 	seq;
	uint32_t 	una;
	uint32_t 	len;

	uint32_t 	resendts;
	uint32_t 	rto;
	uint32_t 	fastack;
	uint32_t 	xmit;

	char 		data[];
};


class SegmentDeleter
{
public:
	SegmentDeleter() = default;
	void operator()(Segment* seg) const
	{ free(seg); }
};

#define ENCODE_OVERHEAD 24
#define MAX_FRAGMENTS 255

typedef std::unique_ptr<Segment, SegmentDeleter> SegmentPtr;
SegmentPtr createSegment(const char *data, uint32_t size);
uint32_t encodeSegment(const Segment& seg, char*& buffer, uint32_t& size);
uint32_t encodeSegment(const Segment& seg, std::vector<char>& buffer);
SegmentPtr decodeSegment(const char *&data, uint32_t &size);

#endif //FDP_SEGMENT_H
