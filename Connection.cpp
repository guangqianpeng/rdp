//
// Created by frank on 17-8-13.
//

#include "Connection.h"
#include "Logger.h"

#include <cstring>

namespace
{
enum Command
{
	CMD_DATA,
	CMD_ACK,
	CMD_WIND_TELL,
	CMD_WIND_RPOBE,
};

#ifndef NDEBUG
const uint32_t DEFAULT_ICW = 1;
#else
const uint32_t DEFAULT_ICW = 10;
#endif

const uint32_t MIN_SEND_WIND = MAX_FRAGMENTS;
const uint32_t MIN_RECV_WIND = MAX_FRAGMENTS;
const uint32_t INIT_PROBE_WAIT = 500;
const uint32_t MAX_PROBE_WAIT = 5000;
const uint32_t MIN_RTO = 1000;
const uint32_t MAX_RTO = 15000;
const uint32_t MAX_RESEND_TIME = 6;
const uint32_t MSL = 5000;

int32_t seqDiff(uint32_t lhs, uint32_t rhs)
{
	return static_cast<int32_t>(lhs - rhs);
}

const char* connStr(const Connection& conn)
{
	static char buffer[1024];
	sprintf(buffer, "Connection %u(%p)",  conn.ident(), &conn);
	return buffer;
}

}

Connection::Connection(uint32_t ident, uint32_t mtu, uint32_t interval):
		ident_(ident),
		mtu_(mtu),
		mss_(mtu - ENCODE_OVERHEAD),
		interval_(interval),
		rtte_(interval_, MIN_RTO, MAX_RTO),
		cong_(mss_, DEFAULT_ICW),
		current_(0),
		flushTimer_(&current_),
		probeTimer_(&current_),
		probeWait_(0),
		updated(false),
		sendUnak_(0), sendNext_(0), sendEnd_(0),
		sendWind_(MIN_SEND_WIND),
		recvNext_(0), recvEnd_(0),
		recvWind_(MIN_RECV_WIND),
		rmtWind_(MIN_RECV_WIND),
		needWindTell(false),
		sendEndValid_(false),
		recvEndValid_(false),
		userStopSend_(false),
		fastRsendTimes_(0),
		timeoutRsendTimes_(0),
		state(&current_, MSL)
{
	assert(mtu_ > ENCODE_OVERHEAD);
	buffer.reserve(std::max(mtu_, 1024u));
}

Connection::~Connection()
{
	LOG_INFO("%u %s "
					  "sendUnak %u sendNext %u sendEnd %u, "
					  "recvNext %u recvEnd %u, "
					  "fastResend %u timeoutResend %u",
			 current_, connStr(*this),
			 sendUnak_, sendNext_, sendEnd_,
			 recvNext_, recvEnd_,
			 fastRsendTimes_, timeoutRsendTimes_);
}

int Connection::send(const char *data, uint32_t size)
{
	assert(!userStopSend_);
	assert(!normalClosed());
	assert(!timeoutClosed());

	uint32_t sizeTemp = size;

	uint32_t count = (size + mss_ - 1) / mss_;

	if (count > MAX_FRAGMENTS) {
		LOG_ERROR("%u %s send() msessage too long",
				  current_, connStr(*this));
		return ERR_MSGSIZE;
	}

	for (; count > 0; count--) {
		uint32_t len = std::min(mss_, size);
		SegmentPtr seg = createSegment(data, len);

		/* other data members will be initialized when move to buffer */
		seg->frg = count - 1;
		sendQueue_.push_back(std::move(seg));

		data += len;
		size -= len;
	}

	assert(size == 0);

	return sizeTemp;
}

int Connection::recv(char *buffer, uint32_t size)
{
	assert(!timeoutClosed());

	int len = peekLength();
	if (len <= 0) return len;
	if (size < static_cast<uint32_t>(len)) {
		LOG_ERROR("%u %s recv() buffer too short",
				  current_, connStr(*this));
		return ERR_MSGSIZE;
	}

	bool full = (recvQueue_.size() >= recvWind_);

	char *ptr = buffer;
	while (!recvQueue_.empty()) {
		SegmentPtr seg = std::move(recvQueue_.front());
		recvQueue_.pop_front();
		memcpy(ptr, seg->data, seg->len);
		ptr += seg->len;

		if (seg->frg == 0)
			break;
	}

	moveRecvBufferToQueue();

	if (full && recvQueue_.size() < recvWind_)
		needWindTell = true;

	return static_cast<int>(ptr - buffer);
}

void Connection::stopSend()
{
	if(userStopSend_)
		return;

	userStopSend_ = true;

	SegmentPtr seg = createSegment(nullptr, 0);
	seg->frg = 0;
	sendQueue_.push_back(std::move(seg));
}

