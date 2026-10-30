# Cockpit display finish

This records the display revision in `038cef9`. The subsequent
[steering-marker revision](PORSCHE_STEERING_MARKER.md) retains this display code,
repeats its native checks, and changes the rim material and asset-loading path.

The native side displays now use four segmented circular instruments on the
modeled left/right panels. Their layout follows the circular instrument
silhouette in [Porsche's official cockpit reference](https://newsroom.porsche.com/en/2022/products/porsche-911-gt3-rs-world-premiere-29177.html).
Each dial shows that tyre's actual game temperature and remaining life. This is
an authored tyre page; it does not pretend to show unimplemented PASM settings.

The central lower LCD now uses the source layout's large current gear with a
compact D indicator when driving. Reverse uses the vertical PRND stack.
The instrument cluster, tachometer LCD and PCM go black when the car is parked.
Entry and exit bypass the 10 Hz telemetry update limit so the power transition
happens immediately. The independent Sport Chrono stopwatch keeps its readout.

The updated executable passes 155 hidden checks and 36 culling image pairs.
Changing each physical tyre independently changes 3,550 pixels in its own dial
and zero pixels outside that instrument. The three unpowered LCDs contain zero
lit pixels. Existing controls, detail switching, display ownership and camera
state restoration remain covered by the native suite.

`porsche-cockpit-displays-review.json` binds the evidence to the executable,
code, captures and retained geometry hashes. `porsche-cockpit-before-dials.png`
and `porsche-native-live-cockpit.png` provide matched native views.
The geometry, moving-part frames, source Blender file and model textures are
unchanged from `990db89`. The packaged executable includes this display revision.

This advances the instrument finish; it does not establish final Forza parity
or a new frame-rate result. No Porsche photograph pixels are used as textures.

The steering-wheel centering stripe identified during this review is included
in the subsequent revision. It follows the corrected wheel axis through full
lock and return to centre; current evidence is linked above.
