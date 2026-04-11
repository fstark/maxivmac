/*
	serial_slirp.cpp — SLIP networking backend using libslirp.

	Bridges SLIP-framed serial bytes (from the SCC) to/from
	libslirp's IP packet interface.
*/

#if HAVE_SLIRP

#include "devices/serial_slirp.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <poll.h>

/* -----------------------------------------------------------------------
   LOG — NET subsystem (libslirp lifecycle)
   ----------------------------------------------------------------------- */
#define NET_dolog 1

#if NET_dolog
#define NET_LOG(fmt, ...) std::fprintf(stderr, "[NET] " fmt "\n", ##__VA_ARGS__)
#else
#define NET_LOG(fmt, ...) ((void)0)
#endif

/* -----------------------------------------------------------------------
   libslirp C callbacks
   ----------------------------------------------------------------------- */

static slirp_ssize_t cb_send_packet(const void *buf, size_t len, void *opaque)
{
	auto *self = static_cast<SlirpBackend *>(opaque);
	self->onSlirpOutput(static_cast<const uint8_t *>(buf), len);
	return static_cast<slirp_ssize_t>(len);
}

static void cb_guest_error(const char *msg, void * /*opaque*/)
{
	NET_LOG("guest error: %s", msg);
}

static int64_t cb_clock_get_ns(void * /*opaque*/)
{
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

/* Timer support — we use the v4 timer_new_opaque interface.
   libslirp only creates a small number of timers (typically 1 for RA).
   We store the expiry time and fire them during poll(). */
struct SlirpTimer
{
	SlirpTimerId id;
	void *cb_opaque;
	int64_t expire_ms; /* -1 = inactive */
	Slirp *slirp;
};

static void *cb_timer_new_opaque(SlirpTimerId id, void *cb_opaque, void *opaque)
{
	(void)opaque;
	auto *t = new SlirpTimer;
	t->id = id;
	t->cb_opaque = cb_opaque;
	t->expire_ms = -1;
	t->slirp = nullptr; /* set in init_completed */
	return t;
}

static void cb_timer_free(void *timer, void * /*opaque*/)
{
	delete static_cast<SlirpTimer *>(timer);
}

static void cb_timer_mod(void *timer, int64_t expire_time, void * /*opaque*/)
{
	auto *t = static_cast<SlirpTimer *>(timer);
	t->expire_ms = expire_time;
}

static void cb_register_poll_socket(slirp_os_socket /*fd*/, void * /*opaque*/)
{
	/* nothing — we collect fds during pollfds_fill */
}

static void cb_unregister_poll_socket(slirp_os_socket /*fd*/, void * /*opaque*/)
{
	/* nothing */
}

static void cb_notify(void * /*opaque*/)
{
	/* nothing — we poll synchronously */
}

static void cb_init_completed(Slirp *slirp, void * /*opaque*/)
{
	(void)slirp;
}

/* ------- poll helpers for slirp_pollfds_fill_socket / slirp_pollfds_poll --- */

static int cb_add_poll(slirp_os_socket fd, int events, void *opaque)
{
	auto *fds = static_cast<std::vector<SlirpBackend::PollEntry> *>(opaque);
	/* Return the index that get_revents will use. */
	int idx = static_cast<int>(fds->size());
	/* Use a temporary struct to avoid aggregate init issues */
	SlirpBackend::PollEntry entry;
	entry.fd = fd;
	entry.events = events;
	entry.revents = 0;
	fds->push_back(entry);
	return idx;
}

static int cb_get_revents(int idx, void *opaque)
{
	auto *fds = static_cast<std::vector<SlirpBackend::PollEntry> *>(opaque);
	if (idx < 0 || idx >= static_cast<int>(fds->size())) return 0;
	return (*fds)[static_cast<size_t>(idx)].revents;
}

/* -----------------------------------------------------------------------
   SlirpBackend implementation
   ----------------------------------------------------------------------- */

SlirpBackend::SlirpBackend()
{
	SlirpConfig cfg;
	std::memset(&cfg, 0, sizeof(cfg));
	cfg.version = 6;
	cfg.restricted = false;
	cfg.in_enabled = true;
	cfg.in6_enabled = false; /* keep it simple — IPv4 only for classic Mac */

	/* Network: 10.0.2.0/24 */
	cfg.vnetwork.s_addr = htonl(0x0A000200);	/* 10.0.2.0 */
	cfg.vnetmask.s_addr = htonl(0xFFFFFF00);	/* 255.255.255.0 */
	cfg.vhost.s_addr = htonl(0x0A000202);		/* 10.0.2.2 (gateway) */
	cfg.vdhcp_start.s_addr = htonl(0x0A00020F); /* 10.0.2.15 */
	cfg.vnameserver.s_addr = htonl(0x0A000203); /* 10.0.2.3 (DNS) */

	static SlirpCb callbacks;
	static bool callbacks_init = false;
	if (!callbacks_init)
	{
		std::memset(&callbacks, 0, sizeof(callbacks));
		callbacks.send_packet = cb_send_packet;
		callbacks.guest_error = cb_guest_error;
		callbacks.clock_get_ns = cb_clock_get_ns;
		callbacks.timer_free = cb_timer_free;
		callbacks.timer_mod = cb_timer_mod;
		callbacks.notify = cb_notify;
		callbacks.init_completed = cb_init_completed;
		callbacks.timer_new_opaque = cb_timer_new_opaque;
		callbacks.register_poll_socket = cb_register_poll_socket;
		callbacks.unregister_poll_socket = cb_unregister_poll_socket;
		callbacks_init = true;
	}

	slirp_ = slirp_new(&cfg, &callbacks, this);
	if (!slirp_)
	{
		NET_LOG("FATAL: slirp_new() failed");
		return;
	}

	NET_LOG("init: network 10.0.2.0/24 gateway 10.0.2.2 dns 10.0.2.3");
}

SlirpBackend::~SlirpBackend()
{
	if (slirp_)
	{
		slirp_cleanup(slirp_);
		NET_LOG("shutdown");
	}
}

void SlirpBackend::txByte(uint8_t byte)
{
	if (decoder_.feed(byte))
	{
		auto pkt = decoder_.packet();
		SLP_LOG("tx packet: %zu bytes -> slirp_input", pkt.size());
		if (slirp_) slirp_input(slirp_, pkt.data(), static_cast<int>(pkt.size()));
	}
}

void SlirpBackend::onSlirpOutput(const uint8_t *pkt, size_t len)
{
	SLP_LOG("rx packet: %zu bytes from slirp", len);
	std::vector<uint8_t> framed;
	slip::encode(pkt, len, framed);
	for (uint8_t b : framed)
		txToGuest_.push(b);
}

bool SlirpBackend::rxReady()
{
	return !txToGuest_.empty();
}

uint8_t SlirpBackend::rxByte()
{
	uint8_t b = txToGuest_.front();
	txToGuest_.pop();
	return b;
}

void SlirpBackend::poll()
{
	if (!slirp_) return;

	/* Phase 1: ask libslirp which fds to watch */
	pollFds_.clear();
	uint32_t timeout = 0; /* non-blocking */
	slirp_pollfds_fill_socket(slirp_, &timeout, cb_add_poll, &pollFds_);

	/* Phase 2: poll (non-blocking) */
	if (!pollFds_.empty())
	{
		/* Build a pollfd array from our entries */
		std::vector<struct pollfd> pfds(pollFds_.size());
		for (size_t i = 0; i < pollFds_.size(); ++i)
		{
			pfds[i].fd = pollFds_[i].fd;
			pfds[i].events = 0;
			if (pollFds_[i].events & SLIRP_POLL_IN) pfds[i].events |= POLLIN;
			if (pollFds_[i].events & SLIRP_POLL_OUT) pfds[i].events |= POLLOUT;
			if (pollFds_[i].events & SLIRP_POLL_PRI) pfds[i].events |= POLLPRI;
			pfds[i].revents = 0;
		}

		int ret = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 0);

		/* Convert revents back to SLIRP_POLL_* flags */
		if (ret > 0)
		{
			for (size_t i = 0; i < pfds.size(); ++i)
			{
				int rev = 0;
				if (pfds[i].revents & POLLIN) rev |= SLIRP_POLL_IN;
				if (pfds[i].revents & POLLOUT) rev |= SLIRP_POLL_OUT;
				if (pfds[i].revents & POLLPRI) rev |= SLIRP_POLL_PRI;
				if (pfds[i].revents & POLLERR) rev |= SLIRP_POLL_ERR;
				if (pfds[i].revents & POLLHUP) rev |= SLIRP_POLL_HUP;
				pollFds_[i].revents = rev;
			}
		}

		/* Phase 3: tell libslirp what happened */
		slirp_pollfds_poll(slirp_, (ret < 0) ? 1 : 0, cb_get_revents, &pollFds_);
	}
	else
	{
		/* No fds, but still let libslirp run timers etc. */
		slirp_pollfds_poll(slirp_, 0, cb_get_revents, &pollFds_);
	}
}

bool SlirpBackend::addHostFwd(bool isUDP, uint16_t hostPort, const char *guestIP,
							  uint16_t guestPort)
{
	if (!slirp_) return false;

	struct in_addr host_addr;
	host_addr.s_addr = htonl(INADDR_ANY);

	struct in_addr guest_addr;
	if (inet_pton(AF_INET, guestIP, &guest_addr) != 1)
	{
		NET_LOG("hostfwd: bad guest IP '%s'", guestIP);
		return false;
	}

	int rc = slirp_add_hostfwd(slirp_, isUDP ? 1 : 0, host_addr, hostPort, guest_addr, guestPort);
	if (rc < 0)
	{
		NET_LOG("hostfwd: failed %s:%u -> %s:%u", isUDP ? "udp" : "tcp", hostPort, guestIP,
				guestPort);
		return false;
	}

	NET_LOG("hostfwd: %s:%u -> %s:%u", isUDP ? "udp" : "tcp", hostPort, guestIP, guestPort);
	return true;
}

#endif /* HAVE_SLIRP */
