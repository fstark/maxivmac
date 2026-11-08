#!/usr/bin/env python3
"""ethhub tests (stdlib only).  Run: python3 -I test_ethhub.py /path/to/ethhub

Build the hub with `make SAN=1` to run under ASan/UBSan: the tests check the
exit status, so a sanitizer report fails the run.
"""
import hmac
import hashlib
import os
import random
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

HUB = None


def mac(n):
    return bytes([0x02, 0, 0, 0, 0, n])


BCAST = b"\xff" * 6


def frame(dst, src, payload=b"x" * 46, etype=b"\x08\x00"):
    return dst + src + etype + payload


class Hub:
    def __init__(self, *args, listen="127.0.0.1:0"):
        self.dir = tempfile.TemporaryDirectory()
        self.errpath = os.path.join(self.dir.name, "stderr")
        self.err = open(self.errpath, "w+")
        self.p = subprocess.Popen([HUB, "--listen", listen, *args],
                                  stdout=subprocess.PIPE, stderr=self.err, text=True)
        line = self.p.stdout.readline()
        m = re.match(r"ethhub listening on (\d+)", line)
        if not m:
            raise RuntimeError("hub did not start: %r" % line)
        self.addr = ("127.0.0.1", int(m.group(1)))
        self.socks = []

    def client(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", 0))
        s.settimeout(0.5)
        self.socks.append(s)
        return s

    def send(self, s, data):
        s.sendto(data, self.addr)

    def errtext(self):
        self.err.flush()
        with open(self.errpath) as f:
            return f.read()

    def stats(self):
        """SIGUSR1 -> parse the newest 'ethhub stats' line."""
        def lines():
            self.err.flush()
            with open(self.errpath) as f:
                return [l for l in f if l.startswith("ethhub stats")]
        n = len(lines())
        self.p.send_signal(signal.SIGUSR1)
        for _ in range(100):
            ls = lines()
            if len(ls) > n:
                return {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", ls[-1])}
            time.sleep(0.05)
        raise RuntimeError("no stats")

    def stop(self):
        for s in self.socks:
            s.close()
        self.p.send_signal(signal.SIGTERM)
        rc = self.p.wait(timeout=10)
        self.err.seek(0)
        text = self.err.read()
        self.err.close()
        self.p.stdout.close()
        self.dir.cleanup()
        return rc, text


def recv(s, t=0.5):
    s.settimeout(t)
    try:
        return s.recvfrom(65535)[0]
    except socket.timeout:
        return None


def drain(*socks):
    for s in socks:
        s.setblocking(False)
        try:
            while True:
                s.recv(65535)
        except (BlockingIOError, OSError):
            pass


def settle(*socks):
    """Wait until the hub has processed everything sent so far (loopback FIFO)."""
    time.sleep(0.15)


class HubBase(unittest.TestCase):
    def start(self, *args, **kw):
        self.hub = Hub(*args, **kw)
        self.addCleanup(self.finish)
        return self.hub

    def finish(self):
        if self.hub:
            rc, text = self.hub.stop()
            self.hub = None
            self.assertEqual(rc, 0, text)
            self.assertNotIn("Sanitizer", text)
            self.assertNotIn("runtime error", text)

    hub = None

    def three(self, *args):
        h = self.start(*args)
        a, b, c = h.client(), h.client(), h.client()
        for s in (a, b, c):
            h.send(s, b"")
        settle()
        return h, a, b, c


class HubTest(HubBase):
    def test_hello_registers_without_traffic(self):
        h, a, b, c = self.three()
        st = h.stats()
        self.assertEqual(st["clients"], 3)
        self.assertEqual(st["hellos"], 3)
        self.assertEqual(st["rx_frames"], 0)
        # a registered-but-silent client receives floods
        h.send(a, frame(BCAST, mac(1)))
        self.assertIsNotNone(recv(b))
        self.assertIsNotNone(recv(c))

    def test_broadcast_floods_not_to_sender(self):
        h, a, b, c = self.three()
        f = frame(BCAST, mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(a, 0.3))

    def test_multicast_floods(self):
        h, a, b, c = self.three()
        f = frame(b"\x01\x00\x5e\x00\x00\x01", mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(a, 0.3))

    def test_unicast_to_learned_mac(self):
        h, a, b, c = self.three()
        h.send(b, frame(BCAST, mac(2)))  # teach hub: mac2 is behind b
        recv(a), recv(c)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))
        self.assertIsNone(recv(a, 0.3))
        self.assertEqual(h.stats()["unicast"], 1)

    def test_unknown_unicast_floods(self):
        h, a, b, c = self.three()
        f = frame(mac(9), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(a, 0.3))
        self.assertEqual(h.stats()["unknown_unicast"], 1)

    def test_mac_moves_between_clients(self):
        h, a, b, c = self.three("--mac-move=allow")
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        h.send(c, frame(BCAST, mac(2)))  # mac2 moved to c
        recv(a), recv(b)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(b, 0.3))

    def test_dst_on_same_port_not_echoed(self):
        h, a, b, c = self.three()
        h.send(a, frame(BCAST, mac(1)))
        recv(b), recv(c)
        h.send(a, frame(mac(1), mac(7)))  # dst learned on the sender's own port
        self.assertIsNone(recv(a, 0.3))
        self.assertIsNone(recv(b, 0.3))

    def test_size_limits(self):
        h, a, b, c = self.three()
        for n in (1, 5, 13):
            h.send(a, b"\xff" * n)
        h.send(a, frame(BCAST, mac(1), b"y" * (1515 - 14)))  # 1515: too long
        h.send(a, b"\xff" * 9000)
        self.assertIsNone(recv(b, 0.4))
        # boundaries are accepted
        small = frame(BCAST, mac(1), b"")  # 14
        big = frame(BCAST, mac(1), b"z" * (1514 - 14))
        h.send(a, small)
        h.send(a, big)
        self.assertEqual(recv(b), small)
        self.assertEqual(recv(b), big)
        st = h.stats()
        self.assertEqual(st["dropped_short"], 3)
        self.assertEqual(st["dropped_long"], 2)

    def test_client_ageing(self):
        h, a, b, c = self.three("--client-timeout", "2")
        t0 = time.time()
        while time.time() - t0 < 3.6:  # keep a alive, let b and c expire
            h.send(a, b"")
            time.sleep(0.4)
        st = h.stats()
        self.assertEqual(st["clients"], 1)
        self.assertEqual(st["clients_expired"], 2)
        h.send(c, b"")  # c returns
        settle()
        f = frame(BCAST, mac(1))
        h.send(a, f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(b, 0.3))

    def test_mac_ageing(self):
        h, a, b, c = self.three("--mac-timeout", "2")
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))
        time.sleep(3.5)
        h.send(a, f)  # mac2 forgotten: floods
        self.assertEqual(recv(b), f)
        self.assertEqual(recv(c), f)
        self.assertGreaterEqual(h.stats()["macs_expired"], 1)

    def test_client_table_bound(self):
        h = self.start("--max-clients", "2")
        a, b, c = h.client(), h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        h.send(c, b"")
        settle()
        h.send(c, frame(BCAST, mac(3)))  # unregistered: ignored
        f = frame(BCAST, mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))
        self.assertIsNone(recv(a, 0.3))
        st = h.stats()
        self.assertEqual(st["clients"], 2)
        self.assertEqual(st["dropped_full"], 2)

    def test_mac_table_bound(self):
        h = self.start("--max-macs", "3", "--max-macs-per-client", "0")
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        h.send(b, frame(BCAST, mac(1)))  # b's own entry: live, kept (see MAC pinning)
        recv(a)
        for i in range(10, 20):
            h.send(a, frame(BCAST, mac(i)))
            recv(b)
        st = h.stats()
        self.assertEqual(st["macs"], 3)
        self.assertEqual(st["macs_evicted"], 8)
        # newest learned entry still unicasts
        f = frame(mac(19), mac(1))
        h.send(b, f)
        self.assertEqual(recv(a), f)

    def test_pcap(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        path = os.path.join(d.name, "x.pcap")
        h = self.start("--pcap", path)
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        f1, f2 = frame(BCAST, mac(1)), frame(mac(1), mac(2), b"q" * 100)
        h.send(a, f1)
        recv(b)
        h.send(b, f2)
        recv(a)
        h.stats()
        with open(path, "rb") as fh:
            data = fh.read()
        magic, vmaj, vmin, _, _, snap, link = struct.unpack("<IHHiIII", data[:24])
        self.assertEqual((magic, vmaj, vmin, link), (0xA1B2C3D4, 2, 4, 1))
        off, got = 24, []
        while off < len(data):
            _, _, caplen, origlen = struct.unpack("<IIII", data[off:off + 16])
            self.assertEqual(caplen, origlen)
            got.append(data[off + 16:off + 16 + caplen])
            off += 16 + caplen
        self.assertEqual(got, [f1, f2])

    def test_fuzz_does_not_crash(self):
        h, a, b, c = self.three("--max-clients", "8", "--max-macs", "16", "--max-macs-per-client", "2", "--verbose")
        rng = random.Random(12345)
        socks = [h.client() for _ in range(6)]
        for s in socks:
            s.setblocking(False)
        sent = 0
        for i in range(6000):
            r = rng.random()
            if r < 0.1:
                n = rng.randrange(0, 20)
            elif r < 0.15:
                n = rng.randrange(1500, 9000)
            else:
                n = rng.randrange(14, 1515)
            d = bytearray(rng.randbytes(n))
            if n >= 14 and rng.random() < 0.5:
                d[0:6] = rng.choice([BCAST, mac(rng.randrange(8)), b"\x01\x00\x5e" + d[3:6]])
                d[6:12] = mac(rng.randrange(1, 12))
            try:
                rng.choice(socks).sendto(bytes(d), h.addr)
                sent += 1
            except (BlockingIOError, OSError):
                pass
            if i % 200 == 0:  # drain so socket buffers don't clog the test
                drain(*socks, a, b, c)
                time.sleep(0.02)
        self.assertGreater(sent, 5000)
        # still alive and sane
        time.sleep(0.2)
        drain(a, b, c)
        f = frame(BCAST, mac(100))  # a MAC the fuzz never used (mac-move=deny is the default)
        h.send(a, f)
        got = None
        deadline = time.time() + 15
        while time.time() < deadline:  # skip any fuzz backlog still draining
            got = recv(b, 0.3)
            if got == f:
                break
        self.assertEqual(got, f)
        st = h.stats()
        self.assertLessEqual(st["clients"], 8)
        self.assertLessEqual(st["macs"], 16)

    def test_default_listen_address_is_loopback(self):
        # no address in --listen: must not fall back to 0.0.0.0
        h = self.start(listen="0")
        text = h.errtext()
        self.assertRegex(text, r"bound to 127\.0\.0\.1:\d+")
        self.assertNotIn("WARNING", text)

    def test_non_loopback_listen_warns(self):
        h = self.start("--allow", "127.0.0.0/8", listen="0.0.0.0:0")
        text = h.errtext()
        self.assertIn("WARNING", text)
        self.assertNotIn("Use --allow", text)  # already restricted
        c = h.client()
        h.send(c, b"")
        settle()
        self.assertEqual(h.stats()["clients"], 1)

    def test_allow_restricts_who_can_join(self):
        h = self.start("--allow", "10.0.0.0/8", "--allow", "192.168.0.0/16")
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, frame(BCAST, mac(1)))
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 0)
        self.assertEqual(st["dropped_denied"], 2)
        self.assertEqual(st["rx_frames"], 0)
        self.assertIsNone(recv(a, 0.2))

    def test_allow_admits_matching_sources(self):
        h = self.start("--allow", "10.0.0.0/8", "--allow", "127.0.0.1")
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 2)
        self.assertEqual(st["dropped_denied"], 0)

    def test_uplink_joins_other_hub(self):
        h1 = self.start()
        a = h1.client()
        h1.send(a, b"")
        h2 = Hub("--connect", "127.0.0.1:%d" % h1.addr[1], "--client-timeout", "2")
        self.addCleanup(lambda: self.assertEqual(h2.stop()[0], 0))
        b = h2.client()
        h2.send(b, b"")
        time.sleep(1.0)  # uplink hello registers h2 at h1
        f = frame(BCAST, mac(1))
        h1.send(a, f)
        self.assertEqual(recv(b), f)
        g = frame(mac(1), mac(2))  # reply is learned through the uplink: unicast
        h2.send(b, g)
        self.assertEqual(recv(a), g)
        # the pinned uplink survives longer than h2's client timeout
        time.sleep(3.5)
        h2.send(b, b"")
        settle()
        h1.send(a, f)
        self.assertEqual(recv(b), f)

    def test_bad_args(self):
        for args in (["--listen", "x:99999"], ["--max-clients", "0"], ["--bogus"],
                     ["--allow", "nonsense"], ["--allow", "10.0.0.0/33"], ["--allow", "10.0.0.0/"],
                     ["--max-macs", "4097"]):
            r = subprocess.run([HUB, *args], capture_output=True, timeout=10, check=False)
            self.assertEqual(r.returncode, 2, args)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def key_hello(key, t=None):
    """The 13-byte keyed hello (see ethhub.c / docs/features/ETHHUB.md)."""
    t = int(time.time()) if t is None else t
    hdr = b"\xe7" + struct.pack(">I", t & 0xFFFFFFFF)
    return hdr + hmac.new(key, b"ethhub-hello-v1" + hdr, hashlib.sha256).digest()[:8]


