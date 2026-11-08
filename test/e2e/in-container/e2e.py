#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Part of maxivmac (GPL v2, see LICENSE).
"""In-container driver of the end-to-end ladder (stdlib only; python3 -I).

usage: e2e.py RUNG [RUNG ...]       RUNG is r1 r2 r3 r3b ... (see RUNGS)

One rung is one scripted run of the System 6.0.8 guest in headless maxivmac on a
virtual LAN (ethhub, with tap0 as the container's own port), plus the checks of
that rung.  Inputs (read-only mounts): /in/rom/vMac.ROM, /in/disk/*.hfs.
Output: /out/<rung>/ (summary.json with per-check PASS/FAIL, logs, screenshots,
the hub pcap, the disk after the run) and exit status 0 only if every rung passed.
See docs/features/NETWORKING_E2E.md.
"""
import contextlib, glob, gzip, json, os, re, resource, shutil, signal, socket, struct, subprocess, sys, threading, time, zlib

ROM = os.environ.get("E2E_ROM", "/in/rom/vMac.ROM")
DISK_DIR = "/in/disk"
OUT = os.environ.get("E2E_OUT", "/out")
WORK = os.environ.get("E2E_WORK", "/work")
SCRIPTS = "/opt/e2e/net-scripts/ping"
HUB_PORT = 5555
DEBUG_PORT = 7356          # PeerTalk's UDP debug broadcast (PT_DEBUG_PORT)
OUT_CAP = 64 * 1024 * 1024  # total bytes copied to /out per run
GUEST_MAC = "02:00:00:00:00:20"
# Icon x positions in the "Net Tools" window (y = 105) of the guest disk made by
# test/e2e/make-disk.sh: the Finder lays the four applications out in name order.
ICON_X = {"BomberTalk": 110, "MacTCP Ping": 172, "test_chat": 240, "test_mesh": 305}

# guest: MacTCP manual address; tap: the container's address on the same LAN.
# peer: POSIX PeerTalk program on the tap address (None = nothing but the kernel).
RUNGS = {
    "r1":  dict(guest="10.88.0.20/24", tap="10.88.0.10/24", peer=None, app="ping",
                what="MacTCP Ping -> container kernel over the hub; container ping -> guest"),
    "r2":  dict(guest="10.88.0.20/24", tap="10.88.0.10/24", peer="test_mesh", app="test_mesh",
                what="PeerTalk discovery + one TCP connection + messages, guest 10.88.0.20/24, peer 10.88.0.10/24"),
    # The guest is the lower address, so the guest is the designated initiator and can
    # only dial after it discovered the peer by UDP broadcast: the proof that the guest
    # hears the POSIX peer (in r2 the peer dials first and the guest may learn of it
    # from the inbound connection).
    "r2b": dict(guest="10.88.0.20/24", tap="10.88.0.30/24", peer="test_mesh", app="test_mesh",
                what="as r2, but the guest is the initiator (peer 10.88.0.30/24): guest must discover the peer to dial"),
    # Both addresses are in 10.0.0.0/8 and differ before the last octet.  Read as
    # native integers, a little-endian peer ranks 10.0.2.25 above 10.88.0.20 (last
    # octet 25 > 20) while a big-endian Mac ranks it below (second octet 0 < 88):
    # the pre-253e821 disagreement.  (10.0.2.15 vs 10.88.0.20 happens to rank the
    # same way under both readings, so it cannot tell fixed from unfixed.)
    "r3":  dict(guest="10.88.0.20/8", tap="10.0.2.25/8", peer="test_mesh", app="test_mesh",
                what="byte-order case: guest 10.88.0.20/8, peer 10.0.2.25/8 (native-int ranks disagree)"),
    "r4":  dict(guest="10.88.0.20/24", tap="10.88.0.10/24", peer="bombertalk", app="BomberTalk",
                what="BomberTalk 68k in the guest lobby <-> POSIX/SDL BomberTalk: each side discovers and connects to the other"),
    # R4 plus a game: the guest presses Return in the lobby (it is the one that sends
    # MSG_GAME_START), both enter the game, and each moves and drops a bomb that the
    # other side must receive (the POSIX peer is driven through sdlkeys.so's command file).
    "r5":  dict(guest="10.88.0.20/24", tap="10.88.0.10/24", peer="bombertalk", app="BomberTalk",
                game=True,
                what="BomberTalk 68k <-> POSIX/SDL BomberTalk: start a game from the lobby, both reach the game screen, each sees the other's movement and bombs"),
    # R5 up to the game, then the round ENDS: the POSIX player (P0, the lowest rank = the
    # game-over authority) drops a bomb on its own tile and stays in the blast; the guest stays
    # at its corner.  Both must detect the game over, the authority sends MSG_GAME_OVER, both
    # run the disconnect grace period and go back to the lobby.
    "r6":  dict(guest="10.88.0.20/24", tap="10.88.0.10/24", peer="bombertalk", app="BomberTalk",
                game="end",
                what="BomberTalk 68k <-> POSIX/SDL BomberTalk: play to the END (POSIX player stands in its own blast): game over, grace period, back to the lobby on both sides"),
    "r3b": dict(guest="10.88.0.20/8", tap="10.0.2.15/8", peer="test_mesh", app="test_mesh",
                what="byte-order case as first specified: peer 10.0.2.15/8 (ranks agree under both readings)"),
}

# ---------------------------------------------------------------------------


