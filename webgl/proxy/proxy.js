#!/usr/bin/env node
/*
 * Armagetron web <-> server bridge proxy.
 *
 * The browser cannot open UDP sockets, so the client's datagrams are carried
 * over WebSockets to this process, which relays them to the real game and
 * master servers.  One WebSocket message is exactly one Armagetron datagram:
 * the protocol has no framing of its own, so the bridge must never split or
 * coalesce what it forwards.
 *
 * Two transport shapes are supported at the same time:
 *
 *   legacy (default)  one WebSocket per destination, the destination given in
 *                     the URL:  ws://host:8091/?target=1.2.3.4:4534
 *                     A LAN sweep therefore costs one WebSocket per port, and
 *                     the sweep's answers are tagged with 6 header bytes
 *                     (4 byte IPv4 + 2 byte port) so the client can tell the
 *                     servers apart.
 *
 *   mux               one WebSocket per client:  ws://host:8091/?mux=1
 *                     Every datagram carries a 9 byte header naming its peer
 *                     (see MUX_VER below); in the outbound direction that is
 *                     the destination, in the inbound direction the sender.
 *                     The proxy answers a datagram addressed to the broadcast
 *                     address with however many replies come back, each tagged
 *                     with its own sender, so a whole LAN sweep fits in one
 *                     socket.  The proxy announces itself with the AGMUX1
 *                     banner before any datagram; a client that does not see
 *                     it falls back to the legacy shape, which keeps old pages
 *                     working against a new proxy.
 *
 * Both directions are metered: a browser talking through one proxy address
 * looks like a single host sending everything, and the game's own flood
 * protection then bans the proxy for every user behind it (measured: "Flood
 * protection bans 127.0.0.1 after 10 pings in 0.81 seconds" while the server
 * browser polls a dozen servers).  The per-target token bucket below keeps the
 * relay under that threshold and counts what it had to delay or drop.
 *
 * Environment:
 *   PROXY_PORT    listen port                       (default 8091)
 *   TLS_CERT, TLS_KEY
 *                 serve wss:// (required from an https page: browsers block
 *                 ws:// there as mixed content)
 *   LAN_EXTRA     extra sweep targets, comma separated, e.g.
 *                 192.168.1.255:4534,gamehost:4534 - every client datagram is
 *                 replayed to these as well.  Development aid for labs where
 *                 the broadcast address does not reach the server.
 *   ALLOW         comma separated hosts this proxy may talk to, each entry a
 *                 host, host:port or *.  Unset means any host, which is fine
 *                 on a private network but makes this an open UDP relay: set
 *                 ALLOW before exposing the proxy publicly.
 *   PROXY_STATS   ms between per-session traffic lines, 0 disables  (15000)
 *   RATE, BURST   per-session datagram budget            (40/s, burst 25)
 *   TARGET_RATE, TARGET_BURST
 *                 per-destination budget                (6/s,  burst 12)
 *   QUEUE_MAX     datagrams held back per destination while pacing (24)
 *   MAX_DGRAM     largest datagram accepted, bytes           (2048)
 *   MAX_SESSIONS  concurrent WebSockets per client address      (32)
 *   IDLE_MS       close a silent session after this long    (180000)
 */
'use strict';
const http = require('http');
const WebSocket = require('ws');
const net = require('net');
const dgram = require('dgram');
const dns = require('dns');

function numEnv(name, dflt) {
  const v = process.env[name];
  if (v === undefined || v === '') return dflt;
  const n = Number(v);
  return Number.isFinite(n) ? n : dflt;
}

const PORT = numEnv('PROXY_PORT', 8091);
const TLS_CERT = process.env.TLS_CERT;
const TLS_KEY = process.env.TLS_KEY;
const LAN_EXTRA = (process.env.LAN_EXTRA || '')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean)
  .map((s) => {
    const m = /^(.*?):(\d+)$/.exec(s);
    return m ? { host: m[1], port: parseInt(m[2], 10) } : { host: s, port: 0 };
  });
