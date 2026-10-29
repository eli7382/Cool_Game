"""Compare same-build cache-off/on runs for parked and moving workloads."""
import json,hashlib,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=[];sources=[];traces_equal=True
for route in ['parked','hangar']:
 paths=[root/f'docs/porsche-building-cache-{route}-{state}-performance.json' for state in ['off','on']]
 off,on=[json.loads(p.read_text()) for p in paths]
 assert off['passed'] and on['passed'] and len(off['cases'])==len(on['cases'])==8
 assert not off['building_shadow_cache_enabled'] and on['building_shadow_cache_enabled']
 for key in ['route','schema','vendor','renderer','GL_version','width','height','warmup_frames','sample_frames_per_case','simulation_step_s','stage_flush_instrumentation']:
  assert off[key]==on[key],(route,key)
 assert off['route']==route
 sources.extend({'file':str(p.relative_to(root)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths)
 for a,b in zip(off['cases'],on['cases']):
  for key in ['name','high','cockpit','ao','sample_frames']:assert a[key]==b[key]
  assert a['motion_and_render_state_passed'] and b['motion_and_render_state_passed']
  assert len(a['raw_frames'])==len(b['raw_frames'])==96
  same=all(all(x[k]==y[k] for k in ['position','speed_mps','steering_control']) for x,y in zip(a['raw_frames'],b['raw_frames']))
  traces_equal &= same
  updates=[sum(f['building_shadow_updates'] for f in c['raw_frames']) for c in [a,b]]
  assert all(isinstance(f['building_shadow_updates'],int) and 0<=f['building_shadow_updates']<=2 for c in [a,b] for f in c['raw_frames'])
  assert updates[0]==96 and updates[1]<=updates[0],(route,a['name'],updates)
  if route=='parked':assert updates[1]<=4,(a['name'],'unexpected parked map movement',updates)
  rows.append({'route':route,'case':a['name'],'ao':a['ao'],'identical_simulation_trace':same,
   'sample_frames':96,'building_map_updates':{'off':updates[0],'on':updates[1]},
   'building_cpu_submit_p50_ms':{'off':a['stages_ms']['building_shadows']['cpu_submit']['p50'],'on':b['stages_ms']['building_shadows']['cpu_submit']['p50']},
   'building_gpu_elapsed_p50_ms':{'off':a['stages_ms']['building_shadows']['gpu_elapsed']['p50'],'on':b['stages_ms']['building_shadows']['gpu_elapsed']['p50']},
   'frame_wall_p50_ms':{'off':a['frame_wall_ms']['p50'],'on':b['frame_wall_ms']['p50']}})
assert traces_equal,'Simulation changed between cache states'
report={'passed':True,'source_base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
 'source_sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in ['src/breakwater/porsche_native.cpp','src/breakwater/world.cpp','src/breakwater/porsche_profile.cpp','src/breakwater/porsche_shadow.cpp','src/breakwater/gameplay.cpp','src/breakwater/porsche_brake_hold.hpp']},
 'executable_sha256':hashlib.sha256((root/'build-native/ninja/bin/game.exe').read_bytes()).hexdigest(),
 'source_reports':sources,'all_simulation_traces_equal':traces_equal,'cases':rows,
 'scope':'Same compiled binary, hidden 1280x720 low/high chase/cockpit AO-off/on cases: brake-held parked inspection beside the hangar and the existing accelerating hangar route. Cache off then on for parked, on then off for moving. 24 warmup and 96 sampled frames per case.',
 'limits':['Sequential runs can include thermal and background variation; not an interleaved repeated benchmark',
 'Per-stage forced flushes and GPU queries add instrumentation overhead; GPU intervals can include idle gaps',
 'Whole-frame timings varied strongly and some cached medians were worse; this run does not prove an overall FPS gain',
 'Only the building shadow stage and update counts isolate the work removed; whole-frame differences have additional variation',
 'Not uninstrumented visible-window FPS or a 60 FPS guarantee','Front tire/fender contact and final asset appearance remain open'],
 'complete':False}
(root/'docs/porsche-building-cache-performance-review.json').write_text(json.dumps(report,indent=2)+'\n')
for row in rows:
 if row['ao']:print(row['route'],row['case'],'updates',row['building_map_updates'],'building CPU',row['building_cpu_submit_p50_ms'],'frame',row['frame_wall_p50_ms'])
