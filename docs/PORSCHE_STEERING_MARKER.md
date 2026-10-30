# Porsche steering wheel and native loading

The white/red GT3 RS now has the contrasting yellow 12-o'clock centering band
shown in Porsche's cockpit reference. The band uses 150 existing crown faces
with a warm yellow Race-Tex material and the existing grain maps. The Hero
wheel's positions, triangle winding and shading normals are retained. Near and
Far now retain that same rim shape, avoiding a broken band when detail changes.
The fitted horn-hub centre and steering axis are unchanged.

The native GLBs now carry untextured material placeholders for the stock loader.
Complete source materials remain in each rig JSON and the generated shader
header, and every source image remains byte-identical. The native renderer
uploads each shared texture through its cache. This avoids the stock loader's
temporary per-material texture uploads and ORM channel copies before rebinding.
The portable authoring exports and editable Blender source retain full materials.

The exact files are bound in `porsche-steering-marker-review.json`:

- 155 hidden native checks pass, including 69 steering poses and 195 suspension
  poses. All 14 steering-wheel surfaces, including the band, follow the hub.
- The indexed band vertices pass 138 additional motion poses across three detail
  levels, host transforms and return to centre. A stationary-parent negative
  control is rejected.
- All four portable exports pass 64 structural checks each. The edited source
  exports match their runtime rim positions and custom normals exactly.
- 36 main-view and 54 mirror-target culling pairs show no missing rendering.
- 15 detail-switch cases pass. Protected white paint, carbon, red graphics,
  glazing, lamp covers and mirrors match across levels.
- Native conversion checks every triangle corner, attribute, morph, hierarchy,
  material record and image. Full materials are checked in native metadata;
  the stock loader receives zero texture references.

Hero / Near / Far contain 695,220 / 292,878 / 209,478 triangles. The editable
`assets/porsche/Porsche_911_GT3RS_Source.blend` includes 52 packed used images.
Source moving groups and spring shape keys remain editable; game code drives
their controls. Attribution remains in the asset license files.

The older front-fit and cockpit review JSON files record their named baseline
revisions. Their historical hashes are not presented as hashes of this revision.
The new scope audit confirms that front body, wheel, suspension and mount
geometry is unchanged; the finite clearance evidence still applies to those
same surfaces. It does not establish continuous whole-car collision clearance.

The fresh 1280×720 hidden-window profile records mean frame times of 53.4–116.4 ms
across its eight cases on the RTX 3050 Laptop GPU. With contact shading enabled,
Low chase / Low cockpit / High chase / High cockpit average 73.1 / 95.4 / 116.4 /
97.4 ms. These instrumented measurements do not establish 60 FPS; rendering
performance remains open work. They include per-stage flush/query overhead,
exclude loading and combat, and are not a visible-play or cross-device result.

Run `Play-Game.cmd`, start a mission, walk to the white/red Porsche and press E.
WASD drives, Space brakes, C changes camera, O/K open the doors when stopped,
I operates the windows, and G operates DRS. The packaged executable matches the
binary used for the hidden tests. To repeat the native checks after building,
run `python scripts/run_porsche_hidden.py qa` and then
`python scripts/run_porsche_hidden.py culling`; both use hidden windows.

Current native captures were inspected at centre and both steering directions.
These checks establish the stated geometry, motion and rendering behavior;
final Forza-level visual parity remains a separate visual assessment.