int Connection::peekLength() const
{
	assert(!timeoutClosed());

	if (recvQueue_.empty()) {
		if (recvEndValid_)
			return 0;
	}

	uint32_t totalLen = 0;
	bool completeSegment = false;
	for (auto &seg: recvQueue_) {
		totalLen += seg->len;
		if (seg->frg == 0) {
			completeSegment = true;
			break;
		}
	}

	if (!completeSegment)
		return ERR_AGAIN;
	return totalLen;
}

void Connection::input(const char *data, uint32_t size)
{
	if (state.closed()) {
		LOG_ERROR("%u %s input() after connection closed",
				 current_, connStr(*this));
		return;
	}

	uint32_t unack = sendUnak_;
	uint32_t maxAck = 0;
	bool recvAck = false;
	bool recvData = false;

	while (size > 0) {
		SegmentPtr seg = decodeSegment(data, size);
		if (seg == nullptr) {
			LOG_ERROR("%u %s decodeSegment() error",
					  current_, connStr(*this));
			return;
		}
		if (seg->ident != ident_) {
			LOG_ERROR("%u %s input() bad ident %u",
					  current_, connStr(*this), seg->ident);
			return;
		}
		if (seg->cmd != CMD_DATA &&
			seg->cmd != CMD_ACK &&
			seg->cmd != CMD_WIND_RPOBE &&
			seg->cmd != CMD_WIND_TELL) {
			LOG_ERROR("%u %s input() bad command %u",
					  current_, connStr(*this), seg->cmd);
			return;
		}
		uint32_t seq = seg->seq;
		uint32_t ts = seg->ts;

		unackSendBuffer(seg->una);

		rmtWind_ = seg->wnd;

		switch (seg->cmd) {
			case CMD_DATA:
				if (seqDiff(seq, recvNext_ + recvWind_) < 0) {
					if (!recvEndValid_ || seqDiff(seq, recvEnd_) < 0) {
						insertRecvBuffer(std::move(seg));
						appendAck(seq, ts);
						recvData = true;
					}
				}
				LOG_DEBUG("%u %s recv packet %u", current_, connStr(*this), seq);
				break;
			case CMD_ACK:
				ackSendBuffer(seq);
				rtte_.update(ts, current_);
				/* for fast ack */
				if (!recvAck) {
					recvAck = true;
					maxAck = seq;
				}
				else if (seqDiff(maxAck, seq) < 0) {
					maxAck = seq;
				}
				LOG_DEBUG("%u %s recv ack %u", current_, connStr(*this), seq);
				LOG_DEBUG("    rtt %d srtt %u rttvar %u rto %u",
						  Timer::timeDiff(current_, ts),
						  rtte_.srtt(), rtte_.rttvar(), rtte_.rto());
				break;
			case CMD_WIND_RPOBE:
				needWindTell = true;
				LOG_DEBUG("%u %s recv window probe", current_, connStr(*this), seq);
				break;
			case CMD_WIND_TELL:
				/* nothing to do */
				LOG_DEBUG("%u %s recv window notify", current_, connStr(*this), seg->wnd);
				break;
			default:
				break;
		}
	}

	assert(size == 0);

	int32_t segmentAcked = seqDiff(sendUnak_ , unack);
	if (segmentAcked > 0) {
		cong_.onGoodUna();
		LOG_DEBUG("    cwnd %d", cong_.cwnd());
	}

	if (recvData)
		moveRecvBufferToQueue();

	if (recvAck)
		incrFastAck(maxAck);
}

void Connection::update(uint32_t current)
{
	if (state.closed()) {
		LOG_WARN("%u %s update() after connection closed",
				  current_, connStr(*this));
		return;
	}

	current_ = current;

	if (!updated) {
		updated = true;
		flushTimer_.set(interval_, true);
	}

	if (flushTimer_.timeout() > 0)
		flush();
}

uint32_t Connection::check() const
{
	if (!updated) return 0;
	if (state.get() == STATE_TIME_WAIT)
		return state.timeWaitTimer_.timeLeft();
	return flushTimer_.timeLeft();
}

void Connection::setSendWind(uint32_t sendWind)
{
	if (sendBuffer_.size() >= sendWind_ &&
		sendBuffer_.size() < sendWind)
		moveSendQueueToBuffer();
	sendWind_ = std::max(sendWind, MIN_SEND_WIND);
}

void Connection::setRecvWind(uint32_t recvWind)
{
	if (recvQueue_.size() >= recvWind_ &&
		recvQueue_.size() < recvWind) {
		needWindTell = true;
		moveRecvBufferToQueue();
	}
	recvWind_ = std::max(recvWind, MIN_RECV_WIND);
}

