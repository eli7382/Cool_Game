"""Review sequential instrumented building-map off/on runs; not a FPS guarantee."""
import json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
paths=[root/'docs'/f'porsche-building-shadow-{state}-performance.json' for state in ['off','on']]
off,on=[json.loads(p.read_text()) for p in paths]
for key in ['vendor','renderer','GL_version','width','height','sample_frames_per_case','simulation_step_s','stage_flush_instrumentation']:
 assert off[key]==on[key],key
assert len(off['cases'])==len(on['cases'])==8
rows=[];same_trace=True
for a,b in zip(off['cases'],on['cases']):
 for key in ['name','high','cockpit','ao','culling','sample_frames']:assert a[key]==b[key],key
 assert a['motion_and_render_state_passed'] and b['motion_and_render_state_passed']
 assert len(a['raw_frames'])==len(b['raw_frames'])==96
 trace=all(all(x[k]==y[k] for k in ['speed_mps','steering_control','position']) for x,y in zip(a['raw_frames'],b['raw_frames']))
 same_trace &= trace
 row={k:b[k] for k in ['name','high','cockpit','ao']};row['identical_driving_trace']=trace
 for key in ['frame_wall_ms','gpu_frame_ms']:
  row[key]={'off_p50':a[key]['p50'],'on_p50':b[key]['p50'],'delta_p50':b[key]['p50']-a[key]['p50']}
 row['building_stage']={state:case['stages_ms']['building_shadows'] for state,case in [('off',a),('on',b)]}
 rows.append(row)
report={'source_reports':[{ 'path':str(p.relative_to(root)), 'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],
 'scope':'One sequential OFF then ON pair, same executable, BW_PORSCHE_BUILDING_SHADOWS=0/1; hidden instrumented eight-case benchmark, 24 warmup and 96 sampled frames per case',
 'identical_all_driving_traces':same_trace,'cases':rows,
 'limits':['Per-stage flushing and GPU timestamps add overhead','Single sequential pair can contain warmup, thermal and background-load differences; repeat before generalizing','This is not uninstrumented visible-game FPS or a cross-device guarantee','Cases follow the existing base-area route, not a dedicated hangar shadow-edge route'],'complete':False}
out=root/'docs/porsche-building-shadow-performance-review.json';out.write_text(json.dumps(report,indent=2)+'\n')
for row in rows:
 if row['ao']:print(row['name'],'frame off/on',row['frame_wall_ms']['off_p50'],row['frame_wall_ms']['on_p50'],'building CPU/GPU on',row['building_stage']['on']['cpu_submit']['p50'],row['building_stage']['on']['gpu_elapsed']['p50'])
print('identical driving traces:',same_trace)