const ALLOW = (process.env.ALLOW || '*')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean);
const STATS_MS = numEnv('PROXY_STATS', 15000);
const RATE = numEnv('RATE', 40);
const BURST = numEnv('BURST', 25);
const TARGET_RATE = numEnv('TARGET_RATE', 6);
const TARGET_BURST = numEnv('TARGET_BURST', 8);
// Discovery traffic (the server browser's polls) is what the servers' flood
// detector punishes, so it gets the tight budget; a live game session is lifted
// to these looser numbers once its reply volume proves it is one (PROMOTE_RX).
const GAME_RATE = numEnv('GAME_RATE', 60);
const GAME_BURST = numEnv('GAME_BURST', 60);
const PROMOTE_RX = numEnv('PROMOTE_RX', 24);
const QUEUE_MAX = numEnv('QUEUE_MAX', 24);
const MAX_DGRAM = numEnv('MAX_DGRAM', 2048);
const MAX_SESSIONS = numEnv('MAX_SESSIONS', 32);
const IDLE_MS = numEnv('IDLE_MS', 180000);

// Wire format of a multiplexed datagram.  fam 1 is a plain IPv4 peer, fam 2 a
// name: the client's address type is 32 bit, but the servers it is asked to
// contact are usually hostnames, so the name has to travel with the datagram
// and the proxy resolves it here, exactly as the legacy ?target=name:port form
// already does.
const MUX_VER = 1;
const MUX_FAM_V4 = 1;
const MUX_FAM_NAME = 2;
const BANNER = Buffer.from('AGMUX1');

function log(...a) {
  console.log(new Date().toISOString().slice(11, 23), ...a);
}

// Every session counts the bytes it moved in both directions and prints a line
// every PROXY_STATS ms (0 turns the reporting off).  Without it a stuck session
// and a working one look identical in the log: a connect, then silence.
const sessions = new Set();
function statsTimer(label, st) {
  if (!(STATS_MS > 0)) return () => {};
  const t = setInterval(() => {
    log('stats', label,
      'tx', st.tx + '/' + st.txBytes + 'B',
      'rx', st.rx + '/' + st.rxBytes + 'B',
      'slow', st.slow, 'drop', st.dropped,
      'age', ((Date.now() - st.t0) / 1000).toFixed(0) + 's');
  }, STATS_MS);
  return () => clearInterval(t);
}

function newStats(label) {
  return { label, tx: 0, txBytes: 0, rx: 0, rxBytes: 0, slow: 0, dropped: 0, t0: Date.now() };
}

// Token bucket: continuous refill, capped burst.  Bursts are what the game's
// own protocol produces (a login and its sync exchange a handful of datagrams
// back to back), while the sustained rate is what the servers' flood detector
// reacts to, so both ends of that shape have to be respected.
class Bucket {
  constructor(rate, burst) {
    this.rate = rate;
    this.burst = burst;
    this.level = burst;
    this.last = Date.now();
  }
  take() {
    const now = Date.now();
    this.level = Math.min(this.burst, this.level + ((now - this.last) / 1000) * this.rate);
    this.last = now;
    if (this.level >= 1) {
      this.level -= 1;
      return true;
    }
    return false;
  }
  // ms until the next token exists, for the drain timer
  waitMs() {
    return Math.max(5, Math.ceil((1 - this.level) / this.rate * 1000));
  }
}

