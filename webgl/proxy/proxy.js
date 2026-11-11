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
    const m = /^([\d.]+):(\d+)$/.exec(target);
    if (m) { host = m[1]; port = m[2]; }
  }
  if (!host) return null;
  port = parseInt(port || '4534', 10);
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
  const close = () => {
    if (closed) return;
    closed = true;
    try { sock.close(); } catch (e) {}
    try { if (ws.readyState === WebSocket.OPEN) ws.close(); } catch (e) {}
  };
  // Each received UDP datagram is sent as exactly one WS binary message
  // (1:1 with the C client, which treats one WS message = one datagram).
  // Replies also fix where follow-up datagrams go: a server may answer from
  // the socket it received on rather than the address in the URL.
  sock.on('message', (msg, rinfo) => {
    remote = { address: rinfo.address, port: rinfo.port };
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(msg, (e) => { if (e) log('ws send err', e.message); });
    }
  });
  sock.on('error', (e) => { log('udp error', host + ':' + port, e.message); close(); });
  sock.on('close', () => { log('udp closed', host + ':' + port); close(); });
  // Each WS binary message is exactly one Armagetron datagram; forward it
  // as a single UDP datagram (no length prefix -- the C client does not
  // frame its messages).
  ws.on('message', (d) => {
    if (closed) return;
    const datagram = toBuf(d);
    sock.send(datagram, remote.port, remote.address,
      (e) => { if (e) log('udp send error', e.message); });
  });
  ws.on('close', () => close());
  ws.on('error', (e) => { log('ws error', host + ':' + port, e.message); close(); });
}
