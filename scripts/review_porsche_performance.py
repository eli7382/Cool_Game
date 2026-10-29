"""Validate a native instrumented driving run and retain reproducible receipts."""
from pathlib import Path
import datetime
import hashlib
import json
import math
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def finite_numbers(value):
    if isinstance(value, dict):
        return all(finite_numbers(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_numbers(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


path = ROOT / 'docs/porsche-native-performance.json'
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
    image_path = ROOT / f"screenshots/porsche-perf-{case['name']}.png"
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
        off, on = [next(c for c in report['cases'] if c['high'] == high and c['cockpit'] == cockpit and c['ao'] == ao) for ao in [False, True]]
        position_error = max(abs(x - y) for a, b in zip(off['raw_frames'], on['raw_frames']) for x, y in zip(a['position'], b['position']))
        speed_error = max(abs(a['speed_mps'] - b['speed_mps']) for a, b in zip(off['raw_frames'], on['raw_frames']))
        steering_error = max(abs(a['steering_control'] - b['steering_control']) for a, b in zip(off['raw_frames'], on['raw_frames']))
        assert position_error < 1e-4 and speed_error < 1e-5 and steering_error < 1e-7
        assert [f['mirror_updates'] for f in off['raw_frames']] == [f['mirror_updates'] for f in on['raw_frames']]
        comparisons.append({'high': high, 'cockpit': cockpit, 'maximum_position_error_m': position_error,
                            'maximum_speed_error_mps': speed_error,
                            'maximum_steering_control_error': steering_error, 'matched_mirror_update_schedule': True,
                            'gpu_p50_on_minus_off_ms': on['gpu_frame_ms']['p50'] - off['gpu_frame_ms']['p50']})

sources = ['src/breakwater/porsche_profile.cpp', 'src/breakwater/porsche_profile.hpp',
           'src/breakwater/main.cpp', 'src/breakwater/world.cpp', 'src/breakwater/porsche_ao.cpp',
           'src/breakwater/porsche_native.cpp', 'src/breakwater/porsche_native.hpp']
assert all((ROOT / p).stat().st_mtime <= exe.stat().st_mtime for p in sources)
qa_path = ROOT / 'docs/porsche-native-qa.json'
qa = json.loads(qa_path.read_text())
assert qa['passed'] and qa['checks'] == 125 and qa['failures'] == 0
assert qa_path.stat().st_mtime >= exe.stat().st_mtime
thermal_log = (ROOT / '.tools/hidden-qa-session.log').read_text(errors='replace')
thermal = [line for line in thermal_log.splitlines() if 'qa-car-tire-thermal-result,' in line][-1]
assert 'pass=1,load-transfer-pass=1' in thermal
review = {'reviewed_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'stage': 'Instrumented native driving performance',
          'baseline_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
          'executable_sha256': sha(exe), 'source_hashes': {p: sha(ROOT / p) for p in sources},
          'reviewer_sha256': sha(Path(__file__)), 'runtime_evidence_sha256': sha(path),
          'gpu': report['renderer'], 'resolution': [report['width'], report['height']],
          'native_qa': qa, 'legacy_thermal_regression': thermal,
          'measured_frames': sum(len(c['raw_frames']) for c in report['cases']),
          'ordinary_distance_LOD_selection': True, 'fixed_step_driving_physics': True,
          'summary': summary, 'matched_driving_paths': comparisons, 'images': images,
          'local_instrumented_60fps_p95_target_met': all(c['frame_wall_ms']['p95'] <= 1000 / 60 for c in report['cases'] if c['ao']),
          'limits': report['limits'], 'complete': False, 'final_fidelity_verified': False}
(ROOT / 'docs/porsche-performance-review.json').write_text(json.dumps(review, indent=2) + '\n')
overview_path = ROOT / 'docs/porsche-native-review.json'
overview = json.loads(overview_path.read_text())
overview['stage'] = 'Native contact shading and measured driving performance baseline'
overview['performance_review'] = 'docs/porsche-performance-review.json'
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
                  'local_instrumented_60fps_p95_target_met': review['local_instrumented_60fps_p95_target_met']}))
