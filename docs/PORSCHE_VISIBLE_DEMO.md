Run a visible driving demonstration from the game repository root:

```
build-native/ninja/bin/game.exe --demo-porsche
```

The game enters the parked Porsche through its interaction path. It repeats
nine simulation seconds: four seconds accelerating, one second accelerating
with steering, two seconds braking, then two seconds holding the brake for
inspection. The car resets to its initial state between runs and alternates
chase and cockpit camera distances. A visible overlay shows the phase, speed,
steering input and body attitude. The ordinary game renderer and 120 Hz
gameplay updates are used. Simulation catch-up is limited to 250 ms per frame,
so slow rendering can make a cycle take longer than nine wall-clock seconds.

F8 ends automatic inputs and continues the ordinary game, letting the user
drive. Closing the window exits. Demo events use a separate local log at
`.tools/porsche-visible-demo-session.log`.

This is an observable demonstration, not an assertion-based test or a claim
of complete collision clearance, realistic handling, visual parity or frame
rate. The separate `--qa-porsche` suite remains hidden and writes its native
QA reports. On 2026-10-29 the visible mode was compiled into the primary game
executable, launched, and observed in cockpit views with live phase/attitude
overlays and repeated route-reset log events. No manual keyboard driving is
claimed for that observation.
