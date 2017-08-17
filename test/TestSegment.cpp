//
// Created by frank on 17-8-14.
//

#include "../Segment.h"

#include <string>
#include <cstring>
#include <cassert>

bool operator==(const Segment& lhs, const Segment& rhs)
{
	if (lhs.ident != rhs.ident ||
		lhs.cmd != rhs.cmd ||
		lhs.frg != rhs.frg ||
		lhs.wnd != rhs.wnd ||
		lhs.ts != rhs.ts ||
		lhs.seq != rhs.seq ||
		lhs.una != rhs.una ||
		lhs.len != rhs.len)
		return false;

	return memcmp(lhs.data, rhs.data, lhs.len) == 0;
}

int main()
{
	const uint32_t segmentNum = 10;
	std::string userData = "1234567890";
	SegmentPtr segs[segmentNum];

	/* user send segments */
	for (uint32_t i = 0; i < segmentNum; ++i) {
		segs[i] = createSegment(userData.c_str(), userData.length());

		assert(segs[i] != nullptr);
		assert(segs[i]->len == userData.length());
		assert(strncmp(userData.c_str(), segs[i]->data, userData.length()) == 0);

		segs[i]->ident = i;
		segs[i]->cmd = i;
		segs[i]->frg = i;
		segs[i]->wnd = i;
		segs[i]->ts = i;
		segs[i]->seq = i;
		segs[i]->una = i;
		/* segs[i]->len = userData.length(); */
		segs[i]->resendts = i;
		segs[i]->rto = i;
		segs[i]->fastack = i;
		segs[i]->xmit = i;
	}

	/* encode segments */
	char buffer[10240];
	char *ptr = buffer;
	uint32_t size = sizeof(buffer);
	for (uint32_t i = 0; i < segmentNum; ++i) {
		encodeSegment(*segs[i], ptr, size);
		uint32_t offset = ptr - buffer;
		assert(offset == sizeof(buffer) - size);
		assert(offset == (i + 1) * (ENCODE_OVERHEAD + userData.length()));
	}

	/* decode segments */
	const char *cptr = buffer;
	size = sizeof(buffer);
	for (uint32_t i = 0; i < segmentNum; ++i) {
		SegmentPtr seg = decodeSegment(cptr, size);
		assert(*seg == *segs[i]);

		uint32_t offset = cptr - buffer;
		assert(offset == sizeof(buffer) - size);
		assert(offset == (i + 1) * (ENCODE_OVERHEAD + userData.length()));
	}
}