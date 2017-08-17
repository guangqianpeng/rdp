//
// Created by frank on 17-8-13.
//

#ifndef FDP_CONNECTION_H
#define FDP_CONNECTION_H

#include "Congestion.h"
#include "RttEstimator.h"
#include "Timer.h"
#include "Segment.h"
#include "State.h"

#include <list>
#include <deque>
#include <functional>
#include <algorithm>
#include <vector>

class Connection;

typedef std::function<void(Connection&, const char *, size_t)> OutputCallback;

enum Error {
	ERR_AGAIN = -1,
	ERR_TRUNCED = -2,
	ERR_TIMEOUT = -3,
	ERR_MSGSIZE = -4,
};

class Connection: noncopyable
{
public:
	Connection(uint32_t ident, uint32_t mtu = 1400, uint32_t interval = 10);
	~Connection();

	/* return size,
	 * return ERR_PIPE
	 * return ERR_MSGSIZE */
	int send(const char *data, uint32_t size);

	/* return size,
	 * return 0 for FIN,
	 * return -1 for EAGAIN
	 * return -2 for short buffer
	 * return -3 for timeout */
	int recv(char *buffer, uint32_t size);
	int peekLength() const;
	void input(const char *data, uint32_t size);
	void update(uint32_t current);
	uint32_t check() const;
	void stopSend();
	bool getStopSend() const { return userStopSend_; }

	void setSendWind(uint32_t sendWind);
	void setRecvWind(uint32_t recvWind);
	void setOutputCallback(OutputCallback cb)
	{ outputCb_ = std::move(cb); }

	bool normalClosed() const
	{ return state.get() == STATE_NORMAL_CLOSED; }
	bool timeoutClosed() const
	{ return state.get() == STATE_TIMEOUT_CLOSED; }
	uint32_t ident() const { return ident_; }
	size_t sendQueueSize() const { return sendQueue_.size(); }
	size_t recvQueueSize() const { return recvQueue_.size(); }
	size_t mss() const { return mss_; }

private:
	void flush();
	void flushPendingAcks();
	void flushWindProbe();
	void flushWindTell();
	void flushSendBuffer();
	void output(const Segment& seg);

	void moveRecvBufferToQueue();
	void moveSendQueueToBuffer();
	uint32_t recvWindUnused();

	int unackSendBuffer(uint32_t unack);
	int ackSendBuffer(uint32_t ack);

	void insertRecvBuffer(SegmentPtr seg);

	void appendAck(uint32_t seq, uint32_t ts)
	{ ackQueue_.push_back({seq, ts}); }

	void incrFastAck(uint32_t maxAck);

	const uint32_t ident_;
	const uint32_t mtu_;
	const uint32_t mss_;
	const uint32_t interval_;

	RttEstimator rtte_;
	Congestion cong_;

	uint32_t current_;

	Timer flushTimer_;
	Timer probeTimer_;
	uint32_t probeWait_;

	bool updated;

	typedef std::deque<SegmentPtr> SegQueue;
	typedef std::list<SegmentPtr> SegList;

	struct ACK { uint32_t ack, ts; };
	typedef std::deque<ACK> AckQueue;

	OutputCallback outputCb_;

	std::vector<char> buffer;

	SegQueue sendQueue_, recvQueue_;
	SegList sendBuffer_, recvBuffer_;
	AckQueue ackQueue_;

	uint32_t sendUnak_;
	uint32_t sendNext_;
	uint32_t sendEnd_;
	uint32_t sendWind_;

	uint32_t recvNext_;
	uint32_t recvEnd_;
	uint32_t recvWind_;

	uint32_t rmtWind_;

	bool needWindTell;
	bool sendEndValid_;
	bool recvEndValid_;
	bool userStopSend_;

	uint32_t fastRsendTimes_;
	uint32_t timeoutRsendTimes_;

	State state;
};


#endif //FDP_CONNECTION_H
