#!/usr/bin/env node
/*
 * Armagetron Web <-> Server bridge proxy.
 *
 * The WASM client is built with emscripten, and its C socket layer
 * (src/network/nSocket.cpp) opens one binary WebSocket session per
 * (ip:port) destination it talks to.  The Armagetron protocol is UDP
 * (nSocket uses sendto/recvfrom), so a WebSocket binary message is
 * exactly one Armagetron datagram.
 *
 * The session URL is built by the C code (ws_url_for):
 *
 *     proxy mode : <Module.ARM_PROXY_BASE>?target=<ip>:<port>
 *     direct mode : ws(s)://<ip>:<port>   (only against a WebSocket-capable
 *                                          server; the stock server is not)
 *
 * In proxy mode the WebSocket lands here.  This process parses the target
 * from the query string and bridges it to a real UDP socket: one WebSocket
 * binary message is exactly one UDP datagram in each direction (no framing
 * bytes), matching what the C client does.  The real server address is
 * learned from the first reply so follow-up datagrams go to the right place.
 *
 * For convenience the proxy also accepts `?host=&port=&proto=` and falls
 * back to UDP when no proto is given.  A TCP relay is kept for testing.
 * Two more things make the server browser work, both of which the browser
 * cannot do by itself:
 *   * `target=<name>:<port>` - the client has no resolver, so a hostname is
 *     carried through to here and resolved by this process.
 *   * `target=255.255.255.255:<port>` - the LAN sweep.  This process broadcasts
 *     on the client's behalf and relays every answer back.  Such a session has
 *     no single peer, so each relayed datagram is prefixed with its sender
 *     (4 byte IPv4 + 2 byte port, network order); the client strips the prefix
 *     and uses it as the source address of the answer.
 * LAN_EXTRA="host[:port],..." replays every client datagram to extra targets
 * (a subnet broadcast address, or a server the sweep cannot reach).
 */
'use strict';
const WebSocket = require('ws');
const net = require('net');
const dgram = require('dgram');

const PORT = process.env.PROXY_PORT ? parseInt(process.env.PROXY_PORT, 10) : 8091;
// Set TLS_CERT/TLS_KEY to serve wss:// (required from an https page such as
// the GitHub Pages deployment; browsers block ws:// there as mixed content).
const TLS_CERT = process.env.TLS_CERT;
const TLS_KEY = process.env.TLS_KEY;
// Extra LAN-sweep targets, e.g. LAN_EXTRA=192.168.1.255:4534,gamehost:4534
const LAN_EXTRA = (process.env.LAN_EXTRA || '')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean)
  .map((s) => {
    const i = s.lastIndexOf(':');
    return i < 0 ? { host: s, port: 0 } : { host: s.slice(0, i), port: parseInt(s.slice(i + 1), 10) };
  });

function log(...a) {
  console.log(new Date().toISOString().slice(11, 23), ...a);
}

const wsOptions = {
  handleProtocols: (protocols) => {
    for (const p of protocols) if (p === 'binary') return p;
    return null;
  },
};
if (TLS_CERT && TLS_KEY) {
  const https = require('https');
  const fs = require('fs');
  const server = https.createServer({
    cert: fs.readFileSync(TLS_CERT),
    key: fs.readFileSync(TLS_KEY),
  });
  server.listen(PORT, () => log('bridge listening on wss://0.0.0.0:' + PORT));
  wsOptions.server = server;
} else {
  wsOptions.port = PORT;
}

const wss = new WebSocket.Server(wsOptions);
wss.on('listening', () => { if (!wsOptions.server) log('bridge listening on ws://0.0.0.0:' + PORT); });

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

wss.on('connection', (ws, req) => {
  let u;
  try { u = new URL(req.url, 'http://localhost'); }
  catch (e) { log('bad url', req.url); ws.close(); return; }
  const t = parseTarget(u);
  if (!t) { log('missing target', req.url); ws.close(); return; }
  log('connect', t.proto, t.host + ':' + t.port);
  if (t.proto === 'udp') bridgeUdp(ws, t.host, t.port);
  else bridgeTcp(ws, t.host, t.port);
});

function toBuf(d) {
  return Buffer.isBuffer(d) ? d : Buffer.from(d);
}

function bridgeTcp(ws, host, port) {
  const sock = new net.Socket();
  let closed = false;
  const close = () => {
    if (closed) return;
    closed = true;
    try { sock.destroy(); } catch (e) {}
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };
  sock.on('connect', () => log('tcp connected', host + ':' + port));
  sock.on('data', (d) => { if (ws.readyState === WebSocket.OPEN) ws.send(d); });
  sock.on('error', (e) => { log('tcp error', host + ':' + port, e.message); close(); });
  sock.on('close', () => { log('tcp closed', host + ':' + port); close(); });
  ws.on('message', (d) => { if (!closed) sock.write(toBuf(d)); });
  ws.on('close', () => close());
  ws.on('error', (e) => { log('ws error', host + ':' + port, e.message); close(); });
  sock.connect(port, host);
}

function bridgeUdp(ws, host, port) {
  const sock = dgram.createSocket('udp4');
  let closed = false;
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
    try { sock.close(); } catch (e) {}
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };
  // Each received UDP datagram is sent as exactly one WS binary message
  // (1:1 with the C client, which treats one WS message = one datagram).
  // Unicast replies also fix where follow-up datagrams go: a server may answer
  // from the socket it received on rather than the address in the URL.  A
  // sweep has no such single peer, so its answers keep their sender instead.
  sock.on('message', (msg, rinfo) => {
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
  const send = (datagram, address, port, tag) => {
    // One bad datagram must never take the whole bridge down.
    if (!(port > 0 && port < 65536) || !address) return;
    try {
      sock.send(datagram, port, address, (e) => { if (e) log(tag, e.message); });
    } catch (e) { log(tag, e.message); }
  };
  ws.on('message', (d) => {
    if (closed) return;
    const datagram = toBuf(d);
    send(datagram, remote.address, remote.port, 'udp send error');
    for (const extra of LAN_EXTRA) {
      send(datagram, extra.host, extra.port || remote.port, 'udp extra send error');
    }
  });
  ws.on('close', () => close());
  ws.on('error', (e) => { log('ws error', host + ':' + port, e.message); close(); });
}
