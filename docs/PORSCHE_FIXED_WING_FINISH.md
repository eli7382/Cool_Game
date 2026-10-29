# Fixed rear-wing carbon finish

The white/red reference design has a dark rear-wing assembly. The previous
model assigned white body paint to the fixed main plane, while the moving
upper flap already used carbon. The fixed plane now uses that same exterior
carbon material, including its close-detail texture mapping. Near and far
variants use the existing prefiltered carbon material, preserving the distance
rendering policy.

Only the 1,008 triangles identified in the original editable fixed-wing object
change material. The runtime exporter had combined them with the static white
body, so the updated exports place them in `FixedWing_Carbon_Mainplane`, under
CarRoot with the original body batch transform. The upper flap, DRS/airbrake
hinges, actuator frames, supports and endplates are unchanged. Triangle counts
remain 699,758 / 226,176 / 163,440 for Hero / Near / Far.

`porsche-fixed-wing-finish-review.json` binds this review to the actual asset
hashes. Validation includes:

- Complete editable-object comparisons for Development, DetailCandidate and
  both distance sources: positions, corner normals, UV0, topology, morphs,
  transforms and unrelated material assignments are preserved.
- Independent GLB triangle-corner comparisons: the original white body equals
  the new white body plus fixed wing. Positions, normals, UV0 and tangents are
  exact. Only the wing material and its close-detail DetailUV change.
- All original material declarations, texture/image bytes and original binary
  data remain unchanged. Distance derivation digests refer to the new Hero.
- All 64 portable-export checks pass for each of the four exports, including
  Khronos validation, exact overall dimensions and live actuator alignment.
- Native conversion compares all 1,089,374 triangles across three variants,
  retaining hierarchy, winding, attributes, morphs, materials and images.
- Fifteen detail-switching checks pass, and protected surface positions and
  carbon normals agree across variants.
- The production native build passes 149 hidden checks, with zero failures.
  Steering-wheel hub drift remains below 0.0005 mm across 69 poses. The pose
  reference now includes the fixed wing and compares 2,572 node frames.

The native rear view is saved as
`screenshots/porsche-fixed-wing-carbon-rear.png`. Broad clearcoat glare made
the separate Blender studio views unsuitable as the only appearance evidence;
the finish was reviewed in the actual game renderer.

For isolated future candidates, standalone CMake accepts an optional
`BW_PORSCHE_ASSET_HEADER` path. Both native rig declarations include that
header when set. The default continues to use the generated production header;
the candidate executable and runtime asset directory can be kept separate.

This finish change does not resolve front tire/fender interference, establish
Forza-quality parity or 60 FPS, reconstruct OEM vehicle physics, or provide
GitHub write access. Those remain separate open requirements.
