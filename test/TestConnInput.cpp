//
// Created by frank on 17-8-14.
//

#include "../Connection.h"

#include <vector>
#include <cstring>

const uint32_t mss = 100;
const uint32_t maxSegmentSize = mss * 128;

typedef std::vector<SegmentPtr> SegVector;

SegVector generateSegs(const char *data, uint32_t size, uint32_t startSeq)
{
	std::vector<SegmentPtr> segs;

	uint32_t count = (size + mss - 1) / mss;

	if (count > MAX_FRAGMENTS)
		throw std::invalid_argument("data size too large");

	segs.reserve(count);

	for (; count > 0; count--) {
		uint32_t len = std::min(mss, size);
		SegmentPtr seg = createSegment(data, len);

		/* other data members will be initialized when move to buffer */
		seg->ident = 1;
		seg->cmd = 0;
		seg->seq = startSeq++;
		seg->frg = count - 1;
		segs.push_back(std::move(seg));

		data += len;
		size -= len;
	}

	return segs;
}

uint32_t generateInput(char* buffer, uint32_t size)
{
	char userData[2 * size] = {};

	SegVector segs = generateSegs(userData, size, 0);

	char *ptr = buffer;
	size = 2 * maxSegmentSize;
	for (auto& s: segs)
		encodeSegment(*s, ptr, size);

	return ptr - buffer;
}

void test(int userDataLen)
{
	Connection conn(1, mss, 10);
	char recvBuffer[maxSegmentSize];
	char netBuffer[2 * maxSegmentSize];
	uint32_t size;

	printf("----BEGIN----\n");

	size = generateInput(netBuffer, userDataLen);
	conn.input(netBuffer, size);

	assert(conn.peekLength() == userDataLen);

	int ret = conn.recv(recvBuffer, userDataLen);
	if (userDataLen == 0)
		assert(ret == -1);
	else
		assert(ret == userDataLen);

	printf("-----END-----\n\n");
}

int main()
{

	Connection conn(1, mss, 10);
	assert(conn.peekLength() == -1);

	test(100);
	test(101);
	test(1000);
	test(1001);
	test(128 * mss);
	// test(128 * mss + 1);
}