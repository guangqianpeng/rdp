//
// Created by frank on 17-8-14.
//

#include "Segment.h"

#include <netinet/in.h>
#include <cstring>
#include <cassert>

namespace
{
struct Header
{
	uint32_t ident;
	uint8_t cmd;
	uint8_t frg;
	uint16_t wnd;
	uint32_t ts;
	uint32_t seq;
	uint32_t una;
	uint32_t len;
};

Header createHeader(const Segment& seg)
{
	Header hdr;
	hdr.ident = htonl(seg.ident);
	hdr.cmd = static_cast<uint8_t>(seg.cmd);
	hdr.frg = static_cast<uint8_t >(seg.frg);
	hdr.wnd = htons(static_cast<uint16_t>(seg.wnd));
	hdr.ts = htonl(seg.ts);
	hdr.seq = htonl(seg.seq);
	hdr.una = htonl(seg.una);
	hdr.len = htonl(seg.len);
	return hdr;
}

}

SegmentPtr createSegment(const char *data, uint32_t size)
{
	uint32_t segTotalLen = sizeof(Segment) + size;

	auto seg = static_cast<Segment*>(malloc(segTotalLen));

	/* other data members cannot initialize here */
	seg->len = size;
	if (size > 0)
		memcpy(seg->data, data, size);

	return SegmentPtr(seg);
}

uint32_t encodeSegment(const Segment& seg, char*& buffer, uint32_t& size)
{
	uint32_t totalLen = sizeof(Header) + seg.len;

	assert(size >= totalLen);

	Header hdr = createHeader(seg);

	memcpy(buffer, &hdr, sizeof(hdr));
	if (seg.len > 0)
		memcpy(buffer + sizeof(hdr), seg.data, seg.len);

	buffer += totalLen;
	size -= totalLen;

	return totalLen;
}

uint32_t encodeSegment(const Segment& seg, std::vector<char>& buffer)
{
	Header hdr = createHeader(seg);
	char *ptr = reinterpret_cast<char*>(&hdr);
	buffer.insert(buffer.end(), ptr, ptr + sizeof(hdr));
	if (seg.len > 0)
		buffer.insert(buffer.end(), seg.data, seg.data + seg.len);
	return sizeof(hdr) + seg.len;
}

SegmentPtr decodeSegment(const char *&data, uint32_t &size)
{
	auto *hdr = reinterpret_cast<const Header*>(data);

	uint32_t ident = ntohl(hdr->ident);
	uint32_t cmd = hdr->cmd;
	uint32_t frg = hdr->frg;
	uint32_t wnd = ntohs(hdr->wnd);
	uint32_t ts = ntohl(hdr->ts);
	uint32_t seq = ntohl(hdr->seq);
	uint32_t una = ntohl(hdr->una);
	uint32_t len = ntohl(hdr->len);

	uint32_t segTotalLen = sizeof(Segment) + len;
	uint32_t hdrTotalLen = ENCODE_OVERHEAD + len;

	if (size < hdrTotalLen)
		return nullptr;

	auto seg = static_cast<Segment*>(malloc(segTotalLen));

	seg->ident = ident;
	seg->cmd = cmd;
	seg->frg = frg;
	seg->wnd = wnd;
	seg->ts = ts;
	seg->seq = seq;
	seg->una = una;
	seg->len = len;

	if (len > 0)
		memcpy(seg->data, data + ENCODE_OVERHEAD, len);

	data += hdrTotalLen;
	size -= hdrTotalLen;

	return SegmentPtr(seg);
}