"""Preserve raw before/after runs and record their practical limits."""
import json,hashlib,re,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
geometry=read(root/'docs/pickup-wheel-cache-geometry-review.json');assert geometry['passed']
qa=read(root/'docs/porsche-native-qa.json');assert qa['passed'] and qa['checks']==142
logs=(root/'.tools/porsche-qa-session.log').read_text();latest=logs[logs.rfind(',session-start'):];checks=[s for s in latest.splitlines() if ',porsche-qa-check,' in s];assert len(checks)==142 and all('pass=1' in s for s in checks)
runs=[]
for label,file in [('initial-off','pickup-wheel-cache-baseline-performance'),('initial-on','pickup-wheel-cache-candidate-performance'),('repeat-off','pickup-wheel-cache-repeat-off-performance'),('repeat-on','pickup-wheel-cache-repeat-on-performance')]:
 src=root/f'.tools/{file}.json';r=read(src);assert r['passed'] and len(r['cases'])==8 and all(c['motion_and_render_state_passed'] for c in r['cases'])
 dest=root/f'docs/pickup-wheel-cache-{label}-performance.json';shutil.copy2(src,dest);runs.append({'label':label,'file':str(dest.relative_to(root)),'sha256':sha(dest),'data':r})
rows=[]
for a,b in [(runs[0],runs[1]),(runs[2],runs[3])]:
 for before,after in zip(a['data']['cases'],b['data']['cases']):
  assert before['name']==after['name']
  rows.append({'comparison':a['label']+' to '+b['label'],'case':before['name'],'before_frame_median_ms':before['frame_wall_ms']['p50'],'after_frame_median_ms':after['frame_wall_ms']['p50'],'before_pickup_cpu_submit_mean_ms':before['stages_ms']['scene_extra_car']['cpu_submit']['mean'],'after_pickup_cpu_submit_mean_ms':after['stages_ms']['scene_extra_car']['cpu_submit']['mean']})
cache_log=(root/'.tools/pickup-wheel-cache-repeat-on.log').read_text();match=re.search(r'Pickup wheel cache: builds=(\d+) draws=(\d+) vertices=(\d+)',cache_log);assert match
counts=dict(zip(['builds','draws','vertices'],map(int,match.groups())));assert counts['builds']==8 and counts['draws']>0
default_log=(root/'.tools/pickup-wheel-cache-default-capture.log').read_text();assert 'Pickup wheel cache:' in default_log
default_match=re.search(r'Pickup wheel cache: builds=(\d+) draws=(\d+) vertices=(\d+)',default_log);assert default_match and int(default_match[2])>0
# Preserve the earlier category-only baseline referenced by its original review.
baseline=root/'docs/porsche-vehicle-stage-baseline-performance.json';shutil.copy2(root/'.tools/pickup-wheel-cache-baseline-performance.json',baseline)
stage=root/'docs/porsche-vehicle-stage-review.json';stage_review=read(stage);assert stage_review['performance_report_sha256']==sha(baseline);stage_review['performance_report_path']=str(baseline.relative_to(root));stage.write_text(json.dumps(stage_review,indent=2)+'\n')
# The latest measured enabled path is now the production default. Its explicit
# feature selection remains recorded in this review and the unmodified raw run.
shutil.copy2(root/'.tools/pickup-wheel-cache-repeat-on-performance.json',root/'docs/porsche-native-performance.json')
report={'enabled_by_default':True,'disable_for_comparison':'BW_PICKUP_WHEEL_CACHE=0','explicit_benchmark_selection':'BW_PICKUP_WHEEL_CACHE=1 for enabled runs; 0 for repeat-disabled run','runs':[{k:v for k,v in r.items() if k!='data'} for r in runs],'comparisons':rows,'repeat_cache_counters':counts,'geometry_review_sha256':sha(root/'docs/pickup-wheel-cache-geometry-review.json'),'native_checks':142,'native_failures':0,'final_validation_executable_sha256':sha(root/'build-native/ninja/bin/game.exe'),'captures':{n:sha(root/'screenshots'/n) for n in ['pickup-wheel-cache-off.png','pickup-wheel-cache-on.png','porsche-pickup-cache-high-chase.png']},'visual_review':'Inspected cached chase view and disabled/enabled steered wheel close-ups. Tread, rotors, calipers and hub geometry retained. Explicit sphere/cylinder normals intentionally alter their lighting; pixel identity is not claimed.','limits':['Sequential local comparisons with varying GPU scheduling/temperature; not randomized ABBA trials or a cross-device FPS guarantee','One high-cockpit AO-off repeat regressed; the pickup benefit is concentrated in chase views where it is visible','Raw performance runs measured the enabled override before the default switch; the final default path separately passed 142 native checks','Geometry equivalence evidence samples surfaces in both directions; articulation uses the retained wheel matrix stack and was visually checked in the steered close-up','Pickup cached geometry is about 14 MB of combined CPU/GPU vertex payload; cold construction occurs before measured frames','Does not resolve Porsche tire/fender interference, reach 60 FPS, or establish final Forza-level fidelity'],'complete':False}
report['default_capture_cache_counters']=dict(zip(['builds','draws','vertices'],map(int,default_match.groups())))
report['default_capture_log_sha256']=sha(root/'.tools/pickup-wheel-cache-default-capture.log')
report['native_regression_scope']='142 Porsche checks; the standard isolated Porsche fixture does not exercise this pickup cache. Separate ordinary pickup-view captures and the 32 performance cases exercise the cache and its legacy path.'
(root/'docs/pickup-wheel-cache-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'native_checks':142,'cache':counts,'run_count':len(runs)}),flush=True)