KEY = b"correct horse battery staple"


class HardeningTest(HubBase):
    """Hash tables, MAC pinning, rate limits, flood cap, client eviction, auth."""

    def keyfile(self, key=KEY, mode=0o600):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        path = os.path.join(d.name, "key")
        with open(path, "wb") as f:
            f.write(key)
        os.chmod(path, mode)
        return path

    def many(self, n, *args):
        h = self.start(*args)
        socks = [h.client() for _ in range(n)]
        for s in socks:
            h.send(s, b"")
        settle()
        return (h, *socks)

    # -- hash table ------------------------------------------------------------

    def test_mac_table_large_and_lru(self):
        h = self.start("--max-macs", "4096", "--max-macs-per-client", "0", "--no-rate")
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        h.send(b, frame(BCAST, b"\x02\x01\x00\x00\x00\x01"))  # b's entry: live, so it is never evicted for a's
        for i in range(5000):
            h.send(a, frame(BCAST, b"\x02\x00\x00\x00" + struct.pack(">H", i)))
            if i % 100 == 0:
                drain(b)
        settle()
        drain(a, b)
        st = h.stats()
        self.assertEqual(st["macs"], 4096)
        self.assertEqual(st["macs_evicted"], 5001 - 4096)
        newest = b"\x02\x00\x00\x00" + struct.pack(">H", 4999)
        f = frame(newest, mac(1))
        h.send(b, f)
        self.assertEqual(recv(a), f)
        oldest = b"\x02\x00\x00\x00" + struct.pack(">H", 0)  # evicted: floods
        h.send(b, frame(oldest, mac(1)))
        self.assertEqual(h.stats()["unknown_unicast"] >= 1, True)

    def test_mac_eviction_is_least_recently_seen(self):
        h, a, b, c = self.three("--max-macs", "4", "--max-macs-per-client", "0", "--no-rate")
        for i in (10, 11, 12):
            h.send(a, frame(BCAST, mac(i)))
        h.send(b, frame(BCAST, mac(1)))  # the table is now full
        settle()
        h.send(a, frame(BCAST, mac(10)))  # refresh 10: 11 is now the oldest
        h.send(a, frame(BCAST, mac(13)))  # evicts 11
        settle()
        drain(a, b, c)
        f = frame(mac(10), mac(1))
        h.send(b, f)
        self.assertEqual(recv(a), f)
        self.assertIsNone(recv(c, 0.3))  # 10 survived, unicast
        g = frame(mac(11), mac(1))
        h.send(b, g)
        self.assertEqual(recv(c), g)  # 11 was evicted: flooded

    # -- MAC pinning -------------------------------------------------------------

    def test_mac_pinned_to_live_owner(self):
        h, a, b, c = self.three()
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        h.send(c, frame(BCAST, mac(2)))  # c tries to take over mac2
        self.assertIsNone(recv(a, 0.3))
        self.assertIsNone(recv(b, 0.3))  # the spoofed frame is dropped, not forwarded
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)  # still unicast to the owner
        self.assertIsNone(recv(c, 0.3))
        self.assertEqual(h.stats()["dropped_macmove"], 1)

    def test_mac_move_allowed_after_owner_goes_quiet(self):
        h, a, b, c = self.three("--live-window", "1")
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        time.sleep(2.2)  # b silent: not live any more
        h.send(c, b"")
        h.send(c, frame(BCAST, mac(2)))
        recv(a), recv(b)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(b, 0.3))
        self.assertEqual(h.stats()["dropped_macmove"], 0)

    def test_mac_stays_pinned_while_owner_sends_keepalives(self):
        h, a, b, c = self.three("--live-window", "2")
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        t0 = time.time()
        while time.time() - t0 < 3.2:  # b only says hello
            h.send(b, b"")
            time.sleep(0.4)
        h.send(c, frame(BCAST, mac(2)))
        settle()
        self.assertEqual(h.stats()["dropped_macmove"], 1)

    def test_mac_move_allow_option_forms(self):
        for args in (["--mac-move", "allow"], ["--mac-move=allow"]):
            h, a, b, c = self.three(*args)
            h.send(b, frame(BCAST, mac(2)))
            recv(a), recv(c)
            h.send(c, frame(BCAST, mac(2)))
            settle()
            self.assertEqual(h.stats()["dropped_macmove"], 0)
            self.finish()

    # -- rate limits ---------------------------------------------------------------

    def test_frame_rate_limit(self):
        h, a, b, c = self.three("--rate", "20")
        for _ in range(300):
            h.send(a, frame(mac(7), mac(1)))
        settle()
        st = h.stats()
        self.assertGreater(st["dropped_rate"], 250)
        self.assertLess(st["rx_frames"], 50)
        self.assertEqual(st["rx_frames"] + st["dropped_rate"], 300)
        # another client is not affected
        f = frame(BCAST, mac(3))
        h.send(b, f)
        self.assertEqual(recv(a), f)

    def test_byte_rate_limit(self):
        h, a, b, c = self.three("--rate", "100000,2000")
        for _ in range(10):
            h.send(a, frame(mac(7), mac(1), b"p" * 986))  # 1000 bytes
        settle()
        st = h.stats()
        self.assertLessEqual(st["rx_frames"], 3)
        self.assertGreaterEqual(st["dropped_rate"], 7)

    def test_no_rate(self):
        h, a, b, c = self.three("--rate", "10", "--no-rate", "--flood-rate", "1000")
        for _ in range(400):
            h.send(a, frame(BCAST, mac(1)))
        settle()
        st = h.stats()
        self.assertEqual(st["dropped_rate"], 0)
        self.assertEqual(st["dropped_flood"], 0)
        self.assertEqual(st["rx_frames"], 400)

    def test_default_rate_is_generous(self):
        h, a, b, c = self.three()
        for _ in range(500):
            h.send(a, frame(BCAST, mac(1), b"d" * 1000))
        settle()
        self.assertEqual(h.stats()["dropped_rate"], 0)

    def test_flood_cap_per_client(self):
        h = self.start("--flood-rate", "100000")  # depth 50 KB
        socks = [h.client() for _ in range(10)]
        for s in socks:
            h.send(s, b"")
        settle()
        big = frame(BCAST, mac(1), b"b" * 1500)  # x9 copies = 13.6 KB per flood
        for _ in range(20):
            h.send(socks[0], big)
        settle()
        st = h.stats()
        self.assertGreaterEqual(st["dropped_flood"], 14)
        self.assertLessEqual(st["tx_frames"], 6 * 9)
        # another client can still flood; unicast of the first client is not charged
        h.send(socks[1], frame(BCAST, mac(2)))
        self.assertIsNotNone(recv(socks[2]))

    def test_flood_cap_global(self):
        h = self.start("--flood-rate", "100000")  # per client 50 KB, hub 200 KB
        socks = [h.client() for _ in range(10)]
        for s in socks:
            h.send(s, b"")
        settle()
        for k, s in enumerate(socks):
            for _ in range(3):  # each client stays inside its own budget
                h.send(s, frame(BCAST, mac(k + 1), b"b" * 1500))
        settle()
        st = h.stats()
        self.assertGreaterEqual(st["dropped_flood"], 3)
        self.assertLess(st["tx_frames"], 30 * 9)

    def test_flood_cap_off(self):
        h = self.start("--flood-rate", "0")
        socks = [h.client() for _ in range(10)]
        for s in socks:
            h.send(s, b"")
        settle()
        for _ in range(30):
            h.send(socks[0], frame(BCAST, mac(1), b"b" * 1500))
        settle()
        self.assertEqual(h.stats()["dropped_flood"], 0)

    # -- client table ---------------------------------------------------------------

    def test_full_table_evicts_idle_client(self):
        h = self.start("--max-clients", "2", "--live-window", "1")
        a, b, c = h.client(), h.client(), h.client()
        h.send(a, frame(BCAST, mac(1)))
        h.send(b, b"")
        time.sleep(2.2)
        h.send(b, b"")  # b is live, a idle: a is the victim
        h.send(c, b"")
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 2)
        self.assertEqual(st["clients_evicted"], 1)
        self.assertEqual(st["dropped_full"], 0)
        f = frame(BCAST, mac(5))
        h.send(b, f)
        self.assertEqual(recv(c), f)
        self.assertIsNone(recv(a, 0.3))
        h.send(c, frame(mac(1), mac(6)))  # a's MAC was purged with it: floods to b
        self.assertIsNotNone(recv(b))

    def test_full_table_keeps_live_clients(self):
        h = self.start("--max-clients", "2")  # default live window 15 s
        a, b, c = h.client(), h.client(), h.client()
        for s in (a, b, c):
            h.send(s, b"")
        settle()
        st = h.stats()
        self.assertEqual((st["clients"], st["dropped_full"], st["clients_evicted"]), (2, 1, 0))

    def test_per_ip_client_cap(self):
        h = self.start("--max-per-ip", "2", "--max-clients", "10")
        a, b, c = h.client(), h.client(), h.client()
        for s in (a, b, c):
            h.send(s, b"")
        h.send(c, frame(BCAST, mac(3)))
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 2)
        self.assertEqual(st["dropped_ip_cap"], 2)
        self.assertEqual(st["rx_frames"], 0)

    def test_per_ip_cap_zero_is_unlimited(self):
        h, *socks = self.many(20, "--max-per-ip", "0", "--max-clients", "30")
        self.assertEqual(h.stats()["clients"], 20)

    # -- authentication -----------------------------------------------------------------

    def test_key_file_must_be_private_and_long_enough(self):
        for kf in (self.keyfile(mode=0o644), self.keyfile(mode=0o640), self.keyfile(b"short"),
                   self.keyfile(b"\n\n\n"), "/nonexistent/key"):
            r = subprocess.run([HUB, "--key-file", kf], capture_output=True, timeout=10, check=False)
            self.assertEqual(r.returncode, 2, kf)
        # a trailing newline is not part of the key
        h = self.start("--key-file=" + self.keyfile(KEY + b"\n"))
        a = h.client()
        h.send(a, key_hello(KEY))
        settle()
        self.assertEqual(h.stats()["clients"], 1)
        self.assertIn("authentication on", h.errtext())

    def test_keyed_hub_drops_unauthenticated_sources(self):
        h = self.start("--key-file", self.keyfile())
        a, b = h.client(), h.client()
        h.send(a, key_hello(KEY))
        h.send(b, b"")  # an empty hello does not register
        h.send(b, frame(BCAST, mac(2)))  # nor does a frame
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 1)
        self.assertEqual(st["dropped_unauth"], 2)
        self.assertEqual(st["rx_frames"], 0)
        self.assertEqual(st["hellos"], 1)
        # b cannot read the LAN's broadcasts either
        h.send(a, frame(BCAST, mac(1)))
        self.assertIsNone(recv(b, 0.3))

    def test_keyed_hello_rejects_wrong_key_and_stale_time(self):
        h = self.start("--key-file", self.keyfile(), "--auth-window", "30")
        s = h.client()
        now = int(time.time())
        bad = [key_hello(b"another key, same length!"), key_hello(KEY, now - 600), key_hello(KEY, now + 600),
               key_hello(KEY)[:-1] + b"\x00" if key_hello(KEY)[-1] else key_hello(KEY)[:-1] + b"\x01"]
        for d in bad:
            h.send(s, d)
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 0)
        self.assertEqual(st["dropped_unauth"], 4)
        h.send(s, key_hello(KEY, now - 20))  # inside the window
        settle()
        self.assertEqual(h.stats()["clients"], 1)

    def test_keyed_clients_talk_and_keep_alive(self):
        h = self.start("--key-file", self.keyfile(), "--client-timeout", "3")
        a, b = h.client(), h.client()
        h.send(a, key_hello(KEY))
        h.send(b, key_hello(KEY))
        settle()
        drain(a, b)  # the hub acknowledges a keyed hello with an empty datagram
        f = frame(BCAST, mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        g = frame(mac(1), mac(2))
        h.send(b, g)
        self.assertEqual(recv(a), g)
        t0 = time.time()
        while time.time() - t0 < 4.5:  # fresh keyed hellos keep the client alive
            h.send(a, key_hello(KEY))  # (a repeat within the same second is a replay, ignored)
            time.sleep(0.4)
        st = h.stats()
        self.assertEqual(st["clients"], 1)
        # b expired: its datagrams are unauthenticated again
        h.send(b, frame(BCAST, mac(2)))
        settle()
        self.assertEqual(h.stats()["dropped_unauth"], 1)

    def test_unkeyed_hub_is_unchanged_by_keyed_hello(self):
        h = self.start()
        a = h.client()
        h.send(a, key_hello(KEY))  # 13 bytes: too short for a frame, as before
        h.send(a, b"")
        settle()
        st = h.stats()
        self.assertEqual(st["dropped_short"], 1)
        self.assertEqual(st["hellos"], 1)

    def test_keyed_uplink(self):
        kf = self.keyfile()
        h1 = self.start("--key-file", kf)
        a = h1.client()
        h1.send(a, key_hello(KEY))
        h2 = Hub("--connect", "127.0.0.1:%d" % h1.addr[1], "--key-file", kf)
        self.addCleanup(lambda: self.assertEqual(h2.stop()[0], 0))
        b = h2.client()
        h2.send(b, key_hello(KEY))
        time.sleep(1.0)
        drain(a, b)
        f = frame(BCAST, mac(1))
        h1.send(a, f)
        self.assertEqual(recv(b), f)
        g = frame(mac(1), mac(2))
        h2.send(b, g)
        self.assertEqual(recv(a), g)

    def test_keyed_hub_still_checks_the_allow_list(self):
        h = self.start("--key-file", self.keyfile(), "--allow", "10.0.0.0/8")
        a = h.client()
        h.send(a, key_hello(KEY))
        settle()
        st = h.stats()
        self.assertEqual((st["clients"], st["dropped_denied"]), (0, 1))

    # -- review round: pinning bypass, trunks, flood fairness, replay ---------------

    def test_mac_table_pressure_cannot_evict_live_owner(self):
        h, a, b, c = self.three("--max-macs", "8", "--max-macs-per-client", "0", "--no-rate")
        for i in (2, 3, 4, 5):
            h.send(b, frame(BCAST, mac(i)))
            h.send(a, frame(BCAST, mac(i + 10)))
        settle()
        for i in range(20, 50):  # c tries to push the others' MACs out of the full table
            h.send(c, frame(BCAST, mac(i)))
        settle()
        drain(a, b, c)
        st = h.stats()
        self.assertEqual(st["macs"], 8)
        self.assertGreater(st["dropped_macfull"], 20)
        h.send(c, frame(BCAST, mac(2)))  # nor can it take mac2 afterwards
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))
        self.assertGreaterEqual(h.stats()["dropped_macmove"], 1)

    def test_per_client_mac_cap_evicts_only_its_own(self):
        h, a, b, c = self.three("--max-macs", "16", "--no-rate")
        h.send(b, frame(BCAST, mac(2)))
        for i in range(20, 50):
            h.send(c, frame(BCAST, mac(i)))
        settle()
        drain(a, b, c)
        st = h.stats()
        self.assertEqual(st["macs"], 1 + 8)  # default cap 8 per client
        self.assertEqual(st["dropped_macfull"], 0)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))
        g = frame(mac(49), mac(1))  # c's newest is kept
        h.send(a, g)
        self.assertEqual(recv(c), g)

    def test_per_client_mac_cap_option(self):
        h, a, b, c = self.three("--max-macs-per-client", "2", "--no-rate")
        for i in (20, 21, 22):
            h.send(c, frame(BCAST, mac(i)))
        settle()
        self.assertEqual(h.stats()["macs"], 2)

    def test_live_owner_keepalive_keeps_mac_entry(self):
        h, a, b, c = self.three("--mac-timeout", "2")
        h.send(b, frame(BCAST, mac(2)))
        recv(a), recv(c)
        t0 = time.time()
        while time.time() - t0 < 3.6:  # b only sends hellos: its MAC must not age out
            h.send(b, b"")
            time.sleep(0.4)
        drain(a, b, c)
        f = frame(mac(2), mac(1))
        h.send(a, f)
        self.assertEqual(recv(b), f)
        self.assertIsNone(recv(c, 0.3))  # unicast, not flooded
        self.assertEqual(h.stats()["macs_expired"], 0)

    def trunk(self, h, port):
        """A socket on a fixed port standing in for a child hub that --trunk names."""
        t = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        t.bind(("127.0.0.1", port))
        t.settimeout(0.5)
        self.addCleanup(t.close)
        t.sendto(b"", h.addr)
        return t

    def test_trunk_can_take_mac_back_and_owns_while_quiet(self):
        p2 = free_port()
        h = self.start("--live-window", "1", "--trunk", "127.0.0.1:%d" % p2)
        a, c = h.client(), h.client()
        h.send(a, b"")
        h.send(c, b"")
        t = self.trunk(h, p2)
        h.send(a, frame(BCAST, mac(5)))  # mac5 is a's
        settle()
        drain(a, c, t)
        time.sleep(2.5)  # a has been silent for longer than --live-window: gone
        t.sendto(frame(BCAST, mac(5)), h.addr)  # the trunk takes it back (the guest moved)
        settle()
        drain(a, c, t)
        f = frame(mac(5), mac(9))
        h.send(c, f)
        self.assertEqual(recv(t), f)
        self.assertIsNone(recv(a, 0.3))
        # and a MAC behind the trunk stays pinned to it while the trunk is quiet
        t.sendto(frame(BCAST, mac(6)), h.addr)
        settle()
        time.sleep(2.5)  # longer than --live-window
        h.send(a, frame(BCAST, mac(6)))
        settle()
        self.assertEqual(h.stats()["dropped_macmove"], 1)

    def test_flood_budget_heavy_senders_cannot_starve_light_ones(self):
        h = self.start("--flood-rate", "100000")  # per client 50 KB, hub 200 KB
        socks = [h.client() for _ in range(12)]
        for s in socks:
            h.send(s, b"")
        settle()
        for k in range(10):  # ten clients flood hard
            for _ in range(3):
                h.send(socks[k], frame(BCAST, mac(k + 1), b"b" * 1500))
        settle()
        st = h.stats()
        self.assertGreaterEqual(st["dropped_flood"], 3)
        drain(*socks)
        f = frame(BCAST, mac(100))  # a quiet client's ARP still gets out
        h.send(socks[10], f)
        self.assertEqual(recv(socks[11]), f)

    def test_trunk_is_not_rate_limited(self):
        p2 = free_port()
        h = self.start("--rate", "20", "--flood-rate", "20000", "--max-macs-per-client", "2",
                       "--trunk", "127.0.0.1:%d" % p2)
        a = h.client()
        h.send(a, b"")
        t = self.trunk(h, p2)
        for i in range(150):
            t.sendto(frame(BCAST, mac(20 + i % 40), b"u" * 1000), h.addr)  # also many MACs
        time.sleep(0.5)
        st = h.stats()
        self.assertEqual(st["dropped_rate"], 0)
        self.assertEqual(st["dropped_flood"], 0)
        self.assertEqual(st["rx_frames"], 150)
        self.assertEqual(st["macs"], 40)  # the per-client MAC cap does not apply either

    def test_untrusted_child_hub_is_limited_like_a_client(self):
        h1 = self.start("--rate", "20")  # no --trunk: the child hub is one client
        a = h1.client()
        h1.send(a, b"")
        h2 = Hub("--connect", "127.0.0.1:%d" % h1.addr[1])
        self.addCleanup(lambda: self.assertEqual(h2.stop()[0], 0))
        b = h2.client()
        h2.send(b, b"")
        time.sleep(1.2)
        for _ in range(150):
            h2.send(b, frame(BCAST, mac(7)))
        time.sleep(0.5)
        self.assertGreater(h1.stats()["dropped_rate"], 100)

    def test_loopback_is_exempt_from_default_per_ip_cap(self):
        h, *socks = self.many(20, "--max-clients", "30")
        self.assertEqual(h.stats()["clients"], 20)

    def test_per_ip_refusal_is_logged(self):
        h = self.start("--max-per-ip", "1")
        a, b = h.client(), h.client()
        h.send(a, b"")
        h.send(b, b"")
        settle()
        self.assertIn("--max-per-ip", h.errtext())

    # -- review round: keyed hub ------------------------------------------------------

    def test_keyed_hello_is_acknowledged_with_an_empty_datagram(self):
        h = self.start("--key-file", self.keyfile())
        a = h.client()
        h.send(a, key_hello(b"x" * 20))  # wrong key: no answer
        self.assertIsNone(recv(a, 0.3))
        h.send(a, key_hello(KEY))
        self.assertEqual(recv(a), b"")
        plain = self.start()  # an unkeyed hub never answers an empty hello (compat)
        b = plain.client()
        plain.send(b, b"")
        self.assertIsNone(recv(b, 0.3))

    def test_keyed_hello_replay_is_refused(self):
        h = self.start("--key-file", self.keyfile())
        a = h.client()
        now = int(time.time())
        h.send(a, key_hello(KEY, now))
        h.send(a, key_hello(KEY, now))  # same hello again
        h.send(a, key_hello(KEY, now - 5))  # older
        settle()
        st = h.stats()
        self.assertEqual(st["dropped_replay"], 2)
        self.assertEqual(st["hellos"], 1)
        h.send(a, key_hello(KEY, now + 1))  # newer: fine
        settle()
        self.assertEqual(h.stats()["hellos"], 2)

    def test_keyed_membership_needs_fresh_hellos(self):
        h = self.start("--key-file", self.keyfile(), "--client-timeout", "2")
        a, b = h.client(), h.client()
        h.send(a, key_hello(KEY))
        h.send(b, key_hello(KEY))
        settle()
        t0 = time.time()
        while time.time() - t0 < 3.6:  # frames alone must not keep a client alive
            h.send(a, frame(BCAST, mac(1)))
            time.sleep(0.3)
        st = h.stats()
        self.assertEqual(st["clients"], 0)
        self.assertGreater(st["dropped_unauth"], 0)

    def test_keyed_uplink_hint_when_peer_has_no_key(self):
        h1 = self.start()
        h2 = Hub("--connect", "127.0.0.1:%d" % h1.addr[1], "--key-file", self.keyfile(), "--hint-after", "2")
        self.addCleanup(lambda: self.assertEqual(h2.stop()[0], 0))
        time.sleep(3.6)
        self.assertEqual(h2.errtext().count("no traffic from the uplink"), 1)

    def test_keyed_uplink_no_hint_when_both_have_the_key(self):
        kf = self.keyfile()
        h1 = self.start("--key-file", kf)
        h2 = Hub("--connect", "127.0.0.1:%d" % h1.addr[1], "--key-file", kf, "--hint-after", "2")
        self.addCleanup(lambda: self.assertEqual(h2.stop()[0], 0))
        time.sleep(3.6)
        self.assertNotIn("no traffic from the uplink", h2.errtext())

    def test_key_file_must_be_a_regular_file(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        fifo = os.path.join(d.name, "fifo")
        os.mkfifo(fifo, 0o600)
        r = subprocess.run([HUB, "--key-file", fifo], capture_output=True, timeout=10, check=False)
        self.assertEqual(r.returncode, 2)  # and it did not block opening it

    def test_key_of_one_repeated_byte_is_warned_about(self):
        h = self.start("--key-file", self.keyfile(b"a" * 20))
        self.assertIn("WARNING", h.errtext())
        self.assertIn("same byte", h.errtext())

    def test_bad_new_args_review(self):
        for args in (["--max-macs-per-client", "-1"], ["--hint-after", "0"], ["--trunk", "x"], ["--trunk", "1.2.3.4:0"], ["--max-per-hello", "0"], ["--max-macs-per-trunk", "-1"]):
            r = subprocess.run([HUB, *args], capture_output=True, timeout=10, check=False)
            self.assertEqual(r.returncode, 2, args)


    # -- second review round ------------------------------------------------------------

    def test_trunk_cannot_take_a_live_guests_mac(self):
        p2 = free_port()
        h = self.start("--trunk", "127.0.0.1:%d" % p2)
        a, c = h.client(), h.client()
        h.send(a, frame(BCAST, mac(5)))
        h.send(c, b"")
        t = self.trunk(h, p2)
        settle()
        drain(a, c, t)
        t.sendto(frame(BCAST, mac(5)), h.addr)  # a spoofed or hostile trunk
        settle()
        self.assertEqual(h.stats()["dropped_macmove"], 1)
        f = frame(mac(5), mac(9))
        h.send(c, f)
        self.assertEqual(recv(a), f)
        self.assertIsNone(recv(t, 0.3))
        h.send(a, frame(BCAST, mac(5)))  # and a is not locked out
        settle()
        self.assertEqual(h.stats()["dropped_macmove"], 1)

    def test_trunk_mac_flood_cannot_evict_guests(self):
        p2 = free_port()
        h = self.start("--max-macs", "64", "--max-macs-per-trunk", "16", "--trunk", "127.0.0.1:%d" % p2)
        a, c = h.client(), h.client()
        h.send(a, b"")
        h.send(c, b"")
        for i in (2, 3, 4):
            h.send(a, frame(BCAST, mac(i)))
        t = self.trunk(h, p2)
        settle()
        for i in range(300):  # macof from the TAP side
            t.sendto(frame(BCAST, b"\x02\x09\x00\x00" + struct.pack(">H", i)), h.addr)
        time.sleep(0.5)
        drain(a, c, t)
        st = h.stats()
        self.assertEqual(st["macs"], 3 + 16)
        self.assertEqual(st["dropped_macfull"], 0)
        f = frame(mac(3), mac(9))
        h.send(c, f)
        self.assertEqual(recv(a), f)
        self.assertIsNone(recv(t, 0.3))

    def test_guests_can_evict_quiet_trunk_entries_but_not_fresh_ones(self):
        p2 = free_port()
        h = self.start("--max-macs", "8", "--max-macs-per-trunk", "8", "--live-window", "2",
                       "--trunk", "127.0.0.1:%d" % p2)
        a = h.client()
        h.send(a, b"")
        t = self.trunk(h, p2)
        for i in range(8):
            t.sendto(frame(BCAST, b"\x02\x09\x00\x00\x00" + bytes([i])), h.addr)
        settle()
        h.send(a, frame(BCAST, mac(1)))  # the table is full of fresh trunk entries
        settle()
        self.assertEqual(h.stats()["dropped_macfull"], 1)
        time.sleep(3.5)
        h.send(a, frame(BCAST, mac(1)))
        settle()
        st = h.stats()
        self.assertEqual(st["dropped_macfull"], 1)
        self.assertGreaterEqual(st["macs_evicted"], 1)

    def test_trunk_must_be_ip_and_port(self):
        for args in (["--trunk", "10.0.0.0/8"], ["--trunk", "127.0.0.1"], ["--trunk", "127.0.0.1/32"]):
            r = subprocess.run([HUB, *args], capture_output=True, timeout=10, check=False)
            self.assertEqual(r.returncode, 2, args)
            self.assertIn(b"ip:port", r.stderr)

    def test_trunk_refused_on_open_listen_without_key(self):
        r = subprocess.run([HUB, "--listen", "0.0.0.0:0", "--trunk", "10.0.0.1:5000"], capture_output=True,
                           timeout=10, check=False)
        self.assertEqual(r.returncode, 2)
        self.assertIn(b"--trunk", r.stderr)
        h = self.start("--listen", "0.0.0.0:0", "--trunk", "10.0.0.1:5000", "--allow", "127.0.0.0/8",
                       "--key-file", self.keyfile())
        self.assertIn("--trunk", h.errtext())  # allowed with a key, but warned about
        self.assertIn("WARNING", h.errtext())

    def test_flood_budget_is_always_charged_to_the_hub(self):
        h = self.start("--flood-rate", "100000")  # per client 50 KB, hub 200 KB
        socks = [h.client() for _ in range(10)]
        for s in socks:
            h.send(s, b"")
        settle()
        for k, s in enumerate(socks):
            for _ in range(3):
                h.send(s, frame(BCAST, mac(k + 1), b"b" * 1500))
        settle()
        st = h.stats()
        self.assertGreaterEqual(st["dropped_flood"], 12)
        self.assertLessEqual(st["tx_frames"], 18 * 9)

    def test_unkeyed_hub_reaches_keyed_hub_with_connect_key(self):
        kf = self.keyfile()
        k = self.start("--key-file", kf)
        a = k.client()
        k.send(a, key_hello(KEY))
        u = Hub("--connect", "127.0.0.1:%d" % k.addr[1], "--connect-key", kf)
        self.addCleanup(lambda: self.assertEqual(u.stop()[0], 0))
        b = u.client()
        u.send(b, b"")  # a plain client of the unkeyed hub (think: QEMU)
        time.sleep(1.2)
        drain(a, b)
        f = frame(BCAST, mac(1))
        k.send(a, f)
        self.assertEqual(recv(b), f)
        g = frame(mac(1), mac(2))
        u.send(b, g)
        self.assertEqual(recv(a), g)

    def test_connect_key_needs_connect_and_a_good_file(self):
        r = subprocess.run([HUB, "--connect-key", self.keyfile()], capture_output=True, timeout=10, check=False)
        self.assertEqual(r.returncode, 2)
        r = subprocess.run([HUB, "--connect", "127.0.0.1:9", "--connect-key", self.keyfile(mode=0o644)],
                           capture_output=True, timeout=10, check=False)
        self.assertEqual(r.returncode, 2)

    def test_one_sniffed_hello_cannot_make_many_clients(self):
        h = self.start("--key-file", self.keyfile(), "--max-per-hello", "4")
        socks = [h.client() for _ in range(10)]
        hello = key_hello(KEY)
        for s in socks:
            h.send(s, hello)  # the same captured hello from ten fresh source ports
        settle()
        st = h.stats()
        self.assertEqual(st["clients"], 4)
        self.assertEqual(st["dropped_replay"], 6)

    def test_clients_sharing_a_hello_second_are_fine_by_default(self):
        h = self.start("--key-file", self.keyfile())
        socks = [h.client() for _ in range(10)]
        hello = key_hello(KEY)  # ten emulators starting in the same second
        for s in socks:
            h.send(s, hello)
        settle()
        self.assertEqual(h.stats()["clients"], 10)

    def test_per_client_mac_cap_is_logged_once(self):
        h, a, b, c = self.three("--max-macs-per-client", "2", "--no-rate")
        for i in range(20, 30):
            h.send(c, frame(BCAST, mac(i)))
        settle()
        self.assertEqual(h.errtext().count("--max-macs-per-client"), 1)

    def test_key_length_rules(self):
        h = self.start("--key-file", self.keyfile(os.urandom(1024).replace(b"\n", b"x").replace(b"\r", b"x") + b"\r\n"))
        self.assertIn("authentication on", h.errtext())
        for body in (os.urandom(1025).replace(b"\n", b"x").replace(b"\r", b"x"),
                     os.urandom(1024).replace(b"\n", b"x").replace(b"\r", b"x") + b"zz"):
            rr = subprocess.run([HUB, "--key-file", self.keyfile(body)], capture_output=True, timeout=10, check=False)
            self.assertEqual(rr.returncode, 2)

    def test_bad_new_args(self):
        for args in (["--mac-move", "maybe"], ["--rate", "0"], ["--rate", "5,0"], ["--rate", "x"],
                     ["--flood-rate", "-1"], ["--max-per-ip", "-1"], ["--live-window", "0"],
                     ["--rate"]):
            r = subprocess.run([HUB, *args], capture_output=True, timeout=10, check=False)
            self.assertEqual(r.returncode, 2, args)


if __name__ == "__main__":
    HUB = os.path.abspath(sys.argv.pop(1))
    unittest.main(verbosity=2)
