#!/usr/bin/env node
// Self-test for the datagram bridge's multiplexed shape (webgl/proxy/proxy.js).
//
// It starts its own bridge on a spare port together with a one-socket UDP
// server, then plays the part of the wasm client: one WebSocket, framed
// datagrams.  Three things are checked, all of them things a player notices:
//
//   1. The very first datagram sent to a named destination has to arrive.
//      The client cannot resolve names (it has no resolver and its address type
//      is 32 bit), so it puts the name in the frame and the bridge looks it up.
//      That lookup is always asynchronous, so the datagram that created the
//      destination entry always beats it to the socket; a bridge that discards
//      what it cannot send yet silently eats the first query to every hostname
//      server, and the server browser then shows "Polling.." forever while the
//      numeric entries fill in.  That is the regression this test exists for.
//   2. The answer has to come back labelled with the name the client used, not
//      with the address the bridge resolved, or the client cannot match it to
//      the server it is showing in its list.
//   3. A name that does not resolve is reported rather than crashing the bridge.
//
// The status page is checked too: it is the only way an operator can tell a
// dead server apart from a bridge that never dialled it.
//
// usage: node webgl/proxy/selftest.js [port]     (a free port is chosen if none
// is given, so the test can run alongside a live bridge)

const dgram = require('dgram');
const net = require('net');
const path = require('path');
const { spawn } = require('child_process');
const WebSocket = require('ws');

const MUX_VER = 1, FAM_V4 = 1, FAM_NAME = 2;

function encodeFramed(peer, payload) {
  if (peer.name) {
    const name = Buffer.from(peer.name, 'ascii');
    const head = Buffer.alloc(3 + name.length + 2);
    head[0] = MUX_VER; head[1] = FAM_NAME; head[2] = name.length;
    name.copy(head, 3);
    head.writeUInt16BE(peer.port, 3 + name.length);
    return Buffer.concat([head, payload]);
  }
  const a = peer.ip.split('.').map((n) => parseInt(n, 10));
  const head = Buffer.alloc(9);
  head[0] = MUX_VER; head[1] = FAM_V4;
  head[2] = a[0]; head[3] = a[1]; head[4] = a[2]; head[5] = a[3];
  head.writeUInt16BE(peer.port, 6);
  return Buffer.concat([head, payload]);
}

function decodeFramed(buf) {
  if (buf.length < 9 || buf[0] !== MUX_VER) return null;
  if (buf[1] === FAM_V4) {
    return {
      ip: buf[2] + '.' + buf[3] + '.' + buf[4] + '.' + buf[5],
      port: buf.readUInt16BE(6), payload: buf.slice(9),
    };
  }
  if (buf[1] === FAM_NAME) {
    const len = buf[2];
    return {
      name: buf.slice(3, 3 + len).toString('ascii'),
      port: buf.readUInt16BE(3 + len), payload: buf.slice(3 + len + 2),
    };
  }
  return null;
}

function waitFor(pred, ms, what) {
  return new Promise((resolve, reject) => {
    const deadline = Date.now() + ms;
    const tick = setInterval(() => {
      const v = pred();
      if (v) { clearInterval(tick); resolve(v); }
      else if (Date.now() > deadline) { clearInterval(tick); reject(new Error('timeout: ' + what)); }
    }, 20);
  });
}

function fetchJSON(url) {
  return new Promise((resolve, reject) => {
    require('http').get(url, (res) => {
      let b = '';
      res.on('data', (d) => { b += d; });
      res.on('end', () => { try { resolve(JSON.parse(b)); } catch (e) { reject(e); } });
    }).on('error', reject);
  });
}

// An ephemeral port, released before it is used again: two listeners would make
// the test fail for a reason that has nothing to do with the bridge.
function freePort() {
  return new Promise((resolve) => {
    const s = net.createServer();
    s.listen(0, '127.0.0.1', () => { const p = s.address().port; s.close(() => resolve(p)); });
  });
}

