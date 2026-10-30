# Porsche front assembly revision

The front wheels previously crossed the body and suspension at some steering
and travel extremes. The revised source fits the front carrier alignment,
wishbone castings, curved tie rods, damper seats, spring morphs and mounting
supports together. The front arches and inner-well skin provide the matching
clearance. Caliper lettering now follows the casting as flat paint.

Original tire dimensions, nominal wheel centres and the existing steering and
±50 mm suspension command ranges are retained. The revised front alignment and
brackets are authored game geometry, not Porsche CAD or OEM suspension settings.

Validation for the exact published hashes is recorded in
`porsche-front-fit-review.json`:

- 135 front wheel/mechanism poses clear at each of Hero, Near and Far: 405 in total.
- 165 Hero tire/body poses clear, including asymmetric travel.
- The refined well adds no self-intersection pairs relative to the source.
- Each level passes 64 export checks and 65 suspension poses; 15 detail-switch
  cases retain controls, and protected exterior surfaces agree across levels.
- Native conversion retains all 1,193,664 triangles, attributes, morphs,
  hierarchy, materials and image bytes across the three exports.
- The rebuilt game passes 152 hidden checks, including 195 native suspension
  poses. Steering-wheel hub drift remains below 0.0003 mm across 69 poses.
- 36 native culling image pairs and 54 mirror-target comparisons show no
  missing rendering. Native front, cockpit and mechanism captures were reviewed.

The rolling regression now uses independent source wheel headings. Its former
track-width-only expectation assumed zero toe; the revised rest alignment has
about 0.157 degrees of toe per front wheel. The check retains its 1 micrometre
distance tolerance and checks both yaw directions. Production rolling logic is
unchanged.

Hero / Near / Far contain 695,220 / 291,248 / 207,196 triangles. Critical fitted
front surfaces are retained in the distance sources. Their other surfaces and
spring tessellation follow the existing distance policy. These counts and the
new geometry have not received a fresh frame-time benchmark; earlier timings
describe earlier hashes. Forza visual parity and 60 FPS are not established.

The finite clearance tests cover the surfaces named in their reports. They do
not prove whole-vehicle continuous clearance or solid containment. The corrected
well audit uses AABB candidates: Blender's float triangle-overlap filter had
missed some original shared-vertex crossings, misclassifying them as new defects.
Existing source self-intersections remain explicitly recorded.

`assets/porsche/Porsche_911_GT3RS_Source.blend` contains the editable source with
52 used image files packed. Moving groups, joint datums and spring shape keys
remain editable. Runtime code drives their controls; the Blender file is not a
timeline animation. The bundled packaging checker can reproduce the selected
wheel/mechanism test; see `tests/porsche_front_fit/README.md`.

To play this build, run `Play-Game.cmd` from the repository folder. Start a
mission, walk to the white/red Porsche at the base and press E. WASD drives,
Space brakes, C changes camera, O/K open the doors when stopped, I operates
the windows, and G operates the DRS flap. The packaged executable is byte-identical
to the binary used for the hidden native tests.
