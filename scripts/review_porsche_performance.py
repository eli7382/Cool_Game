"""Validate a native instrumented driving run and retain reproducible receipts."""
from pathlib import Path
import datetime
import hashlib
import json
import math
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CULLING = '--culling' in sys.argv
FEATURE = 'culling' if CULLING else 'ao'
RUNTIME_NAME = 'porsche-native-culling-performance.json' if CULLING else 'porsche-native-performance.json'
REVIEW_NAME = 'porsche-culling-performance-review.json' if CULLING else 'porsche-performance-review.json'
IMAGE_PREFIX = 'porsche-culling-perf-' if CULLING else 'porsche-perf-'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def finite_numbers(value):
    if isinstance(value, dict):
        return all(finite_numbers(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_numbers(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


path = ROOT / 'docs' / RUNTIME_NAME
report = json.loads(path.read_text())
exe = ROOT / 'build-native/ninja/bin/game.exe'
assert report['passed'] and report['gpu_timer_queries'] and finite_numbers(report)
assert path.stat().st_mtime >= exe.stat().st_mtime
assert (report['width'], report['height']) == (1280, 720)
assert report['sample_frames_per_case'] >= 90 and report['warmup_frames'] >= 20
assert len(report['cases']) == 8
images = []
case_names = set()
summary = []
for case in report['cases']:
    assert case['name'] not in case_names
    case_names.add(case['name'])
    assert case['motion_and_render_state_passed'] and case['distance_m'] > 1
    if CULLING:
        assert report['comparison'] == 'mesh_culling' and case['ao']
        assert case['porsche_meshes_tested'] > 0 and case['hero_meshes_tested'] > 0
        assert (case['porsche_meshes_rejected'] + case['hero_meshes_rejected'] > 0) == case['culling']
    raw = case['raw_frames']
    assert len(raw) == case['sample_frames'] == report['sample_frames_per_case']
    assert all(f['gpu_ms'] > 0 and f['frame_wall_ms'] > 0 for f in raw)
    assert all(-1 <= f['steering_control'] <= 1 for f in raw)
    assert all(f['ao_updates'] == int(case['ao']) for f in raw)
    assert all(0 <= f['mirror_updates'] <= 1 for f in raw)
    assert sum(f['display_updates'] for f in raw) > 0
    if case['cockpit']:
        assert case['mirror_updates'] > 0
    stages = case['stages_ms']
    for name in ['scene_color', 'sun_shadows', 'simulation']:
        assert stages[name]['cpu_submit']['mean'] > 0
    for name in ['ambient_normal_depth', 'ambient_evaluation', 'ambient_filter']:
        assert (stages[name]['gpu_elapsed']['mean'] > 0) == case['ao']
    image_path = ROOT / f"screenshots/{IMAGE_PREFIX}{case['name']}.png"
    assert image_path.stat().st_mtime >= exe.stat().st_mtime
    with Image.open(image_path) as image:
        assert image.size == (report['width'], report['height'])
        assert len(image.getcolors(1280 * 720)) > 100
    images.append({'path': image_path.relative_to(ROOT).as_posix(), 'sha256': sha(image_path)})
    summary.append({'case': case['name'], 'frame_wall_p50_ms': case['frame_wall_ms']['p50'],
                    'frame_wall_p95_ms': case['frame_wall_ms']['p95'], 'gpu_p50_ms': case['gpu_frame_ms']['p50'],
                    'ambient_mean_ms': stages['ambient_total']['gpu_elapsed']['mean'],
                    'normal_depth_mean_ms': stages['ambient_normal_depth']['gpu_elapsed']['mean'],
                    'evaluation_mean_ms': stages['ambient_evaluation']['gpu_elapsed']['mean'],
                    'filter_mean_ms': stages['ambient_filter']['gpu_elapsed']['mean']})

comparisons = []
for high in [False, True]:
    for cockpit in [False, True]:
        off, on = [next(c for c in report['cases'] if c['high'] == high and c['cockpit'] == cockpit and c[FEATURE] == value) for value in [False, True]]
        position_error = max(abs(x - y) for a, b in zip(off['raw_frames'], on['raw_frames']) for x, y in zip(a['position'], b['position']))
        speed_error = max(abs(a['speed_mps'] - b['speed_mps']) for a, b in zip(off['raw_frames'], on['raw_frames']))
        steering_error = max(abs(a['steering_control'] - b['steering_control']) for a, b in zip(off['raw_frames'], on['raw_frames']))
        assert position_error < 1e-4 and speed_error < 1e-5 and steering_error < 1e-7
        assert [f['mirror_updates'] for f in off['raw_frames']] == [f['mirror_updates'] for f in on['raw_frames']]
        comparisons.append({'high': high, 'cockpit': cockpit, 'maximum_position_error_m': position_error,
                            'maximum_speed_error_mps': speed_error,
                            'maximum_steering_control_error': steering_error, 'matched_mirror_update_schedule': True,
                            'gpu_p50_on_minus_off_ms': on['gpu_frame_ms']['p50'] - off['gpu_frame_ms']['p50']})
        if CULLING:
            assert off['porsche_meshes_tested'] == on['porsche_meshes_tested']
            assert off['hero_meshes_tested'] == on['hero_meshes_tested']
            comparisons[-1].update({
                'frame_wall_p50_on_minus_off_ms': on['frame_wall_ms']['p50'] - off['frame_wall_ms']['p50'],
                'porsche_draws_skipped': on['porsche_meshes_rejected'], 'hero_draws_skipped': on['hero_meshes_rejected'],
                'eligible_mesh_draw_fraction_skipped': (on['porsche_meshes_rejected'] + on['hero_meshes_rejected']) / (on['porsche_meshes_tested'] + on['hero_meshes_tested'])})

sources = ['src/breakwater/porsche_profile.cpp', 'src/breakwater/porsche_profile.hpp',
           'src/breakwater/main.cpp', 'src/breakwater/world.cpp', 'src/breakwater/porsche_ao.cpp',
           'src/breakwater/porsche_native.cpp', 'src/breakwater/porsche_native.hpp']
if CULLING:
    sources += ['src/breakwater/mesh_visibility.cpp', 'src/breakwater/mesh_visibility.hpp',
                'src/breakwater/porsche_rig.cpp', 'src/breakwater/porsche_ao.hpp']
assert all((ROOT / p).stat().st_mtime <= exe.stat().st_mtime for p in sources)
qa_path = ROOT / 'docs/porsche-native-qa.json'
qa = json.loads(qa_path.read_text())
assert qa['passed'] and qa['checks'] == 125 and qa['failures'] == 0
assert qa_path.stat().st_mtime >= exe.stat().st_mtime
thermal_log = (ROOT / '.tools/hidden-qa-session.log').read_text(errors='replace')
thermal = [line for line in thermal_log.splitlines() if 'qa-car-tire-thermal-result,' in line][-1]
assert 'pass=1,load-transfer-pass=1' in thermal
review = {'reviewed_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'stage': 'Conservative mesh visibility optimization' if CULLING else 'Instrumented native driving performance',
          'baseline_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
          'executable_sha256': sha(exe), 'source_hashes': {p: sha(ROOT / p) for p in sources},
          'reviewer_sha256': sha(Path(__file__)), 'runtime_evidence_sha256': sha(path),
          'gpu': report['renderer'], 'resolution': [report['width'], report['height']],
          'native_qa': qa, 'legacy_thermal_regression': thermal,
          'measured_frames': sum(len(c['raw_frames']) for c in report['cases']),
          'ordinary_distance_LOD_selection': True, 'fixed_step_driving_physics': True,
          'summary': summary, 'matched_driving_paths': comparisons, 'images': images,
          'local_instrumented_60fps_p95_target_met': all(c['frame_wall_ms']['p95'] <= 1000 / 60 for c in report['cases'] if c[FEATURE]),
          'limits': report['limits'], 'complete': False, 'final_fidelity_verified': False}
if CULLING:
    qa_path = ROOT / 'docs/porsche-native-culling-qa.json'
    culling_qa = json.loads(qa_path.read_text())
    assert qa_path.stat().st_mtime >= exe.stat().st_mtime
    assert culling_qa['passed'] and culling_qa['image_pairs'] == 36
    assert all(culling_qa[k] == 0 for k in ['color_differing_pairs', 'normal_depth_differing_pairs', 'ambient_visibility_differing_pairs', 'mirror_target_differing_pairs', 'rejected_vertex_oracle_failures'])
    assert culling_qa['mirror_target_pairs'] >= 27 and culling_qa['rejected_vertices_verified'] > 1_000_000
    assert culling_qa['MSAA_culling_maximum_code_delta'] <= culling_qa['MSAA_unchanged_repeat_maximum_code_delta']
    review['visual_equivalence'] = culling_qa
    review['optimizations'] = ['Conservative posed mesh/frustum rejection using actual view, stack and projection matrices', 'Upload shader materials only for submitted meshes while preserving CPU bindings and shared map overrides', 'Updated spring bounds after morphing', 'Cached inverse rest matrices and per-draw posed transforms']
    review['timing_scope'] = 'Same executable with culling disabled/enabled; AO and other effects held on. Both sides include the inverse-rest and transform caches.'
    manifest = json.loads((ROOT / 'assets/porsche/manifest.json').read_text())
    for level in manifest['levels']:
        assert sha(ROOT / f"assets/porsche/gt3rs_{level['level']}.glb") == level['runtime_sha256']
    review['runtime_geometry_unchanged'] = True
    import numpy as np
    replay_images = []
    for high in [False, True]:
        for cockpit in [False, True]:
            pair = [next(c for c in report['cases'] if c['high'] == high and c['cockpit'] == cockpit and c['culling'] == value) for value in [False, True]]
            # The measured FPS text intentionally differs. Full-frame model
            # equivalence is separately covered by the dedicated color QA.
            arrays = [np.asarray(Image.open(ROOT / f"screenshots/{IMAGE_PREFIX}{c['name']}.png").convert('RGB'), dtype=np.int16)[:-30] for c in pair]
            delta = np.abs(arrays[1] - arrays[0])
            assert int(delta.max()) <= culling_qa['MSAA_unchanged_repeat_maximum_code_delta']
            replay_images.append({'high': high, 'cockpit': cockpit, 'excluded_FPS_footer_rows': 30,
                                  'maximum_color_code_delta': int(delta.max()),
                                  'pixels_over_unchanged_repeat_envelope': int(np.any(delta > culling_qa['MSAA_unchanged_repeat_maximum_code_delta'], axis=2).sum())})
    review['driving_image_comparisons'] = replay_images
    for name in ['cockpit', 'open']:
        for state in ['off', 'on']:
            p = ROOT / f'screenshots/porsche-culling-{name}-{state}.png'
            assert p.stat().st_mtime >= exe.stat().st_mtime
            review['images'].append({'path': p.relative_to(ROOT).as_posix(), 'sha256': sha(p), 'single_sample_color_with_AO': True})
(ROOT / 'docs' / REVIEW_NAME).write_text(json.dumps(review, indent=2) + '\n')
overview_path = ROOT / 'docs/porsche-native-review.json'
overview = json.loads(overview_path.read_text())
overview['stage'] = 'Native contact shading and measured driving performance baseline'
if CULLING:
    overview['stage'] = 'Conservative visibility and material-upload optimization'
    overview.setdefault('source_hashes', {}).update(review['source_hashes'])
    overview['mesh_visibility_review'] = 'docs/' + REVIEW_NAME
overview['performance_review'] = 'docs/' + REVIEW_NAME
overview['native_driving_performance'] = {
    'measured_frames': review['measured_frames'], 'renderer': review['gpu'],
    'resolution': review['resolution'], 'paired_paths_and_mirror_schedule_match': True,
    'local_instrumented_60fps_p95_target_met': review['local_instrumented_60fps_p95_target_met'],
    'optimization_complete': False}
limitation = 'Native instrumented driving baseline fails the 60 FPS target; repeated scene submission, mirror rendering and normal/depth pass require optimization'
if limitation not in overview.setdefault('limitations', []):
    overview['limitations'].append(limitation)
overview_path.write_text(json.dumps(overview, indent=2) + '\n')
print(json.dumps({'gpu': review['gpu'], 'measured_frames': review['measured_frames'], 'summary': summary,
                  'paired_comparisons': comparisons,
                  'local_instrumented_60fps_p95_target_met': review['local_instrumented_60fps_p95_target_met']}))