// A paced FIFO for one destination.  Two budgets are in play and getting them
// wrong in either direction breaks the bridge: a server browser fires bursts of
// discovery queries at many servers, and seen from one shared proxy address
// that is a flood (the game bans the source after "10 pings in 0.81 seconds"),
// while a playing client needs a steady stream of datagrams each way and must
// not be squeezed by that same limit.  A destination therefore starts at the
// discovery budget and is lifted to the gameplay budget as soon as its reply
// volume proves a live game: a query answer is one or two datagrams, a game
// session answers hundreds.
class PacedQueue {
  constructor(st, rate, burst) {
    this.st = st;
    this.bucket = new Bucket(rate, burst);
    this.q = [];
    this.timer = null;
    this.promoted = false;
  }
  maybePromote() {
    if (this.promoted || this.st.rx <= PROMOTE_RX) return;
    this.promoted = true;
    this.bucket = new Bucket(GAME_RATE, GAME_BURST);
    log('pacing lifted', this.st.label, 'rx', this.st.rx);
  }
  push(datagram, sendNow) {
    this.maybePromote();
    if (this.bucket.take()) {
      sendNow(datagram);
      return;
    }
    this.st.slow++;
    if (this.q.length >= QUEUE_MAX) {
      this.st.dropped++;
      this.q.shift();      // the oldest is already stale: the client retries
    }
    this.q.push(datagram);
    this.schedule(sendNow);
  }
  // Drain on a timer instead of re-entering push(): re-entering push with a
  // placeholder would count a phantom drop and hand a null to the socket.
  schedule(sendNow) {
    if (this.timer) return;
    this.timer = setTimeout(() => {
      this.timer = null;
      while (this.q.length && this.bucket.take()) sendNow(this.q.shift());
      if (this.q.length) this.schedule(sendNow);
    }, this.bucket.waitMs());
  }
  clear() {
    if (this.timer) clearTimeout(this.timer);
    this.timer = null;
    this.q.length = 0;
  }
}

function allowedHost(host, port) {
  for (const a of ALLOW) {
    if (a === '*') return true;
    const m = /^(.*):(\d+)$/.exec(a);
    if (m) {
      if (m[1] === host && parseInt(m[2], 10) === port) return true;
    } else if (a === host) {
      return true;
    }
  }
  return false;
}

// ---------------- HTTP: /status for operators ----------------
const started = Date.now();
// Closed sessions leave the live list, but "did anything get dropped?" has to
// be answerable for the whole lifetime of the process, not only for whatever
// happens to be open when the request arrives.
const lifetime = { sessions: 0, tx: 0, txBytes: 0, rx: 0, rxBytes: 0, delayed: 0, dropped: 0 };
function accumulate(st) {
  lifetime.sessions++;
  lifetime.tx += st.tx; lifetime.txBytes += st.txBytes;
  lifetime.rx += st.rx; lifetime.rxBytes += st.rxBytes;
  lifetime.delayed += st.slow; lifetime.dropped += st.dropped;
}
function statusJson() {
  const list = [];
  for (const st of sessions) {
    list.push({
      kind: st.kind, target: st.target,
      tx: st.tx, txBytes: st.txBytes, rx: st.rx, rxBytes: st.rxBytes,
      delayed: st.slow, dropped: st.dropped, ageS: Math.round((Date.now() - st.t0) / 1000),
    });
  }
  const totals = list.reduce((a, s) => {
    a.tx += s.tx; a.txBytes += s.txBytes; a.rx += s.rx; a.rxBytes += s.rxBytes;
    a.delayed += s.delayed; a.dropped += s.dropped;
    return a;
  }, { tx: 0, txBytes: 0, rx: 0, rxBytes: 0, delayed: 0, dropped: 0 });
  return {
    uptimeS: Math.round((Date.now() - started) / 1000),
    sessions: list.length, allow: ALLOW, lanExtra: LAN_EXTRA,
    rate: { session: [RATE, BURST], target: [TARGET_RATE, TARGET_BURST], queue: QUEUE_MAX },
    totals, lifetime, list,
  };
}

function handleRequest(req, res) {
  const path = (req.url || '/').split('?')[0];
  if (path === '/status' || path === '/status.json') {
    const body = JSON.stringify(statusJson(), null, 2);
    res.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
    res.end(body);
  } else {
    res.writeHead(200, { 'content-type': 'text/plain' });
    res.end('armagetron datagram bridge, see /status\n');
  }
}

// The WebSocket endpoint and the status page share one port: an operator
// pointing a browser at the proxy sees what it is doing instead of guessing.
const wss = new WebSocket.Server({ noServer: true });
function serveOn(server, scheme) {
  server.on('request', handleRequest);
  server.on('upgrade', (req, socket, head) => {
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit('connection', ws, req));
  });
  server.listen(PORT, () => log('bridge listening on ' + scheme + '://0.0.0.0:' + PORT));
}

