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
#define NET_dolog 0

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
	(void)msg;
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

/* Ethernet header size */
static constexpr size_t ETH_HLEN = 14;
static constexpr size_t ARP_PKT_LEN = 28; /* ARP payload for IPv4/Ethernet */
static constexpr uint16_t ETHERTYPE_IP = 0x0800;
static constexpr uint16_t ETHERTYPE_ARP = 0x0806;

/* Fake MACs for the Ethernet shim.  libslirp operates at Layer 2 (Ethernet),
   but SLIP is Layer 3 (raw IP).  We maintain a minimal ARP responder so
   libslirp can resolve our guest's MAC before sending IP packets. */
static const uint8_t kGuestMAC[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x0F};
static const uint8_t kGatewayMAC[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};

/* Read big-endian uint16 from buffer */
static uint16_t readU16BE(const uint8_t *p)
{
	return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

void SlirpBackend::txByte(uint8_t byte)
{
	if (decoder_.feed(byte))
	{
		auto pkt = decoder_.packet();
		if (pkt.empty()) return;

		/* Determine EtherType from IP version nibble */
		uint16_t etherType = ETHERTYPE_IP;
		uint8_t ipVer = (pkt[0] >> 4) & 0x0F;
		if (ipVer == 6) etherType = 0x86DD; /* IPv6 */

		/* Wrap raw IP in a fake Ethernet frame for libslirp */
		std::vector<uint8_t> frame;
		frame.reserve(ETH_HLEN + pkt.size());
		frame.insert(frame.end(), kGatewayMAC, kGatewayMAC + 6); /* dst */
		frame.insert(frame.end(), kGuestMAC, kGuestMAC + 6);	 /* src */
		frame.push_back(static_cast<uint8_t>(etherType >> 8));
		frame.push_back(static_cast<uint8_t>(etherType & 0xFF));
		frame.insert(frame.end(), pkt.begin(), pkt.end());

		SLP_LOG("tx packet: %zu IP bytes -> %zu eth frame -> slirp_input", pkt.size(),
				frame.size());
		if (slirp_) slirp_input(slirp_, frame.data(), static_cast<int>(frame.size()));
	}
}

/* Handle an ARP request from libslirp by auto-replying with our fake MAC.
   This lets libslirp resolve 10.0.2.15 → kGuestMAC so it can deliver
   IP packets addressed to the guest. */
void SlirpBackend::handleArp(const uint8_t *frame, size_t len)
{
	if (len < ETH_HLEN + ARP_PKT_LEN) return;

	const uint8_t *arp = frame + ETH_HLEN;

	uint16_t htype = readU16BE(arp + 0); /* hardware type: 1 = Ethernet */
	uint16_t ptype = readU16BE(arp + 2); /* protocol type: 0x0800 = IPv4 */
	uint16_t oper = readU16BE(arp + 6);	 /* operation: 1 = request */

	if (htype != 1 || ptype != ETHERTYPE_IP || oper != 1)
	{
		NET_LOG("arp: ignoring htype=%u ptype=0x%04X oper=%u", htype, ptype, oper);
		return;
	}

	/* ARP request fields (after htype/ptype/hlen/plen/oper):
	   offset  8: sender MAC (6)
	   offset 14: sender IP  (4)
	   offset 18: target MAC (6) — zeroed in request
	   offset 24: target IP  (4) */
	const uint8_t *senderMAC = arp + 8;
	const uint8_t *senderIP = arp + 14;
	const uint8_t *targetIP = arp + 24;

	NET_LOG("arp: who-has %u.%u.%u.%u? tell %u.%u.%u.%u", targetIP[0], targetIP[1], targetIP[2],
			targetIP[3], senderIP[0], senderIP[1], senderIP[2], senderIP[3]);

	/* Build ARP reply */
	uint8_t reply[ETH_HLEN + ARP_PKT_LEN];
	/* Ethernet header: dst = whoever asked, src = our guest MAC */
	std::memcpy(reply + 0, senderMAC, 6);
	std::memcpy(reply + 6, kGuestMAC, 6);
	reply[12] = 0x08;
	reply[13] = 0x06; /* ARP */

	/* ARP payload */
	uint8_t *r = reply + ETH_HLEN;
	r[0] = 0x00;
	r[1] = 0x01; /* htype = Ethernet */
	r[2] = 0x08;
	r[3] = 0x00; /* ptype = IPv4 */
	r[4] = 6;	 /* hlen */
	r[5] = 4;	 /* plen */
	r[6] = 0x00;
	r[7] = 0x02;					   /* oper = reply */
	std::memcpy(r + 8, kGuestMAC, 6);  /* sender MAC = us */
	std::memcpy(r + 14, targetIP, 4);  /* sender IP = target they asked for */
	std::memcpy(r + 18, senderMAC, 6); /* target MAC = whoever asked */
	std::memcpy(r + 24, senderIP, 4);  /* target IP = their IP */

	NET_LOG("arp: reply %u.%u.%u.%u is-at %02X:%02X:%02X:%02X:%02X:%02X", targetIP[0], targetIP[1],
			targetIP[2], targetIP[3], kGuestMAC[0], kGuestMAC[1], kGuestMAC[2], kGuestMAC[3],
			kGuestMAC[4], kGuestMAC[5]);

	if (slirp_) slirp_input(slirp_, reply, sizeof(reply));
}

void SlirpBackend::onSlirpOutput(const uint8_t *pkt, size_t len)
{
	if (len < ETH_HLEN)
	{
		NET_LOG("rx: runt frame (%zu bytes), dropped", len);
		return;
	}

	uint16_t etherType = readU16BE(pkt + 12);

	if (etherType == ETHERTYPE_ARP)
	{
		/* ARP — handle internally, don't pass to guest SLIP layer */
		handleArp(pkt, len);
		return;
	}

	if (etherType != ETHERTYPE_IP && etherType != 0x86DD)
	{
		NET_LOG("rx: unknown EtherType 0x%04X, dropped", etherType);
		return;
	}

	/* Strip Ethernet header — SLIP carries raw IP only */
	const uint8_t *ip = pkt + ETH_HLEN;
	size_t ipLen = len - ETH_HLEN;

	SLP_LOG("rx packet: %zu eth -> %zu IP bytes -> SLIP encode", len, ipLen);
	std::vector<uint8_t> framed;
	slip::encode(ip, ipLen, framed);
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
	uint32_t timeout = UINT32_MAX;
	slirp_pollfds_fill_socket(slirp_, &timeout, cb_add_poll, &pollFds_);

	/* Phase 2: poll (non-blocking) */
	int ret = 0;
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

		ret = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 0);

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
	}

	/* Phase 3: tell libslirp what happened */
	slirp_pollfds_poll(slirp_, (ret < 0) ? 1 : 0, cb_get_revents, &pollFds_);
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
