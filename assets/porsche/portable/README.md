# Textured Porsche 911 GT3 RS exports

`exports/Porsche_911_GT3RS_Development.glb` is the highest-detail model, with
695,220 triangles and embedded textures. Near and Far contain 292,878 and
209,478 triangles. `exports/gt3rs-lods.json` binds their hashes and shared rig.

The exports use metres, +Y up and +Z forward; the named left side is +X.
They contain named articulation groups and spring morph targets. The game
controller drives their motion; baked animation clips are not included.

The running C++ game uses the loader-optimized files in the parent folder and
the full material metadata/header. These portable files retain ordinary glTF
materials and embedded images for direct import into Blender or another tool.

The editable, image-packed source is `../Porsche_911_GT3RS_Source.blend`.
Controllers are in `src/breakwater/porsche_rig.cpp`, `porsche_suspension.cpp`
and `porsche_native.cpp` in the repository. The fitted steering-wheel axis is
part of the rig's rest transform; preserve that local frame when porting it.

Keep `ATTRIBUTION.md` and `fonts/OFL.txt` with model distributions. They record
the Black Snow CC BY 4.0 source, project adaptations, and font attribution.
Paths within the copied attribution notice refer to the full repository/project.