// wss:// needs real certificate files; a missing one is a startup error, not a
// silently downgraded bridge.  An https page cannot open ws:// (browsers block
// it as mixed content), so an https page has to be paired with a TLS proxy.
if (TLS_CERT && TLS_KEY) {
  const fs = require('fs');
  const https = require('https');
  serveOn(https.createServer(
    { cert: fs.readFileSync(TLS_CERT), key: fs.readFileSync(TLS_KEY) }
  ), 'wss');
} else {
  serveOn(http.createServer(), 'ws');
}

// sessionsPerIp guards one browser tab (or one abusive client) from eating the
// whole process: the legacy shape opens a socket per destination, and a LAN
// sweep across a port range does exactly that.
const sessionsPerIp = new Map();
function ipOf(req) {
  return (req && req.socket && req.socket.remoteAddress) || '?';
}

wss.on('connection', (ws, req) => {
  const ip = ipOf(req);
  const used = sessionsPerIp.get(ip) || 0;
  if (used >= MAX_SESSIONS) {
    log('session cap', ip, used);
    try { ws.close(1013, 'too many sessions'); } catch (e) {}
    return;
  }
  sessionsPerIp.set(ip, used + 1);
  let counted = false;
  const release = () => {
    if (counted) return;
    counted = true;
    const n = (sessionsPerIp.get(ip) || 1) - 1;
    if (n <= 0) sessionsPerIp.delete(ip);
    else sessionsPerIp.set(ip, n);
  };

  let u;
  try { u = new URL(req.url, 'http://localhost'); }
  catch (e) { log('bad url', req.url); release(); ws.close(); return; }

  if (u.searchParams.get('mux')) {
    bridgeMux(ws, ip);
  } else {
    const t = parseTarget(u);
    if (!t) { log('missing target', req.url); release(); ws.close(); return; }
    if (!allowedHost(t.host, t.port)) {
      log('denied', t.proto, t.host + ':' + t.port, 'from', ip);
      release(); ws.close(1008, 'not allowed'); return;
    }
    log('connect', t.proto, t.host + ':' + t.port, 'from', ip);
    if (t.proto === 'udp') bridgeUdp(ws, t.host, t.port);
    else bridgeTcp(ws, t.host, t.port);
  }
  ws.on('close', release);
});

// Parse the target from the query string. Supports both the C code's
// `?target=<ip>:<port>` form and the `?host=&port=&proto=` form.
function parseTarget(u) {
  let host = u.searchParams.get('host');
  let port = u.searchParams.get('port');
  let proto = u.searchParams.get('proto');
  const target = u.searchParams.get('target');
  if (target) {
    // <ip>:<port> or <name>:<port>: names are resolved here, not by the client
    const m = /^([0-9A-Za-z.\-_]+):(\d+)$/.exec(target);
    if (m) { host = m[1]; port = m[2]; }
  }
  if (!host) return null;
  port = parseInt(port || '4534', 10);
  if (!(port > 0 && port < 65536)) return null;   // 0.0.0.0:0: nothing to bridge
  // Armagetron is UDP; default to udp unless explicitly told otherwise.
  proto = (proto || 'udp').toLowerCase();
  return { host, port, proto };
}

function toBuf(d) {
  return Buffer.isBuffer(d) ? d : Buffer.from(d);
}

function isV4(s) {
  return /^(\d{1,3}\.){3}\d{1,3}$/.test(s);
}

