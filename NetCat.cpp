//
// Created by frank on 17-8-16.
//

#include "Logger.h"
#include "Connection.h"

#include <functional>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

class UdpSocket: noncopyable
{
public:
	explicit
	UdpSocket(Connection *conn):
			conn_(conn),
			connected_(false)
	{
		fd_ = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd_ == -1)
			LOG_SYSFATAL("socket() error");

		const int recvBuf = 4096*4096;
		int err = setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &recvBuf, sizeof(recvBuf));
		if (err == -1)
			LOG_SYSFATAL("setsockopt() error");
	}

	int fd() const { return fd_; }

	void bind(const char *ip, uint16_t port)
	{
		sockaddr_in addr;
		int ret = inet_pton(AF_INET, ip, &addr.sin_addr);
		if (ret != 1) {
			LOG_ERROR("inet_pton(): bad ip address %s", ip);
			exit(EXIT_FAILURE);
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		ret = ::bind(fd_, (sockaddr *) &addr, sizeof(addr));
		if (ret == -1) {
			LOG_SYSERR("::bind() error");
			exit(EXIT_FAILURE);
		}
	}

	void connect(const char *ip, uint16_t port)
	{
		assert(!connected_);
		connected_ = true;

		sockaddr_in addr;
		int ret = inet_pton(AF_INET, ip, &addr.sin_addr);
		if (ret != 1) {
			LOG_ERROR("inet_pton(): bad ip address %s", ip);
			exit(EXIT_FAILURE);
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		ret = ::connect(fd_, (sockaddr*)&addr, sizeof(addr));
		if (ret == -1)
			LOG_SYSFATAL("::connect() error");
	}

	void output(Connection& conn, const char* data, size_t len)
	{
		assert(&conn == conn_);
		assert(!conn.normalClosed());

		/* rarely block */
		ssize_t ret = ::send(fd_, data, len, 0);
		if (ret == -1) {
			LOG_SYSERR("::send() error(ignore)");
			//conn.close();
		}
	}

	void input()
	{
		if (conn_->normalClosed()) return;

		char buf[65536];
		ssize_t ret;
		sockaddr_in addr;

		while (true) {
			if (!connected_) {
				socklen_t len = sizeof(addr);
				ret = ::recvfrom(fd_, buf, sizeof(buf), MSG_DONTWAIT,
										 (sockaddr*)&addr, &len);
			}
			else
				ret = ::recv(fd_, buf, sizeof(buf), MSG_DONTWAIT);

			if (ret == -1) {
				if (errno != EAGAIN) {
					LOG_SYSERR("::recv() error(ignore)");
					//conn_->close();
				}
				else break;
			}

			if (ret >= 0 && !connected_) {
				int err = ::connect(fd_, (sockaddr *) &addr, sizeof(addr));
				if (err == -1)
					LOG_SYSFATAL("::connect() error");
				connected_ = true;
			}

			if (ret > 0)
				conn_->input(buf, static_cast<uint32_t>(ret));
		}
	}

	bool connected() const { return connected_; }

private:
	Connection *conn_;
	bool connected_;
	int fd_;
};

class NetCat: noncopyable
{
public:
	NetCat(UdpSocket* sock, Connection* conn,
		   uint32_t maxSendQueueSize = 4096):
			sock_(sock), conn_(conn),
			maxSendQueueSize_(maxSendQueueSize)
	{
		if (sock_->connected())
			stdinfd_.fd = STDIN_FILENO;
		else
			stdinfd_.fd = -1;
		stdinfd_.events = POLLIN;

		sockfd_.fd = sock_->fd();
		sockfd_.events = POLLIN;

		int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		int err = fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
		if (err == -1)
			LOG_SYSFATAL("fcntl error()");

		conn_->setSendWind(4096);
		conn_->setRecvWind(4096);
	}

	~NetCat() { close(sock_->fd()); }

	void loop()
	{
		char buffer[conn_->mss()];

		while (true) {
			uint32_t waitTime = conn_->check();

			int ret = ::poll(pfd_, 2, waitTime);
			if (ret == -1 && errno != EINTR)
				LOG_SYSFATAL("poll() error");

			if (stdinfd_.revents & POLLIN) {
				while (true) {
					ssize_t n = ::read(stdinfd_.fd, buffer, sizeof(buffer));
					if (n == 0) {
						conn_->stopSend();
						enableUserInput(false);
						break;
					} else if (n == -1) {
						if (errno == EAGAIN) break;
						else LOG_SYSFATAL("::read() error");
					} else {
						ret = conn_->send(buffer, static_cast<uint32_t>(n));
						if (ret < 0) {
							LOG_ERROR("Connection::send() error %d", ret);
							return;
						}
						/* control memory usage */
						if (conn_->sendQueueSize() >= maxSendQueueSize_) {
							enableUserInput(false);
							break;
						}
					}
				}
			}

			if (sockfd_.revents & POLLIN) {

				sock_->input();
				if (!conn_->getStopSend()) {
					if (sock_->connected())
						enableUserInput(true);
					if (conn_->sendQueueSize() < maxSendQueueSize_)
						enableUserInput(true);
				}

				while (true) {
					ret = conn_->recv(buffer, sizeof(buffer));
					if (ret == ERR_AGAIN) break;
					else if (ret == 0) {
						conn_->stopSend();
						enableUserInput(false);
						break;
					}
					else if (ret < 0) {
						LOG_ERROR("Connection::recv() error %d", ret);
						return;
					}
					else {
						ssize_t n = write(STDOUT_FILENO, buffer, static_cast<size_t>(ret));
						if (n != ret)
							LOG_SYSFATAL("::write() error");
					}
				}
			}

			conn_->update(current());
			if (conn_->timeoutClosed()) {
				LOG_ERROR("connection timeout");
				return;
			}
			if (conn_->normalClosed()) {
				LOG_INFO("Connection closed normally");
				return;
			}
		}
	}

private:

	void enableUserInput(bool on)
	{ stdinfd_.fd = (on ? STDIN_FILENO : -1); }

	static uint32_t current()
	{
		timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}

	pollfd pfd_[2];
	pollfd &stdinfd_ = pfd_[0];
	pollfd &sockfd_ = pfd_[1];
	UdpSocket *sock_;
	Connection *conn_;
	const uint32_t maxSendQueueSize_;
};

int parseAddr(char *addr)
{
	char *colon = strchr(addr, ':');
	if (colon == nullptr)
		return -1;
	int port = atoi(colon + 1);
	if (port == 0 || port > UINT16_MAX)
		return -1;
	*colon = '\0';
	return port;
}

void usage()
{
	printf("usage: ./netCat -l <ip:port>\n"
		   "                -c <ip:port>\n"
		   "                -v verbose\n"
	);
}

int main(int argc, char **argv)
{
	int opt, logLevel = LOG_LEVEL_INFO;
	uint32_t mtu = 1500 - 20 - ENCODE_OVERHEAD;
	char *localAddr = nullptr, *remoteAddr = nullptr;
	while ( (opt = getopt(argc, argv, "l:c:m:v")) != -1) {
		switch (opt) {
			case 'l':
				localAddr = optarg;
				break;
			case 'c':
				remoteAddr = optarg;
				break;
			case 'v':
				logLevel = LOG_LEVEL_DEBUG;
				break;
			case 'm':
				mtu = atoi(optarg);
				if (mtu == 0 || mtu > 65535) {
					fprintf(stderr, "bad mtu");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}

	if (localAddr == nullptr && remoteAddr == nullptr) {
		usage();
		exit(EXIT_FAILURE);
	}

	set_log_fd(STDERR_FILENO);
	set_log_level(logLevel);

	Connection conn(1, mtu, 5);
	UdpSocket sock(&conn);
	conn.setOutputCallback(std::bind(
			&UdpSocket::output, &sock, _1, _2, _3));

	if (localAddr != nullptr) {
		int port = parseAddr(localAddr);
		if (port == -1) {
			fprintf(stderr, "bad local address %s", localAddr);
			exit(EXIT_FAILURE);
		}
		sock.bind(localAddr, static_cast<uint16_t>(port));
	}

	if (remoteAddr != nullptr) {
		int port = parseAddr(remoteAddr);
		if (port == -1) {
			fprintf(stderr, "bad remote address %s", remoteAddr);
			exit(EXIT_FAILURE);
		}
		sock.connect(remoteAddr, static_cast<uint16_t>(port));
	}

	NetCat nc(&sock, &conn);
	nc.loop();
}