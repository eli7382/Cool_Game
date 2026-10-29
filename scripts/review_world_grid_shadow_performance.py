"""Review current shared/world/grid shadows versus retained car-only runs."""
import json,hashlib,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
routes=[('base','porsche-building-group-cull-performance.json','porsche-world-grid-base-performance.json'),('hangar','porsche-hangar-group-on-performance.json','porsche-world-grid-hangar-performance.json')]
review=[];sources=[];same=True
for route,old_name,new_name in routes:
 paths=[root/'docs'/old_name,root/'docs'/new_name];old,new=[json.loads(p.read_text()) for p in paths]
 for p in paths:sources.append({'path':str(p.relative_to(root)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 assert old.get('route','base')==new.get('route','base')==route
 for key in ['vendor','renderer','GL_version','width','height','sample_frames_per_case','simulation_step_s','stage_flush_instrumentation']:assert old[key]==new[key],key
 assert old['passed'] and new['passed'] and len(old['cases'])==len(new['cases'])==8
 for a,b in zip(old['cases'],new['cases']):
  for key in ['name','high','cockpit','ao','sample_frames']:assert a[key]==b[key],key
  assert a['motion_and_render_state_passed'] and b['motion_and_render_state_passed']
  assert len(a['raw_frames'])==len(b['raw_frames'])==96
  trace=all(all(x[k]==y[k] for k in ['speed_mps','steering_control','position']) for x,y in zip(a['raw_frames'],b['raw_frames']));same &= trace
  row={'route':route,'name':b['name'],'ao':b['ao'],'identical_driving_trace':trace}
  for key in ['frame_wall_ms','gpu_frame_ms']:row[key]={'historical_p50':a[key]['p50'],'current_p50':b[key]['p50']}
  row['stages']={}
  for stage in ['building_shadows','scene_color','mirrors','ambient_total']:
   row['stages'][stage]={}
   for metric in ['cpu_submit','gpu_elapsed']:row['stages'][stage][metric]={'historical_p50':a['stages_ms'][stage][metric]['p50'],'current_p50':b['stages_ms'][stage][metric]['p50']}
  review.append(row)
report={'current_source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'current_source_sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in ['src/breakwater/porsche_native.cpp','src/breakwater/porsche_shadow.cpp','src/breakwater/world.cpp']},'source_reports':sources,'identical_all_driving_traces':same,'current_configuration':{'building_span_m':48,'world_receivers':True,'light_space_grid':True,'group_culling':True},'cases':review,'scope':'Two current hidden instrumented routes, compared to retained earlier 16 m car-only building-map runs. Difference includes world receivers, wider coverage and grid stabilization, not one isolated change.','limits':['Historical versus current non-interleaved runs can include thermal/background variation','Nested stages and forced batch flushes include instrumentation overhead; GPU intervals can include CPU submission gaps','No claim that every frame-time difference is caused by the shadow changes','Not uninstrumented visible-game FPS, all-world workloads or cross-device performance'],'complete':False}
(root/'docs/porsche-world-grid-performance-review.json').write_text(json.dumps(report,indent=2)+'\n')
for row in review:
 if row['ao']:print(row['route'],row['name'],'frame',row['frame_wall_ms'],'building CPU',row['stages']['building_shadows']['cpu_submit'],'scene GPU',row['stages']['scene_color']['gpu_elapsed'])
print('Identical driving traces:',same)
