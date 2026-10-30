# Front wheel packaging check

This offline test poses the actual game GLB with the source Three.js rig and
checks front tire/rim triangle surfaces against the front mechanisms. It samples
135 steering, left/right suspension and wheel-phase combinations, including
asymmetric travel. The native game does not need Node, Three.js or Blender.

From the repository root, with Node and Blender 4.5 installed:

```powershell
New-Item -ItemType Directory tests/porsche_front_fit/reports -Force
node tests/porsche_front_fit/scripts/build_front_mechanism_fixture.mjs assets/porsche/gt3rs_hero.glb tests/porsche_front_fit/reports/hero-fixture.json --packaging
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --threads 2 --python-exit-code 1 --python tests/porsche_front_fit/scripts/check_front_wheel_packaging.py -- --fixture reports/hero-fixture.json --report reports/hero-qa.json --aabb
```

Repeat with `gt3rs_near.glb` and `gt3rs_far.glb` for the distance variants.
Inspect the report's `passed` field. The AABB broad phase avoids Blender BVH's
triangle filter; nine analytic controls exercise the double-precision predicates.
The predicate functions match those used for the published clearance evidence.

This is a finite surface-crossing test. It excludes other body panels, solid
containment, rotor/pad interfaces and continuous motion between samples. It does
not establish manufacturer dimensions, collision physics or frame rate.

The source rig and test code accompany the adapted model; see
`../../assets/porsche/ATTRIBUTION.md`. Three.js is distributed under the MIT
license in `viewer/vendor/LICENSE-three.txt`.