async function main() {
  const listenPort = process.argv[2] ? parseInt(process.argv[2], 10) : await freePort();
  const serverPort = await freePort();

  // One UDP socket standing in for a game server: it answers whatever it gets
  // and remembers what it was asked.
  const seen = [];
  const game = dgram.createSocket('udp4');
  game.on('message', (msg, rinfo) => {
    seen.push(msg.toString());
    game.send(Buffer.from('PONG'), rinfo.port, rinfo.address);
  });
  await new Promise((r) => game.bind(serverPort, '127.0.0.1', r));

  const proxy = spawn(process.execPath, [path.join(__dirname, 'proxy.js')], {
    env: Object.assign({}, process.env, {
      PROXY_PORT: String(listenPort), ALLOW: '*', LAN_EXTRA: '', PROXY_STATS: '0',
    }),
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  let out = '';
  proxy.stdout.on('data', (d) => { out += d.toString(); });
  proxy.stderr.on('data', (d) => { out += d.toString(); });

  try {
    await waitFor(() => out.indexOf('bridge listening') >= 0, 5000, 'bridge listening');
  } catch (e) {
    console.log('the bridge would not start; its output:');
    console.log(out || '(nothing)');
    proxy.kill(); game.close();
    throw e;
  }

  const ws = new WebSocket('ws://127.0.0.1:' + listenPort + '/?mux=1');
  const frames = [];
  ws.on('message', (d) => {
    const b = Buffer.isBuffer(d) ? d : Buffer.from(d);
    if (b.length === 6 && b.toString() === 'AGMUX1') return;
    frames.push(decodeFramed(b));
  });
  await new Promise((resolve, reject) => {
    ws.on('open', resolve);
    ws.on('error', reject);
  });

  const failures = [];

  // 1. the first datagram to a named destination must reach the server
  ws.send(encodeFramed({ name: 'localhost', port: serverPort }, Buffer.from('QUERY1')));
  let arrived = false;
  try {
    await waitFor(() => seen.indexOf('QUERY1') >= 0, 3000, 'named query never reached the server');
    arrived = true;
  } catch (e) { failures.push(e.message); }

  // 2. the reply must be labelled with the name the client used
  let labelled = false;
  if (arrived) {
    try {
      await waitFor(() => frames.some((f) => f && f.name === 'localhost' &&
        f.port === serverPort && f.payload && f.payload.toString() === 'PONG'),
        3000, 'reply was not labelled with the name');
      labelled = true;
    } catch (e) { failures.push(e.message); }
  } else {
    failures.push('reply labelling not exercised: the query never arrived');
  }

  // 3. a name that does not resolve is reported, and the bridge stays up
  ws.send(encodeFramed({ name: 'no-such-host.invalid', port: serverPort }, Buffer.from('X')));
  let reported = false;
  try {
    await waitFor(() => out.indexOf('mux lookup failed') >= 0, 6000, 'failed lookup not reported');
    reported = true;
  } catch (e) { failures.push(e.message); }

  // 4. the status page reports where a name resolved to
  let peersVisible = false;
  try {
    const body = await fetchJSON('http://127.0.0.1:' + listenPort + '/status');
    const sess = (body.list || []).find((s) => s.kind === 'mux');
    peersVisible = !!(sess && sess.peers && sess.peers.some((p) =>
      p.key === 'localhost:' + serverPort && p.ip === '127.0.0.1'));
    if (!peersVisible) failures.push('status does not report the resolved peer: ' +
      JSON.stringify(sess && sess.peers));
  } catch (e) { failures.push('status: ' + e.message); }

  ws.close();
  game.close();
  proxy.kill();

  console.log('first named query delivered: ' + (arrived ? 'ok' : 'FAIL'));
  console.log('reply labelled by name:      ' + (labelled ? 'ok' : 'FAIL'));
  console.log('bad name reported:           ' + (reported ? 'ok' : 'FAIL'));
  console.log('status shows peers:          ' + (peersVisible ? 'ok' : 'FAIL'));
  if (failures.length) {
    console.log('failures:');
    for (const f of failures) console.log('  - ' + f);
    process.exit(1);
  }
  console.log('proxy selftest: all 4 checks passed');
}

main().then(() => process.exit(0)).catch((e) => {
  console.log('selftest error: ' + e.message);
  process.exit(1);
});