// ---------------- multiplexed session ----------------
//
// One WebSocket, many destinations.  Outbound frames name where the datagram
// goes, inbound frames name who sent it, so the client can tell a sweep's
// answers apart without a socket per port.  A destination may be given as a
// name, which this process resolves (the browser cannot) and remembers, so the
// answer carries exactly the key the client used.
function muxEncode(peer, payload) {
  if (peer.name) {
    const name = Buffer.from(peer.name, 'ascii');
    const head = Buffer.alloc(2 + 1 + name.length + 2);
    head[0] = MUX_VER;
    head[1] = MUX_FAM_NAME;
    head[2] = name.length;
    name.copy(head, 3);
    head.writeUInt16BE(peer.port, 3 + name.length);
    return Buffer.concat([head, payload]);
  }
  const a = peer.ip.split('.').map((n) => parseInt(n, 10));
  const head = Buffer.alloc(9);
  head[0] = MUX_VER;
  head[1] = MUX_FAM_V4;
  head[2] = a[0]; head[3] = a[1]; head[4] = a[2]; head[5] = a[3];
  head.writeUInt16BE(peer.port, 6);
  return Buffer.concat([head, payload]);
}

function muxDecode(buf) {
  if (buf.length < 9 || buf[0] !== MUX_VER) return null;
  if (buf[1] === MUX_FAM_V4) {
    return {
      ip: buf[2] + '.' + buf[3] + '.' + buf[4] + '.' + buf[5],
      port: buf.readUInt16BE(6),
      payload: buf.slice(9),
    };
  }
  if (buf[1] === MUX_FAM_NAME) {
    const len = buf[2];
    if (buf.length < 3 + len + 2) return null;
    return {
      name: buf.slice(3, 3 + len).toString('ascii'),
      port: buf.readUInt16BE(3 + len),
      payload: buf.slice(3 + len + 2),
    };
  }
  return null;
}

function bridgeMux(ws, ip) {
  const st = newStats('mux ' + ip);
  st.kind = 'mux';
  st.target = ip;
  sessions.add(st);
  let stopStats = statsTimer(st.label, st);
  const session = new Bucket(RATE, BURST);
  // destination key -> {ip, port, name, queue}.  The key is what the client
  // put in the header, so answers can be labelled with the same key.
  const peers = new Map();
  const bySocket = new Map();      // resolved ip:port -> key
  const sock = dgram.createSocket('udp4');
  let closed = false;
  let lastSeen = Date.now();
  const close = (why) => {
    if (closed) return;
    closed = true;
    stopStats();
    sessions.delete(st);
    accumulate(st);
    for (const p of peers.values()) p.queue.clear();
    peers.clear();
    try { sock.close(); } catch (e) {}
    log('mux closed', ip, why || '', 'tx', st.tx + '/' + st.txBytes + 'B',
      'rx', st.rx + '/' + st.rxBytes + 'B', 'delayed', st.slow, 'dropped', st.dropped);
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };

  sock.bind(0, '0.0.0.0');
  sock.on('listening', () => {
    // A sweep datagram addressed to 255.255.255.255 has to leave through a
    // wildcard-bound socket with SO_BROADCAST on.
    try { sock.setBroadcast(true); } catch (e) { log('setBroadcast failed', e.message); }
    ws.send(BANNER, { binary: true }, (e) => {
      if (e) log('banner send failed', e.message);
    });
    log('mux open', ip);
  });

  const keyOf = (peer) => (peer.name ? peer.name + ':' + peer.port : peer.ip + ':' + peer.port);

  function entryFor(peer) {
    const key = keyOf(peer);
    let e = peers.get(key);
    if (e) return e;
    if (!allowedHost(peer.name || peer.ip, peer.port)) {
      log('mux denied', key, 'from', ip);
      return null;
    }
    e = {
      key, ip: peer.ip || null, port: peer.port, name: peer.name || null,
      queue: new PacedQueue(st, TARGET_RATE, TARGET_BURST),
    };
    peers.set(key, e);
    if (e.ip) bySocket.set(e.ip + ':' + e.port, key);
    if (e.name) {
      dns.lookup(e.name, { family: 4 }, (err, address) => {
        if (err) { log('mux lookup failed', e.name, err.message); return; }
        e.ip = address;
        bySocket.set(address + ':' + e.port, key);
      });
    }
    return e;
  }

  sock.on('message', (msg, rinfo) => {
    st.rx++; st.rxBytes += msg.length;
    lastSeen = Date.now();
    if (ws.readyState !== WebSocket.OPEN) return;
    // Label the reply with the key the client used for this server, so a
    // server reached through a name is still recognised as that name.
    const key = bySocket.get(rinfo.address + ':' + rinfo.port);
    const peer = (key && peers.get(key)) || null;
    const out = muxEncode(
      peer && peer.name ? { name: peer.name, port: rinfo.port }
        : { ip: rinfo.address, port: rinfo.port },
      msg
    );
    ws.send(out, { binary: true }, (e) => { if (e) log('ws send err', e.message); });
  });
  sock.on('error', (e) => { log('mux udp error', ip, e.message); close('error'); });

  ws.on('message', (d) => {
    if (closed) return;
    lastSeen = Date.now();
    const buf = toBuf(d);
    const f = muxDecode(buf);
    if (!f) { log('mux bad frame', ip, buf.length); return; }
    if (buf.length > MAX_DGRAM) { st.dropped++; return; }
    const e = entryFor(f);
    if (!e) return;
    const sendNow = (datagram) => {
      if (!e.ip) { log('mux unresolved', e.key); return; }
      try {
        sock.send(datagram, e.port, e.ip, (err) => { if (err) log('mux send error', e.key, err.message); });
        st.tx++; st.txBytes += datagram.length;
      } catch (err) { log('mux send error', e.key, err.message); }
    };
    // The session bucket first (all traffic from one browser), then the
    // per-destination bucket, which is what the servers' flood detector sees.
    if (!session.take()) {
      st.slow++;
      e.queue.push(f.payload, sendNow);
      return;
    }
    e.queue.push(f.payload, sendNow);
    for (const extra of LAN_EXTRA) {
      e.queue.push(f.payload, (datagram) => {
        try {
          sock.send(datagram, extra.port || e.port, extra.host,
            (err) => { if (err) log('mux extra send error', extra.host, err.message); });
        } catch (err) { log('mux extra send error', extra.host, err.message); }
      });
    }
  });

  const idleTimer = IDLE_MS > 0 ? setInterval(() => {
    if (Date.now() - lastSeen > IDLE_MS) close('idle');
  }, Math.max(1000, Math.min(IDLE_MS, 10000))) : null;

  ws.on('close', () => close('ws close'));
  ws.on('error', (e) => { log('mux ws error', ip, e.message); close('ws error'); });
}