class Run:
    def __init__(self, name):
        self.name = name
        self.cfg = RUNGS[name]
        self.checks = {}
        self.timing = {}
        self.t0 = time.time()
        self.work = os.path.join(WORK, name)
        self.shots = os.path.join(self.work, "shots")
        self.procs = []
        self.capture = []      # (t, src_ip, text)
        self.capture_lock = threading.Lock()
        self.stop = threading.Event()
        shutil.rmtree(self.work, ignore_errors=True)
        os.makedirs(self.shots)

    # -- bookkeeping ---------------------------------------------------------
    def check(self, name, ok, detail=""):
        self.checks[name] = {"ok": bool(ok), "detail": str(detail)}
        print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, detail), flush=True)
        return ok

    def mark(self, what):
        self.timing[what] = round(time.time() - self.t0, 1)

    def path(self, n):
        return os.path.join(self.work, n)

    @property
    def guest_ip(self):
        return self.cfg["guest"].split("/")[0]

    @property
    def tap_ip(self):
        return self.cfg["tap"].split("/")[0]

    # -- processes -----------------------------------------------------------
    def spawn(self, argv, outname, env=None, fsize=96 * 1024 * 1024, cwd=None):
        out = open(self.path(outname), "wb")
        e = dict(os.environ)
        e.update(env or {})

        def limits():
            resource.setrlimit(resource.RLIMIT_FSIZE, (fsize, fsize))
            os.setsid()
        p = subprocess.Popen(argv, stdout=out, stderr=subprocess.STDOUT, env=e, preexec_fn=limits, cwd=cwd)  # noqa: PLW1509 (RLIMIT_FSIZE must be set in the child; the parent has no threads running yet at spawn)
        self.procs.append(p)
        return p

    def terminate(self, p, sig=signal.SIGTERM, wait=10):
        if p.poll() is None:
            try:
                os.killpg(p.pid, sig)
            except ProcessLookupError:
                pass
            try:
                p.wait(wait)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()

    # -- the UDP debug-broadcast capture (PeerTalk "Remote Test-Log Capture") -
    def start_capture(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.bind(("0.0.0.0", DEBUG_PORT))
        s.settimeout(0.5)
        log = open(self.path("capture.log"), "w")

        def loop():
            while not self.stop.is_set():
                try:
                    data, (ip, _) = s.recvfrom(2048)
                except socket.timeout:
                    continue
                except OSError:
                    break
                t = time.time() - self.t0
                text = data.decode("latin-1").rstrip("\r\n")
                with self.capture_lock:
                    self.capture.append((t, ip, text))
                    log.write("%8.2f %s %s\n" % (t, ip, text))
                    log.flush()
            log.close()
            s.close()
        th = threading.Thread(target=loop, daemon=True)
        th.start()
        return th

    # -- the emulator script ----------------------------------------------------
    def build_script(self):
        def inc(n):
            return open(os.path.join(SCRIPTS, n + ".inc")).read()
        gen = subprocess.run(["python3", "-I", "/opt/e2e/mactcp-config.py", self.cfg["guest"]],
                             capture_output=True, text=True, check=True).stdout
        s = ""
        for b in "boot chooser cpopen ether move restart cpopen mactcp-open".split():
            s += inc(b)
        s += gen
        for b in "mactcp-close restart".split():
            s += inc(b)
        # open "Net Tools" (the disk window is open after the restart)
        folder = ("click 82 105 hold 200ms\nwait for 1s\nclick 82 105 hold 200ms\nwait for 6s\n"
                  'screenshot "n_folder"\n')
        def launch(app):   # double-click an icon of the open "Net Tools" window
            x = ICON_X[app]
            return "click %d 105 hold 200ms\nwait for 1s\nclick %d 105 hold 200ms\n" % (x, x)
        s += folder
        if self.cfg["app"] == "ping":
            s += launch("MacTCP Ping") + 'wait text "Start Ping" 60s\nwait for 3s\nscreenshot "n_ping"\n'
            s += (inc("ping").replace("10.0.2.2", self.tap_ip)
                  .replace("z1_gw_done", "E2E_PING_DONE"))
            # MacTCP answers ARP and ICMP only once an application has opened it (here
            # Ping, which stays open): the container pings the guest now.  The driver
            # ends the emulator when that is done; this wait is only a ceiling.
            s += "wait for 6000s\n"
        elif self.cfg.get("game"):
            s += self.bt_game_script(launch("BomberTalk"))
        elif self.cfg["app"] == "BomberTalk":
            # Menu -> Play (Return).  BomberTalk polls GetKeys, so the press is held; the
            # lobby starts PeerTalk discovery and shows what it finds.
            s += (launch("BomberTalk") + "wait for 160s\nscreenshot \"E2E_BT_MENU\"\n"
                  "key return hold 2s\nwait for 160s\nscreenshot \"E2E_BT_LOBBY\"\n"
                  "# stay in the lobby: discovery, auto-mesh connection (script time = 16 x guest time)\n"
                  "wait for 1600s\nscreenshot \"E2E_BT_LOBBY2\"\nwait for 800s\nscreenshot \"E2E_BT_LOBBY3\"\n")
        else:
            s += launch(self.cfg["app"])
            s += ('wait for 12s\nscreenshot "E2E_APP_STARTED"\n'
                  '# every verdict line of the test apps starts with "***"\n'
                  'wait text "***" %ds\nwait for 2s\nscreenshot "E2E_APP_VERDICT"\nwait for 15s\n'
                  % self.app_wait())
        return s

    def bt_game_script(self, launch):
        if self.cfg.get("game") == "end":
            # as R5 up to the game; the guest then just stands at its corner (13,1), far from
            # the POSIX player's blast, while the round ends and both return to the lobby
            return (launch + "wait for 160s\nscreenshot \"E2E_BT_MENU\"\n"
                    "key return hold 2s\nwait for 160s\n"
                    'wait text "Press Return to Start" 3000s\nwait for 160s\nscreenshot "E2E_BT_LOBBY"\n'
                    "key return hold 2s\nwait for 320s\nscreenshot \"E2E_GAME_1\"\n"
                    "# the POSIX player bombs itself now (driver); fuse 3 s, death 1 s, grace 1.5 s\n"
                    "wait for 640s\nscreenshot \"E2E_END_1\"\nwait for 640s\nscreenshot \"E2E_END_2\"\n"
                    "wait for 640s\nscreenshot \"E2E_END_3\"\n")
        # Menu -> lobby as in R4.  Then wait for the lobby to list the peer ("Press Return to
        # Start" is only drawn once a peer is discovered), give the TCP mesh time to form and
        # press Return: the guest is the one that sends MSG_GAME_START.  In the game it plays
        # first (the driver starts the POSIX peer when the guest's bomb arrives there): player 1
        # spawns at the top right, (13,1); (13,2) and (12,1) are free, (13,3) and (11,1) are
        # blocks.  Down one tile, drop a bomb, back up, one tile left: out of the blast.
        # Script time is emulated CPU time, 16 script seconds per second of guest clock at the
        # default speed, and a tile takes 0.2 s: hence the short holds.  `key` only queues
        # events, one after the other, so the sequence is one burst.
        return (launch + "wait for 160s\nscreenshot \"E2E_BT_MENU\"\n"
                "key return hold 2s\nwait for 160s\n"
                'wait text "Press Return to Start" 3000s\nwait for 160s\nscreenshot "E2E_BT_LOBBY"\n'
                "key return hold 2s\nwait for 320s\nscreenshot \"E2E_GAME_1\"\n"
                "# the guest's own moves and bomb\n"
                "key down hold 6s\nkey space hold 2s\nkey up hold 6s\nkey left hold 6s\n"
                "wait for 80s\nscreenshot \"E2E_GAME_2\"\n"
                "# stay in the game while the POSIX peer plays (the driver starts it on the guest's bomb)\n"
                "wait for 320s\nscreenshot \"E2E_GAME_3\"\nwait for 320s\nscreenshot \"E2E_GAME_4\"\n")

    def wait_capture(self, ip, needle, timeout):
        """Wall time (since the run began) of the first debug-broadcast line from ip containing needle."""
        end = time.time() + timeout
        while time.time() < end and not self.stop.is_set():
            with self.capture_lock:
                for t, _, text in self.capture:
                    if "@%s]" % ip in text and needle in text:
                        return t
            time.sleep(0.2)
        return None

    def press(self, *scancodes, secs):
        """Hold SDL scancodes for the POSIX peer (sdlkeys.so re-reads the file every 40 ms)."""
        open(self.path("peer-keys"), "w").write(" ".join(str(c) for c in scancodes))
        time.sleep(secs)
        open(self.path("peer-keys"), "w").write("")

    def drive_posix_peer(self):
        """R5: the POSIX peer (player 0, spawns top left at (1,1); (1,2) and (2,1) are free,
        (1,3) and (3,1) are blocks) waits for the guest's bomb to arrive, then plays: down one
        tile, bomb, back up, one tile right (out of the blast), and ends displaced from its
        corner so the guest's last screenshots show it elsewhere."""
        RIGHT, UP, DOWN, SPACE = 79, 82, 81, 44
        open(self.path("peer-keys"), "w").write("")
        if self.wait_capture(self.tap_ip, "Game started:", 900) is None:
            return
        self.mark("posix_in_game")
        if self.cfg.get("game") == "end":
            # let the guest enter the game too, then drop a bomb on the spot and stay in it
            self.wait_capture(self.guest_ip, "Game started:", 300)
            time.sleep(8.0)
            self.press(SPACE, secs=0.3)
            self.mark("posix_self_bomb")
            return
        if self.wait_capture(self.tap_ip, "RX bomb placed P1", 900) is None:
            return
        self.mark("posix_got_guest_bomb")
        time.sleep(0.3)
        self.press(DOWN, secs=0.6); time.sleep(0.2)
        self.press(SPACE, secs=0.2); time.sleep(0.2)
        self.press(UP, secs=0.6); time.sleep(0.2)
        self.press(RIGHT, secs=0.6)
        self.mark("posix_moves_done")

    def app_wait(self):
        # Script time is counted in emulated CPU time, and the guest CPU runs 16x
        # faster than its clock at the default --speed=4: the app's own 90 s window (and
        # the up to 25 s barrier wait) therefore take about 90 * 16 script seconds.
        return 3000

    # -- the rung ----------------------------------------------------------------
    def go(self):
        cfg = self.cfg
        print("== %s: %s" % (self.name, cfg["what"]), flush=True)
        # tap0 as the entrypoint made it
        ip = subprocess.run(["ip", "-4", "-o", "addr", "show", "dev", "tap0"], capture_output=True, text=True, check=False).stdout
        if not self.check("tap0_address", cfg["tap"] in ip, "want %s, have: %s" % (cfg["tap"], ip.strip() or "none")):
            return
        caps = dict(l.split(":\t") for l in open("/proc/self/status").read().splitlines() if l.startswith(("Cap", "Uid", "NoNewPrivs")))
        self.check("unprivileged", caps["Uid"].split()[0] == "2010" and int(caps["CapEff"], 16) == 0 and int(caps["CapPrm"], 16) == 0
                   and caps["NoNewPrivs"].strip() == "1",
                   "uid %s, CapPrm %s, CapEff %s, CapBnd %s, NoNewPrivs %s" % (caps["Uid"].split()[0], caps["CapPrm"], caps["CapEff"], caps["CapBnd"], caps["NoNewPrivs"].strip()))
        disks = sorted(glob.glob(DISK_DIR + "/*.hfs"))
        if not os.path.isfile(ROM) or not disks:
            self.check("inputs", False, "ROM %s / disk %s missing" % (ROM, DISK_DIR))
            return
        disk = self.path("disk.hfs")
        subprocess.run(["cp", "--sparse=always", disks[0], disk], check=True)
        os.chmod(disk, 0o600)   # the input is mounted 0400 (retro-ci stages it owner-read-only); a copy keeps that mode and the
                                # emulator then opens it write-protected ("locked startup disk" in the Chooser)

        # hub with the container's tap as one port, ingress pcap
        # E2E_HUB_KEY=1: the hub needs the shared secret and the emulator sends it (ETHHUB.md, Authentication);
        # tap0 is the hub's own pinned port and needs no key
        keyfile = None
        if os.environ.get("E2E_HUB_KEY"):
            keyfile = self.path("hub.key")
            with open(os.open(keyfile, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as kf:
                kf.write("e2e-shared-secret-%d\n" % os.getpid())
        hub = self.spawn(["ethhub", "--listen", "127.0.0.1:%d" % HUB_PORT, "--tap", "tap0",
                          "--pcap", self.path("lan.pcap")] + (["--key-file", keyfile] if keyfile else []), "hub.out")
        time.sleep(1.0)
        if not self.check("hub_up", hub.poll() is None, "ethhub on 127.0.0.1:%d + tap0" % HUB_PORT):
            return
        self.mark("hub_up")
        self.start_capture()

        peer = None
        if cfg["peer"]:
            variant = os.environ.get("E2E_PEER", "new")
            if cfg["peer"] == "bombertalk":
                # SDL2 build with the dummy video driver; the shim holds Return once, so the
                # menu moves on to the lobby (the only place PeerTalk discovery is started)
                exe = "/opt/bombertalk/BomberTalk"
                argv = [exe]
                penv = {"SDL_VIDEODRIVER": "dummy", "SDL_AUDIODRIVER": "dummy",
                        "LD_PRELOAD": "/opt/bombertalk/sdlkeys.so", "SDLKEYS_AFTER_MS": "5000"}
                if cfg.get("game"):
                    penv["SDLKEYS_CMDFILE"] = self.path("peer-keys")
            else:
                exe = "/opt/peers/%s/%s" % (variant, cfg["peer"])
                argv = [exe, "--name", "POSIX", "--peers", "0", "--secs", "3600"]
                penv = {}
            if not os.path.isfile(exe):
                self.check("peer_binary", False, exe + " missing (image built without it?)")
                return
            self.peer_variant = variant
            peer = self.spawn(argv, "peer.log", env=penv, cwd=self.work)
            self.mark("peer_started")
            if cfg.get("game"):
                threading.Thread(target=self.drive_posix_peer, daemon=True).start()

        script = self.build_script()
        open(self.path("run.dbg"), "w").write(script)
        speed = os.environ.get("E2E_SPEED", "")   # "": the emulator's default (--speed=4); see the docs
        emu = self.spawn(["stdbuf", "-oL", "-eL", "maxivmac", "--headless", "--silent", "--model=MacPlus", "--rom=" + ROM]
                         + (["--speed=" + speed] if speed else []) + ["--scsi4=daynaport", "--eth=hub:127.0.0.1:%d%s" % (HUB_PORT, ",key=" + keyfile if keyfile else ""),
                          "--eth-mac=" + GUEST_MAC, "--dbg-script=" + self.path("run.dbg"), disk],
                         "emulator.out", env={"MAXIVMAC_OUT_DIR": self.shots})
        self.mark("emulator_started")
        host_ping = None
        deadline = time.time() + int(os.environ.get("E2E_RUNG_TIMEOUT", "1500"))
        while emu.poll() is None and time.time() < deadline:
            if self.name == "r1" and host_ping is None and os.path.exists(os.path.join(self.shots, "E2E_PING_DONE.png")):
                self.mark("guest_ping_done")
                time.sleep(3)
                host_ping = subprocess.run(["ping", "-c", "5", "-i", "1", "-W", "3", self.guest_ip],
                                           capture_output=True, text=True, check=False)
                open(self.path("host-ping.txt"), "w").write(host_ping.stdout + host_ping.stderr)
                self.mark("host_ping_done")
                time.sleep(1)
                break
            time.sleep(0.5)
        timed_out = emu.poll() is None and not (self.name == "r1" and host_ping is not None)
        self.terminate(emu)
        self.mark("emulator_exit")
        time.sleep(1)
        # stop the peer (it prints its report on SIGTERM), then the hub (stats on SIGUSR1 + exit)
        if peer:
            self.terminate(peer)
        os.killpg(hub.pid, signal.SIGUSR1)
        time.sleep(0.5)
        self.stop.set()
        self.terminate(hub)
        self.mark("done")

        if self.name == "r1":     # ended by the driver after the container's ping, not by the script
            self.check("emulator_script_completed", not timed_out and os.path.exists(os.path.join(self.shots, "E2E_PING_DONE.png")),
                       "the script reached E2E_PING_DONE (every `wait` before it was met)%s" % (" (rung timeout)" if timed_out else ""))
        else:
            self.check("emulator_script_completed", (not timed_out) and emu.returncode == 0,
                       "exit %s%s" % (emu.returncode, " (rung timeout)" if timed_out else ""))
        self.analyse(host_ping)
        # the disk after the run, for the host-side PT_Log cross-check (hfsutils is not in the image)
        with open(disk, "rb") as f, gzip.open(self.path("disk-after.hfs.gz"), "wb", 6) as g:
            shutil.copyfileobj(f, g)
        os.unlink(disk)

    # -- evidence ------------------------------------------------------------------
    def analyse(self, host_ping):
        pk = list(read_pcap(self.path("lan.pcap"))) if os.path.exists(self.path("lan.pcap")) else []
        frames = [decode(f) for _, f in pk]
        gmac = bytes.fromhex(GUEST_MAC.replace(":", ""))
        gip, tip = socket.inet_aton(self.guest_ip), socket.inet_aton(self.tap_ip)
        self.facts = {"pcap_frames": len(pk), "guest_frames": sum(1 for d in frames if d["src"] == gmac)}
        self.check("guest_on_hub", self.facts["guest_frames"] > 0,
                   "%d frames from %s in the pcap" % (self.facts["guest_frames"], GUEST_MAC))

        def icmp(src, dst, typ):
            return [d for d in frames if d.get("proto") == 1 and d["ip_src"] == src and d["ip_dst"] == dst
                    and d["icmp_type"] == typ]
        if self.name == "r1":
            req, rep = icmp(gip, tip, 8), icmp(tip, gip, 0)
            self.check("guest_ping_hub", len(req) >= 5 and len(rep) >= 5,
                       "guest->%s echo requests %d, replies from the container kernel %d" % (self.tap_ip, len(req), len(rep)))
            self.check("guest_ping_screen", self.screen_text_seen("5/5/0/0"),
                       "MacTCP Ping drew the tally 5/5/0/0 (the guest's DrawString, echoed by showtext)")
            if host_ping is None:
                self.check("host_ping_guest", False, "E2E_PING_DONE marker never appeared")
            else:
                m = re.search(r"(\d+) packets transmitted, (\d+) (?:packets )?received", host_ping.stdout)
                recv = int(m.group(2)) if m else 0
                self.check("host_ping_guest", recv == 5, "container `ping %s`: %s" %
                           (self.guest_ip, m.group(0) if m else host_ping.stdout.strip()[-200:]))
                hreq, hrep = icmp(tip, gip, 8), icmp(gip, tip, 0)
                self.check("host_ping_pcap", len(hreq) >= 5 and len(hrep) >= 5,
                           "pcap: container->guest echo requests %d, guest replies %d" % (len(hreq), len(hrep)))
            return
        if self.cfg["app"] == "BomberTalk":
            self.analyse_bombertalk(frames, gip, tip)
        else:
            self.analyse_peertalk(frames, gip, tip)

    def tcp_checks(self, frames, gip, tip):
        ga, ta = self.guest_ip, self.tap_ip
        # TCP: SYN (no ACK) packets between the two addresses = connection attempts
        syn = [d for d in frames if d.get("proto") == 6 and d["tcp_flags"] & 0x12 == 0x02
               and {d["ip_src"], d["ip_dst"]} == {gip, tip}]
        synack = [d for d in frames if d.get("proto") == 6 and d["tcp_flags"] & 0x12 == 0x12
                  and {d["ip_src"], d["ip_dst"]} == {gip, tip}]
        tuples = {(d["ip_src"], d["sport"], d["ip_dst"], d["dport"]) for d in syn}
        # a connection is established when its SYN was answered by a SYN-ACK of the reversed tuple
        answered = {(d["ip_dst"], d["dport"], d["ip_src"], d["sport"]) for d in synack}
        established = tuples & answered
        unanswered = tuples - answered
        initiators = {socket.inet_ntoa(t[0]) for t in tuples}
        self.facts.update(tcp_syn_segments=len(syn), tcp_connections_established=len(established),
                          tcp_unanswered_syn_tuples=len(unanswered), initiators=sorted(initiators))
        # test_mesh redials after a disconnect, so SYNs that nobody answers can follow the
        # guest's exit at the end of the run; they are counted, not failed.
        # R6: after the round the players are back in the lobby and may dial again, so the first
        # connection is what is asserted there
        ok_n = (lambda k: k >= 1) if self.cfg.get("game") == "end" else (lambda k: k == 1)
        self.check("one_tcp_connection", ok_n(len(established)),
                   "%d established connection(s) (SYN answered by SYN-ACK), %d later unanswered SYN(s) (test_mesh redials after the guest quits); SYN sent by: %s"
                   % (len(established), len(unanswered), ",".join(sorted(initiators)) or "nobody"))
        # who should dial: the lower address compared as network-order bytes (PT_ShouldInitiate)
        lower = ga if gip < tip else ta
        le = lambda b: struct.unpack("<I", b)[0]
        be = lambda b: struct.unpack(">I", b)[0]
        self.facts["native_int_rank"] = {
            "bytewise_lower": lower,
            "little_endian_peer_thinks_lower": ga if le(gip) < le(tip) else ta,
            "big_endian_mac_thinks_lower": ga if be(gip) < be(tip) else ta}
        self.check("initiator_is_designated", initiators == {lower} and ok_n(len(established)),
                   "SYN from %s; bytewise-lower address is %s (peer reads it as LE: %s lower, Mac as BE: %s lower)"
                   % (",".join(sorted(initiators)) or "nobody", lower,
                      self.facts["native_int_rank"]["little_endian_peer_thinks_lower"],
                      self.facts["native_int_rank"]["big_endian_mac_thinks_lower"]))

    def screen_text_seen(self, text):
        # boot.inc turns `showtext on`: every string the guest draws is echoed on stdout
        return text in open(self.path("emulator.out"), errors="replace").read()

    def analyse_peertalk(self, frames, gip, tip):
        ga, ta = self.guest_ip, self.tap_ip
        peer_log = open(self.path("peer.log"), errors="replace").read()
        with self.capture_lock:
            cap = list(self.capture)

        def lines_from(ip):
            return [t for _, i, t in cap if ("@%s]" % ip) in t]
        g, p = lines_from(ga), lines_from(ta)
        # The container's own debug lines are also on stdout (peer.log), in case loopback missed some
        self.facts.update(guest_lines=len(g), peer_lines=len(p))
        self.check("udp_capture_guest", len(g) > 0, "%d lines tagged @%s received over the hub on udp/%d" % (len(g), ga, DEBUG_PORT))
        disc_g = [l for l in g if "[DISC] " in l and "@ %s" % ta in l]
        conn_in = [l for l in g if "[CONN] " in l and "@ %s" % ta in l]
        guest_dials = (socket.inet_aton(ga) < socket.inet_aton(ta))
        if guest_dials:   # it cannot dial a peer it has not discovered
            self.check("guest_discovers_posix", disc_g, disc_g[0] if disc_g else "no '[DISC] .. @ %s' from the guest" % ta)
        else:
            self.check("guest_knows_posix", disc_g or conn_in,
                       (disc_g or conn_in or ["guest never saw the peer"])[0] + "  (the peer dials first: the guest may learn of it from the inbound connection; r2b proves UDP discovery on the guest)")
        disc_p = [l for l in (p + peer_log.splitlines()) if "[DISC] " in l and "@ %s" % ga in l]
        self.check("posix_discovers_guest", disc_p, disc_p[0] if disc_p else "no '[DISC] .. @ %s' from the peer" % ga)

        self.tcp_checks(frames, gip, tip)
        # both sides report the connection ([CONN]) and receive heartbeats from the other
        conn_g = [l for l in g if "[CONN] " in l and "@ %s" % ta in l]
        conn_p = [l for l in (p + peer_log.splitlines()) if "[CONN] " in l and "@ %s" % ga in l]
        self.check("both_sides_connected", conn_g and conn_p,
                   "guest: %s | peer: %s" % (conn_g[0] if conn_g else "none", conn_p[0] if conn_p else "none"))

        def hb(lines, other):
            best = 0
            for l in lines:
                m = re.search(r"HB recv from %s: (\d+)" % re.escape(other), l)
                if m:
                    best = max(best, int(m.group(1)))
            return best
        hg, hp = hb(g, ta), hb(p + peer_log.splitlines(), ga)
        self.facts.update(heartbeats_guest_received=hg, heartbeats_peer_received=hp)
        self.check("message_round_trip", hg >= 1 and hp >= 1,
                   "heartbeats (PT_Broadcast, PT_RELIABLE over the TCP link): guest received %d from the peer, peer received %d from the guest"
                   % (hg, hp))
        live_g = [l for l in g if "LIVE LINK" in l]
        live_p = [l for l in (p + peer_log.splitlines()) if "LIVE LINK" in l]
        self.check("guest_verdict_pass", any("*** PASS ***" in l for l in g),
                   "guest report: %s" % (live_g[0] if live_g else "no LIVE LINK line"))
        # The peer's own final report comes after the guest has left (PT_Shutdown closes the
        # link right after the guest's verdict), so judge the peer by its status lines while the
        # link was up: connected=1 with a growing heartbeat count, and no DEAD LINK.
        plines = peer_log.splitlines()
        up = [l for l in plines if re.search(r"connected=1 ", l)]
        dead = [l for l in plines if "DEAD LINK" in l]
        self.check("peer_link_alive", up and hp >= 1 and not dead,
                   "peer status lines with connected=1: %d, heartbeats received %d, DEAD LINK lines: %d%s"
                   % (len(up), hp, len(dead), ("; " + live_p[0]) if live_p else ""))

    def analyse_bombertalk(self, frames, gip, tip):
        ga, ta = self.guest_ip, self.tap_ip
        with self.capture_lock:
            cap = list(self.capture)
        peer_log = open(self.path("peer.log"), errors="replace").read().splitlines()
        g = [t for _, i, t in cap if "@%s]" % ga in t]
        p = [t for _, i, t in cap if "@%s]" % ta in t] + peer_log
        self.facts.update(guest_lines=len(g), peer_lines=len(p))
        self.check("udp_capture_both", g and p, "%d BomberTalk log lines from the guest, %d from the POSIX build, received over the hub on udp/%d"
                   % (len(g), len(p), DEBUG_PORT))
        self.check("guest_in_lobby", any("Lobby entered" in l for l in g), "guest 'Lobby entered'")
        self.check("posix_in_lobby", any("Lobby entered" in l for l in p), "POSIX 'Lobby entered' (Return held by the LD_PRELOAD shim)")
        dg = [l for l in g if "Peer discovered:" in l and ta in l]
        dp = [l for l in p if "Peer discovered:" in l and ga in l]
        self.check("guest_discovers_posix", dg, dg[0] if dg else "no 'Peer discovered: .. (%s)' from the guest" % ta)
        self.check("posix_discovers_guest", dp, dp[0] if dp else "no 'Peer discovered: .. (%s)' from the POSIX build" % ga)
        self.tcp_checks(frames, gip, tip)
        cg = [l for l in g if "Connected to:" in l]
        cp = [l for l in p if "Connected to:" in l]
        self.check("both_sides_connected", cg and cp, "guest: %s | POSIX: %s" % (cg[0] if cg else "none", cp[0] if cp else "none"))
        if self.cfg.get("game") == "end":
            self.analyse_end(g, p)
            return
        if self.cfg.get("game"):
            self.analyse_game(g, p)
            return
        shots = [n for n in ("E2E_BT_MENU", "E2E_BT_LOBBY", "E2E_BT_LOBBY2", "E2E_BT_LOBBY3")
                 if os.path.exists(os.path.join(self.shots, n + ".png"))]
        self.check("guest_lobby_screenshots", len(shots) == 4, "saved: %s (the lobby text is drawn by BomberTalk's own renderer, so the screenshots are for a human)" % ", ".join(shots))

    def analyse_end(self, g, p):
        """R6: the round ends (P0, the POSIX build, stands in its own blast; it is the lowest rank, so the
        game-over authority) and both sides run the round-end path and return to the lobby."""
        def first(lines, *needles):
            return next((l for l in lines if all(n in l for n in needles)), None)
        def n(lines, needle):
            return sum(1 for l in lines if needle in l)
        self.check("guest_sends_game_start", first(g, "TX game start: 2 players"), first(g, "TX game start") or "no 'TX game start'")
        self.check("both_in_game", first(g, "Game started: 2 players, local=1") and first(p, "Game started: 2 players, local=0"),
                   "guest: %s | POSIX: %s" % (first(g, "Game started"), first(p, "Game started")))
        self.check("posix_bomb_placed", first(p, "TX bomb placed") and first(g, "RX bomb placed P0 (1,1)"),
                   "POSIX: %s | guest: %s" % (first(p, "TX bomb placed"), first(g, "RX bomb placed P0")))
        # authority = the POSIX build (rank 0): it detects, sends MSG_GAME_OVER (winner 1 = guest), ends
        self.check("posix_authority_game_over", first(p, "Game over!", "Winner: 1") and first(p, "Game over authority: sending MSG_GAME_OVER")
                   and first(p, "TX game over: winner=1"),
                   "POSIX: %s | %s | %s" % (first(p, "Game over!"), first(p, "Game over authority"), first(p, "TX game over")))
        # non-authority = the guest: either its own detection (then MSG_GAME_OVER ends it) or just the message
        self.check("guest_game_over", first(g, "Game over"), "guest: %s" % (first(g, "Game over (remote)") or first(g, "Game over")))
        self.check("guest_winner_is_guest", first(g, "winner=1") or first(g, "Winner: 1"),
                   first(g, "winner=1") or first(g, "Winner: 1") or "no winner line")
        self.check("guest_started_grace", first(g, "Game over: starting grace period"), first(g, "starting grace period") or "none")
        self.check("posix_started_grace", first(p, "Game over: starting grace period"), first(p, "starting grace period") or "none")
        self.check("both_grace_complete", first(g, "Grace period complete, disconnecting") and first(p, "Grace period complete, disconnecting"),
                   "guest: %s | POSIX: %s" % (first(g, "Grace period complete"), first(p, "Grace period complete")))
        self.check("both_back_to_lobby", first(g, "GAME -> LOBBY") and first(p, "GAME -> LOBBY") and n(g, "Lobby entered") >= 2 and n(p, "Lobby entered") >= 2,
                   "guest: %s (Lobby entered x%d) | POSIX: %s (x%d)" % (first(g, "GAME -> LOBBY"), n(g, "Lobby entered"), first(p, "GAME -> LOBBY"), n(p, "Lobby entered")))
        # the round end must come after the game start and after the bomb, on each side
        for who, L in (("guest", g), ("posix", p)):
            seq = [k for l in L for k in ("Game started:", "Game over", "Grace period complete", "GAME -> LOBBY") if k in l]
            self.check(who + "_round_end_order", seq.index("Game started:") < seq.index("Game over") < seq.index("Grace period complete") < max(i for i, k in enumerate(seq) if k == "GAME -> LOBBY"),
                       "%s sequence: %s" % (who, " > ".join(dict.fromkeys(seq))))
        bad = lambda L: first(L, "Game over failsafe:") or first(L, "Game over timeout")
        self.check("no_failsafe_or_timeout", not bad(g) and not bad(p),
                   "no 'Game over failsafe:'/'Game over timeout' warnings (the normal authority path was taken); guest: %s | POSIX: %s"
                   % (bad(g), bad(p)))
        shots = [x for x in ("E2E_GAME_1", "E2E_END_1", "E2E_END_2", "E2E_END_3") if os.path.exists(os.path.join(self.shots, x + ".png"))]
        self.check("guest_end_screenshots", len(shots) == 4, "saved: %s" % ", ".join(shots))

    def analyse_game(self, g, p):
        """R5: the game started on both sides and each side saw the other's movement and bomb.
        Player ids are ranks by address: the POSIX peer (10.88.0.10) is P0, the guest P1."""
        def first(lines, *needles):
            return next((l for l in lines if all(n in l for n in needles)), None)
        gs = first(g, "Game started: 2 players, local=1")
        ps = first(p, "Game started: 2 players, local=0")
        self.check("guest_sends_game_start", first(g, "TX game start: 2 players"), first(g, "TX game start") or "no 'TX game start' from the guest")
        self.check("posix_receives_game_start", first(p, "Game start received, expecting 2 players"),
                   first(p, "Game start received") or "no 'Game start received' on the POSIX side")
        self.check("both_leave_lobby", first(g, "LOBBY -> GAME") and first(p, "LOBBY -> GAME"),
                   "guest: %s | POSIX: %s" % (first(g, "LOBBY -> GAME"), first(p, "LOBBY -> GAME")))
        self.check("guest_in_game", gs, gs or "no 'Game started: 2 players, local=1' from the guest")
        self.check("posix_in_game", ps, ps or "no 'Game started: 2 players, local=0' from the POSIX build")

        # The POSIX build logs at DEBUG, so it records every position it receives; the 68k build
        # logs at INFO only (isMacSE), so what the guest saw of the POSIX player is its bomb
        # message (placed where the peer stood) and, below, its screenshots.
        def px(lines, who):   # distinct pixel positions logged for player `who` from the network
            return {m.group(1) for l in lines for m in [re.search(r"P%d target pos px=(\(\d+,\d+\))" % who, l)] if m}
        pos_p = px(p, 1)
        self.facts.update(posix_saw_guest_positions=sorted(pos_p))
        self.check("posix_sees_guest_move", len(pos_p) >= 2,
                   "POSIX received %d distinct positions of P1: %s" % (len(pos_p), ", ".join(sorted(pos_p)[:8])))
        txg, txp = first(g, "TX bomb placed"), first(p, "TX bomb placed")
        rxg, rxp = first(g, "RX bomb placed P0"), first(p, "RX bomb placed P1")
        self.check("guest_bomb_reaches_posix", txg and rxp, "guest: %s | POSIX: %s" % (txg, rxp))
        self.check("posix_bomb_reaches_guest", txp and rxg, "POSIX: %s | guest: %s" % (txp, rxg))
        m = re.search(r"RX bomb placed P0 \((\d+),(\d+)\)", rxg or "")
        self.check("guest_sees_posix_moved", m and (m.group(1), m.group(2)) != ("1", "1"),
                   "the guest received P0's bomb at %s; P0 spawns at (1,1), so it had moved" % (m.group(0) if m else "nothing"))
        self.check("both_alive_no_game_over", not first(g, "Game over") and not first(p, "Game over"),
                   "no 'Game over' on either side (nobody walked into a blast)")
        shots = [n for n in ("E2E_BT_LOBBY", "E2E_GAME_1", "E2E_GAME_2", "E2E_GAME_3", "E2E_GAME_4")
                 if os.path.exists(os.path.join(self.shots, n + ".png"))]
        self.check("guest_game_screenshots", len(shots) == 5, "saved: %s (E2E_GAME_*: the guest in the game with the POSIX player)" % ", ".join(shots))
        import hashlib
        def where(n, x0, x1):
            return sprite_x(os.path.join(self.shots, n + ".png"), x0, x1) if n in shots else None
        # the Plus board is 15 tiles of 16 px from x=136; the corners (1,1) and (13,1) are white floor
        # with the free neighbours (2,1) and (12,1): the sprite is the only dark thing in the band
        pa0, pa1 = where("E2E_GAME_1", 152, 184), where("E2E_GAME_4", 152, 184)
        ga0, ga1 = where("E2E_GAME_1", 328, 360), where("E2E_GAME_4", 328, 360)
        self.facts["sprite_x"] = {"posix_player": [pa0, pa1], "guest_player": [ga0, ga1]}
        self.check("guest_screen_shows_posix_player_moved", None not in (pa0, pa1) and 12 <= pa1 - pa0 <= 20,
                   "on the guest's screen the POSIX player's sprite is at x=%s at the start and x=%s at the end (one 16 px tile right)" % (pa0, pa1))
        self.check("guest_screen_shows_guest_player_moved", None not in (ga0, ga1) and -20 <= ga1 - ga0 <= -12,
                   "the guest's own sprite is at x=%s at the start and x=%s at the end (one tile left)" % (ga0, ga1))
        digs = {n: hashlib.sha1(open(os.path.join(self.shots, n + ".png"), "rb").read(), usedforsecurity=False).hexdigest()[:8]
                for n in shots}
        self.facts["screenshot_sha1"] = digs
        self.check("game_screen_changes", len({digs.get("E2E_BT_LOBBY"), digs.get("E2E_GAME_1")}) == 2
                   and len({digs.get("E2E_GAME_1"), digs.get("E2E_GAME_4")}) == 2,
                   "lobby, game start and the last game screenshot differ")

    # -- result ---------------------------------------------------------------------
    def finish(self):
        ok = bool(self.checks) and all(c["ok"] for c in self.checks.values())
        usage = {}
        for k, f in (("memory_peak_bytes", "/sys/fs/cgroup/memory.peak"), ("pids_peak", "/sys/fs/cgroup/pids.peak")):
            with contextlib.suppress(Exception):  # cgroup files may be absent or unreadable; usage is best-effort
                usage[k] = int(open(f).read())
        with contextlib.suppress(Exception):  # best-effort, as above
            for l in open("/sys/fs/cgroup/cpu.stat"):
                if l.startswith("usage_usec"):
                    usage["cpu_seconds"] = round(int(l.split()[1]) / 1e6, 1)
        summary = {"rung": self.name, "what": self.cfg["what"], "status": "PASS" if ok else "FAIL",
                   "guest": self.cfg["guest"], "tap": self.cfg["tap"],
                   "peer": ("%s (%s)" % (self.cfg["peer"], getattr(self, "peer_variant", "?"))) if self.cfg["peer"] else None,
                   "speed": os.environ.get("E2E_SPEED") or "default", "checks": self.checks,
                   "facts": getattr(self, "facts", {}), "timing_s": self.timing,
                   "wall_s": round(time.time() - self.t0, 1), "container_usage": usage}
        json.dump(summary, open(self.path("summary.json"), "w"), indent=2)
        if os.path.exists(self.path("disk.hfs")):     # an early exit left the working copy behind
            os.unlink(self.path("disk.hfs"))
        copy_out(self.work, OUT if os.environ.get("E2E_FLAT") else os.path.join(OUT, self.name))
        print("== %s: %s (%.0f s)" % (self.name, summary["status"], summary["wall_s"]), flush=True)
        return ok


# ---------------------------------------------------------------------------


def png_gray(path):
    """(width, height, rows) of a PNG as 8-bit red-channel rows (stdlib only; 8-bit RGBA/RGB, no interlace)."""
    d = open(path, "rb").read()
    o, idat = 8, b""
    while o < len(d):
        n, = struct.unpack(">I", d[o:o + 4])
        kind = d[o + 4:o + 8]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", d[o + 8:o + 18])
            assert depth == 8 and ctype in (2, 6), "unsupported PNG"
            bpp = 4 if ctype == 6 else 3
        elif kind == b"IDAT":
            idat += d[o + 8:o + 8 + n]
        o += 12 + n
    raw = zlib.decompress(idat)
    st, prev, rows, i = w * bpp, bytearray(w * bpp), [], 0
    for _ in range(h):
        f, line, i = raw[i], bytearray(raw[i + 1:i + 1 + st]), i + 1 + st
        for x in range(st):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(line)
        prev = line
    return w, h, [bytes(r[0::bpp]) for r in rows]


def sprite_x(path, x0, x1, y0=103, y1=118):
    """Leftmost dark pixel column in the top row of the Plus-sized board between x0 and x1
    (the two spawn corners are white floor, so a player sprite is the only dark thing there)."""
    w, h, rows = png_gray(path)
    xs = [x for x in range(x0, x1) if any(rows[y][x] < 100 for y in range(y0, y1))]
    return xs[0] if xs else None


def read_pcap(path):
    data = open(path, "rb").read()
    if len(data) < 24:
        return
    e = "<" if struct.unpack("<I", data[:4])[0] in (0xA1B2C3D4, 0xA1B23C4D) else ">"
    off = 24
    while off + 16 <= len(data):
        s, us, incl, _orig = struct.unpack(e + "IIII", data[off:off + 16])
        off += 16
        yield s + us / 1e6, data[off:off + incl]
        off += incl


def decode(f):
    d = {"dst": f[0:6], "src": f[6:12], "etype": struct.unpack(">H", f[12:14])[0]}
    if d["etype"] == 0x0800 and len(f) >= 34:
        ihl = (f[14] & 0x0F) * 4
        d["proto"] = f[23]
        d["ip_src"], d["ip_dst"] = f[26:30], f[30:34]
        l4 = 14 + ihl
        if d["proto"] == 1 and len(f) > l4:
            d["icmp_type"] = f[l4]
        elif d["proto"] == 6 and len(f) >= l4 + 14:
            d["sport"], d["dport"] = struct.unpack(">HH", f[l4:l4 + 4])
            d["tcp_flags"] = f[l4 + 13]
    return d


def copy_out(src, dst):
    os.makedirs(dst, exist_ok=True)
    total = 0
    for root, _dirs, files in os.walk(src):
        for n in sorted(files):
            p = os.path.join(root, n)
            rel = os.path.relpath(p, src)
            size = os.path.getsize(p)
            if total + size > OUT_CAP:
                print("output cap reached, skipping %s (%d bytes)" % (rel, size), file=sys.stderr)
                continue
            q = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(q), exist_ok=True)
            shutil.copyfile(p, q)
            total += size


def main(argv):
    if len(argv) == 2 and argv[0] == "--tap-ip":      # the entrypoint (root) asks which address tap0 gets
        print(RUNGS[argv[1]]["tap"])
        return 0
    rungs = argv or ["all"]
    if rungs == ["all"]:      # (the tap address is per rung; test/e2e/run.sh starts one container each)
        rungs = ["r1", "r2", "r3"]
    bad = [r for r in rungs if r not in RUNGS]
    if bad:
        print("unknown rung(s): %s (have %s)" % (" ".join(bad), " ".join(RUNGS)), file=sys.stderr)
        return 2
    results = {}
    for r in rungs:
        run = Run(r)
        try:
            run.go()
        except Exception as ex:  # a driver bug is a failure with a reason, never a silent pass
            run.check("driver", False, "%s: %s" % (type(ex).__name__, ex))
        finally:
            for p in run.procs:
                run.terminate(p, signal.SIGKILL, 5)
            run.stop.set()
        results[r] = run.finish()
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
