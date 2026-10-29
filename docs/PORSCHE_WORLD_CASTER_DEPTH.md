Static world caster depth pass
==============================

`DrawStaticWorldSunDepth(camera, projection)` renders the actual hangar,
tower, safehouse and sluice geometry into the caller's depth-enabled target.
It uses a depth-output branch in the existing world shader, with its existing
foliage alpha test and exclusion of water/translucent glass. It saves and
restores the scene camera and projection/modelview matrices and flushes the
render batch before resetting the shader mode.

The caller owns the render target, clear, depth mask, culling and blending
states. The callback does not call the main scene renderer or recursively
prepare Porsche shadows. It does not include terrain, vegetation, debris,
characters or other vehicles. The sluice drawing includes its current gate
and wheel state, so a future cache must include the relevant world state.

The native suite exercises a 512-square R32 target with an overhead hangar
view and a distant empty view. It checks finite normalized depths, nonempty
building coverage, an empty background, and the projected depth of an
interior roof point away from seams/vents against the authored roof height.
It also checks restoration of the main camera matrices/framebuffer. This is
limited depth-generation evidence, not a complete caster or shadow oracle.

Production integration
----------------------

The callback now feeds a separate world-space building map for each visible
Porsche. It uses a 16 m orthographic span, 0.1–100 m depth range, and a camera
40 m along the shared sun direction. High/low maps are 1024/512 square. The
car shader samples the map on explicit texture unit 7, applies receiver-plane
PCF using the existing self-shadow algorithm, and combines the two visibility
terms with their minimum. The building-map binding is cleared after each car
draw so it does not leak into other material passes or QA probes. Both map
slots are released during shutdown.

Maps currently regenerate on each preparation call. This avoids stale shadows
from the moving safehouse door and sluice parts, but adds rendering work.
Cache policy and frame time have not been validated. Building maps do not
replace the separate car-caster maps used to shadow the ground.

A receiver fixture compares the actual car shader at six exterior points
against independent positive rays through the broad hangar core and roof
boxes, away from narrow details. Three lit and three blocked points match
exactly in the initial run. This finite fixture does not validate every PCF
edge, all building details, vehicle body coverage or whole-scene appearance.
Native appearance review near buildings and performance measurement remain
required before considering this lighting work finished. Terrain, vegetation,
debris and other vehicles are still outside this caster callback's scope.
