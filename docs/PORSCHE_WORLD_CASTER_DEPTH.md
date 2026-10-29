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

This callback is not yet connected to production Porsche shadow preparation.
It does not currently change car lighting. The next stage needs a world-space
shadow volume, receiver bindings, cache invalidation, independent GPU/ray
receiver comparisons, rendered appearance review and performance measurement.