void Connection::flush()
{
	assert(updated);

	flushPendingAcks();

	flushWindProbe();

	if (needWindTell) {
		flushWindTell();
		needWindTell = false;
	}

	if (!sendEndValid_)
		moveSendQueueToBuffer();
	flushSendBuffer();

	if (!buffer.empty() && !state.closed()) {
		outputCb_(*this, buffer.data(), buffer.size());
		buffer.clear();
	}
}

void Connection::flushPendingAcks()
{
	Segment seg;

	if (ackQueue_.empty())
		return;

	seg.ident = ident_;
	seg.cmd = CMD_ACK;
	seg.wnd = recvWindUnused();
	seg.una = recvNext_;
	seg.len = 0;

	/* make valgrind happy */
	seg.frg = 0;
	seg.ts = 0;
	seg.seq = 0;


	for (auto a: ackQueue_) {
		seg.ts = a.ts;
		seg.seq = a.ack;
		output(seg);

		LOG_DEBUG("%u %s send ack %u", current_, connStr(*this), seg.seq);
	}

	ackQueue_.clear();
}

void Connection::flushWindProbe()
{
	bool needWindProbe = false;

	if (rmtWind_ == 0) {
		if (probeWait_ == 0) {
			needWindProbe = true;
			probeWait_ = INIT_PROBE_WAIT;
			probeTimer_.set(probeWait_, false);
		}
		else if (probeTimer_.timeout() > 0){
			needWindProbe = true;
			probeWait_ = std::min(probeWait_ * 2, MAX_PROBE_WAIT);
			probeTimer_.set(probeWait_, false);
		}
	}
	else {
		probeTimer_.reset();
		probeWait_ = 0;
	}

	if (needWindProbe) {
		Segment seg;
		seg.ident = ident_;
		seg.cmd = CMD_WIND_RPOBE;
		seg.wnd = recvWindUnused();
		seg.una = recvNext_;
		seg.len = 0;

		/* make valgrind happy */
		seg.frg = 0;
		seg.ts = 0;
		seg.seq = 0;

		output(seg);

		LOG_DEBUG("%u %s probe window", current_, connStr(*this));
	}
}

void Connection::flushWindTell()
{
	Segment seg;
	seg.ident = ident_;
	seg.cmd = CMD_WIND_TELL;
	seg.wnd = recvWindUnused();
	seg.una = recvNext_;
	seg.len = 0;

	/* make valgrind happy */
	seg.frg = 0;
	seg.ts = 0;
	seg.seq = 0;

	output(seg);

	LOG_DEBUG("%6u %s notify window(%u)", current_, connStr(*this), seg.wnd);
}

void Connection::flushSendBuffer()
{
	for (auto& seg: sendBuffer_) {

		bool needSend = false;

		if (seg->xmit == 0) {
			/* first send */
			seg->rto = rtte_.rto();
			seg->resendts = current_ + seg->rto;
			seg->xmit++;
			needSend = true;

			LOG_DEBUG("%u %s send packet %u",
					  current_, connStr(*this), seg->seq);
		}
		else if (seg->fastack >= 3) {
			/* fast resend */
			uint32_t inflight = static_cast<uint32_t>(sendBuffer_.size());
			cong_.onFastResend(inflight);
			fastRsendTimes_++;
			seg->fastack = 0;
			needSend = true;

			LOG_DEBUG("%u %s fast resend %u xmit %u",
					  current_, connStr(*this), seg->seq, seg->xmit);
		}
		else if (Timer::timeDiff(current_ , seg->resendts) >= 0) {
			/* timeout resend */
			cong_.onTimeout();
			timeoutRsendTimes_++;
			seg->xmit++;
			seg->rto += rtte_.rto();
			if (seg->rto > MAX_RTO)
				seg->rto = MAX_RTO;
			seg->resendts = current_ + seg->rto;
			needSend = true;

			LOG_INFO("%u %s timeout resend %u xmit %u",
					  current_, connStr(*this), seg->seq, seg->xmit);
		}

		if (needSend) {
			seg->wnd = recvWindUnused();
			seg->ts = current_;
			output(*seg);
		}

		if (seg->xmit >= MAX_RESEND_TIME) {
			LOG_ERROR("%u %s connection timeout",
					  current_, connStr(*this));
			state.timeout();
			break;
		}
	}
}

void Connection::output(const Segment& seg)
{
	assert(outputCb_ != nullptr);

	uint32_t encodeLength = ENCODE_OVERHEAD + seg.len;
	if (buffer.size() + encodeLength > mtu_) {
		outputCb_(*this, buffer.data(), buffer.size());
		buffer.clear();
	}
	assert(encodeLength <= mtu_);
	encodeSegment(seg, buffer);
}

