# Server browser over the bridge — before / after

Not a graphics item; included because both frames come from the same capture
setup and the difference is one the player sees.

`server-browser-before.png` — the port's Online Multiplayer list, 18 s after
opening it, nothing pressed: the master list arrived (13 entries) but every row
reads `Polling..` with ping, users and score empty. Measured: **zero** direct
queries to any listed server, and the bridge held exactly **one** peer (the
master).

`server-browser-after.png` — same build path, same master, after the fix that
re-arms the per-server query round when a browse round ends: every row shows its
name, ping and user count with nothing pressed. Measured: **40** direct queries
(the probe's cap) including named servers such as `tomcraft.ddns.net:4534`,
**12** bridge peers, 157 datagrams out and 153 back, 27 held while a name lookup
was in flight, 0 dropped.

Two separate faults were involved, both on the path a browser build has to take
because it has neither UDP nor a resolver:

1. The bridge discarded the first datagram to any named destination, because
   creating that destination starts an asynchronous DNS lookup that always
   finishes after the datagram that created it. Named servers could therefore
   never be queried. `webgl/proxy/selftest.js` now checks exactly that case,
   plus that answers come back labelled with the name the client used.
2. The client's per-server query round was armed when the menu opened, i.e.
   before the master's asynchronously collected list existed, so it walked an
   empty list and the menu's polling latch went false for good. Pressing the
   list's refresh key filled the list in, which is what pinned the diagnosis.

Both fixes are in `1ee4ec65`. The frames above are served by the same harness
that produced the graphics comparisons (`perp2.aarec` replay, game-time gates).