// ---------------- legacy per-destination sessions ----------------

function bridgeTcp(ws, host, port) {
  const sock = new net.Socket();
  const st = newStats('tcp ' + host + ':' + port);
  st.kind = 'tcp'; st.target = host + ':' + port;
  sessions.add(st);
  let stopStats = statsTimer(st.label, st);
  const pace = new Bucket(RATE, BURST);
  let closed = false;
  let lastSeen = Date.now();
  const close = () => {
    if (closed) return;
    closed = true;
    stopStats();
    clearInterval(idleTimer);
    accumulate(st);
    sessions.delete(st);
    try { sock.destroy(); } catch (e) {}
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };
  sock.on('connect', () => log('tcp connected', host + ':' + port));
  sock.on('data', (d) => {
    st.rx++; st.rxBytes += d.length;
    lastSeen = Date.now();
    if (ws.readyState === WebSocket.OPEN) ws.send(d);
  });
  sock.on('error', (e) => { log('tcp error', host + ':' + port, e.message); close(); });
  sock.on('close', () => { log('tcp closed', host + ':' + port); close(); });
  ws.on('message', (d) => {
    if (closed) return;
    lastSeen = Date.now();
    const buf = toBuf(d);
    if (buf.length > MAX_DGRAM) { st.dropped++; return; }
    if (!pace.take()) st.slow++;           // a stream cannot be re-ordered: delay
    st.tx++; st.txBytes += buf.length;
    sock.write(buf);
  });
  ws.on('close', () => close());
  ws.on('error', (e) => { log('ws error', host + ':' + port, e.message); close(); });
  const idleTimer = IDLE_MS > 0 ? setInterval(() => {
    if (Date.now() - lastSeen > IDLE_MS) { log('tcp idle', host + ':' + port); close(); }
  }, Math.max(1000, Math.min(IDLE_MS, 10000))) : null;
  sock.connect(port, host);
}

