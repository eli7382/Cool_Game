"""Compare the retained uncullled ON run with building-group culling enabled."""
import json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
paths=[root/'docs/porsche-building-shadow-on-performance.json',root/'docs/porsche-building-group-cull-performance.json']
before,after=[json.loads(p.read_text()) for p in paths]
for key in ['vendor','renderer','GL_version','width','height','sample_frames_per_case','simulation_step_s','stage_flush_instrumentation']:assert before[key]==after[key],key
assert len(before['cases'])==len(after['cases'])==8
rows=[];same=True
for a,b in zip(before['cases'],after['cases']):
 for key in ['name','high','cockpit','ao','sample_frames']:assert a[key]==b[key],key
 assert a['motion_and_render_state_passed'] and b['motion_and_render_state_passed']
 assert len(a['raw_frames'])==len(b['raw_frames'])==96
 trace=all(all(x[key]==y[key] for key in ['speed_mps','steering_control','position']) for x,y in zip(a['raw_frames'],b['raw_frames']));same &= trace
 row={'name':b['name'],'ao':b['ao'],'identical_driving_trace':trace}
 for key in ['cpu_submit','gpu_elapsed']:
  old=a['stages_ms']['building_shadows'][key]['p50'];new=b['stages_ms']['building_shadows'][key]['p50']
  row['building_'+key]={'before_p50_ms':old,'after_p50_ms':new,'change_percent':100*(new-old)/old if old else None}
 row['frame_wall_ms']={'before_p50':a['frame_wall_ms']['p50'],'after_p50':b['frame_wall_ms']['p50']};rows.append(row)
report={'source_reports':[{'path':str(p.relative_to(root)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],'identical_all_driving_traces':same,'cases':rows,'scope':'Retained pre-culling ON run versus one later group-culling ON run, same eight-case hidden instrumented route','limits':['Non-interleaved runs can contain thermal and background-load differences','Dedicated stage submission cost is more informative here than assigning all frame-time change to culling','Base-area route does not replace near-building movement measurements','Not uninstrumented normal-game FPS or cross-device performance'],'complete':False}
(root/'docs/porsche-building-group-performance-review.json').write_text(json.dumps(report,indent=2)+'\n')
for row in rows:
 if row['ao']:print(row['name'],row['building_cpu_submit'],row['building_gpu_elapsed'])
print('Identical driving traces:',same)