void Connection::moveRecvBufferToQueue()
{
	if (recvEndValid_) return;

	while (recvQueue_.size() < recvWind_) {
		if (recvBuffer_.empty())
			break;
		/* may contain incomplete fragments */
		if (recvNext_ == recvBuffer_.front()->seq) {
			SegmentPtr seg = std::move(recvBuffer_.front());
			recvBuffer_.pop_front();
			assert(seg != nullptr);

			if (seg->len == 0 && !recvEndValid_) {
				recvEnd_ = seg->seq + 1;
				recvEndValid_ = true;

				LOG_DEBUG("%u %s recv fin %u",
						  current_, connStr(*this), seg->seq);
				state.recvFin();
			}

			recvQueue_.push_back(std::move(seg));
			recvNext_++;
		}
		else break;
	}
}

void Connection::moveSendQueueToBuffer()
{
	assert(!sendEndValid_);

	uint32_t wind =
			std::min({sendWind_, rmtWind_, cong_.cwnd()});

	while (sendBuffer_.size() < wind) {
		if (sendQueue_.empty())
			break;
		SegmentPtr seg = std::move(sendQueue_.front());
		sendQueue_.pop_front();
		assert(seg != nullptr);

		seg->ident = ident_;
		seg->cmd = CMD_DATA;
		seg->wnd = recvWindUnused();
		seg->seq = sendNext_++;
		seg->una = sendUnak_;

		seg->fastack = 0;
		seg->xmit = 0;
		/* seg->rto
		 * seg->resendts
		 * seg->ts
		 * will be init when output */

		if (seg->len == 0) {
			assert(sendQueue_.empty());
			assert(userStopSend_);

			sendEnd_ = seg->seq + 1;
			sendEndValid_ = true;

			LOG_DEBUG("%u %s send fin %u",
					  current_, connStr(*this), seg->seq);
			state.sendFin();
		}

		sendBuffer_.push_back(std::move(seg));
	}
}

uint32_t Connection::recvWindUnused()
{
	if (recvWind_ > recvQueue_.size())
		return recvWind_ - static_cast<uint32_t>(recvQueue_.size());
	return 0;
}

int Connection::unackSendBuffer(uint32_t unack)
{
	int numAcked = 0;
	while (!sendBuffer_.empty()) {
		uint32_t seq = sendBuffer_.front()->seq;
		if (seq < unack) {
			if (sendEndValid_ && sendEnd_ == seq + 1) {
				LOG_DEBUG("%u %s recv fin_ack %u",
						  current_, connStr(*this), seq);
				state.recvAck();
			}
			sendBuffer_.pop_front();
			numAcked++;
		}
		else break;
	}

	if (!sendBuffer_.empty())
		sendUnak_ = sendBuffer_.front()->seq;
	else
		sendUnak_ = sendNext_;

	return numAcked;
}

int Connection::ackSendBuffer(uint32_t ack)
{
	auto it = std::find_if(
			sendBuffer_.begin(),
			sendBuffer_.end(),
			[=](const SegmentPtr& seg) {
				return seqDiff(ack, seg->seq) <= 0;
			});

	if (it == sendBuffer_.end() || (*it)->seq != ack)
		return 0;

	if (sendEndValid_ && sendEnd_ == (*it)->seq + 1) {
		LOG_DEBUG("%u %s recv fin_ack %u",
				  current_, connStr(*this), (*it)->seq);
		state.recvAck();
	}

	sendBuffer_.erase(it);

	if (!sendBuffer_.empty())
		sendUnak_ = sendBuffer_.front()->seq;
	else
		sendUnak_ = sendNext_;

	return 1;
}

void Connection::insertRecvBuffer(SegmentPtr seg)
{
	uint32_t seq = seg->seq;

	if (seqDiff(seq, recvNext_) < 0 ||
		seqDiff(seq, recvNext_ + recvWind_) >= 0)
		return;

	if (recvEndValid_ && seqDiff(seq, recvEnd_) >= 0)
		return;

	bool needInsert = true;
	auto it = recvBuffer_.rbegin();
	for (; it != recvBuffer_.rend(); ++it) {
		if ((*it)->seq <= seq) {
			if ((*it)->seq == seq)
				needInsert = false;
			break;
		}
	}

	if (needInsert)
		recvBuffer_.insert(it.base(), std::move(seg));
}

void Connection::incrFastAck(uint32_t maxAck)
{
	for (auto& seg: sendBuffer_) {
		assert(seg->seq != maxAck);
		if (seqDiff(seg->seq, maxAck) < 0)
			seg->fastack++;
		else break;
	}
}