function bridgeUdp(ws, host, port) {
  const sock = dgram.createSocket('udp4');
  const st = newStats('udp ' + host + ':' + port);
  st.kind = 'udp'; st.target = host + ':' + port;
  sessions.add(st);
  let stopStats = statsTimer(st.label, st);
  const queue = new PacedQueue(st, TARGET_RATE, TARGET_BURST);
  let closed = false;
  let lastSeen = Date.now();
  let remote = { address: host, port: port };
  const sweep = host === '255.255.255.255' || host === 'broadcast';
  // Node needs a bound dgram handle before setBroadcast, and a broadcast can
  // only leave through a socket bound to the wildcard address.  A failure here
  // is logged, not fatal: the LAN_EXTRA unicast replays still reach the peers.
  sock.bind(0, '0.0.0.0');
  sock.on('listening', () => {
    if (!sweep) return;
    try { sock.setBroadcast(true); } catch (e) { log('setBroadcast failed', e.message); }
  });
  const close = () => {
    if (closed) return;
    closed = true;
    stopStats();
    queue.clear();
    clearInterval(idleTimer);
    accumulate(st);
    sessions.delete(st);
    try { sock.close(); } catch (e) {}
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };
  // Each received UDP datagram is sent as exactly one WS binary message
  // (1:1 with the C client, which treats one WS message = one datagram).
  // Unicast replies also fix where follow-up datagrams go: a server may answer
  // from the socket it received on rather than the address in the URL.  A
  // sweep has no such single peer, so its answers keep their sender instead.
  sock.on('message', (msg, rinfo) => {
    st.rx++; st.rxBytes += msg.length;
    lastSeen = Date.now();
    if (!sweep) remote = { address: rinfo.address, port: rinfo.port };
    if (ws.readyState !== WebSocket.OPEN) return;
    let out = msg;
    if (sweep) {
      log('sweep reply', rinfo.address + ':' + rinfo.port, msg.length);
      const a = rinfo.address.split('.').map((n) => parseInt(n, 10));
      if (a.length !== 4 || a.some((n) => isNaN(n))) return;
      out = Buffer.concat([
        Buffer.from([a[0], a[1], a[2], a[3], (rinfo.port >> 8) & 255, rinfo.port & 255]),
        msg,
      ]);
    }
    ws.send(out, (e) => { if (e) log('ws send err', e.message); });
  });
  sock.on('error', (e) => { log('udp error', host + ':' + port, e.message); close(); });
  sock.on('close', () => { log('udp closed', host + ':' + port); close(); });
  // Each WS binary message is exactly one Armagetron datagram; forward it
  // as a single UDP datagram (no length prefix -- the C client does not
  // frame its messages).
  const send = (datagram, address, dport, tag) => {
    // One bad datagram must never take the whole bridge down.
    if (!(dport > 0 && dport < 65536) || !address) return;
    try {
      sock.send(datagram, dport, address, (e) => { if (e) log(tag, e.message); });
      st.tx++; st.txBytes += datagram.length;
    } catch (e) { log(tag, e.message); }
  };
  ws.on('message', (d) => {
    if (closed) return;
    lastSeen = Date.now();
    const datagram = toBuf(d);
    if (datagram.length > MAX_DGRAM) { st.dropped++; return; }
    queue.push(datagram, (dg) => send(dg, remote.address, remote.port, 'udp send error'));
    for (const extra of LAN_EXTRA) {
      queue.push(datagram, (dg) => send(dg, extra.host, extra.port || remote.port, 'udp extra send error'));
    }
  });
  ws.on('close', () => close());
  ws.on('error', (e) => { log('ws error', host + ':' + port, e.message); close(); });
  const idleTimer = IDLE_MS > 0 ? setInterval(() => {
    if (Date.now() - lastSeen > IDLE_MS) { log('udp idle', host + ':' + port); close(); }
  }, Math.max(1000, Math.min(IDLE_MS, 10000))) : null;
}
