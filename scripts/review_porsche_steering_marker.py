"""Bind the accepted steering-marker delivery to actual source and native evidence."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=root.parent/'Porsche_911');source=parser.parse_args().source.resolve()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as stream:
  for chunk in iter(lambda:stream.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
native=read(root/'docs/porsche-native-qa.json');culling=read(root/'docs/porsche-native-culling-qa.json');conversion=read(root/'docs/porsche-asset-validation.json');performance=read(root/'docs/porsche-native-performance.json')
assert native['passed'] and native['checks']==155 and native['failures']==0 and native['native_steering_poses']==69
assert native['maximum_axial_drift_m']<1e-6 and native['maximum_radial_drift_m']<1e-6
assert culling['passed'] and culling['image_pairs']==36 and culling['mirror_target_pairs']==54
assert all(culling[k]==0 for k in ['color_differing_pairs','normal_depth_differing_pairs','ambient_visibility_differing_pairs','mirror_target_differing_pairs'])
assert conversion['passed'] and performance['passed'] and performance['hidden_window'] and len(performance['cases'])==8
exe=root/'build-native/ninja/bin/game.exe'
for name in ['porsche-native-qa.json','porsche-native-culling-qa.json','porsche-native-performance.json']:assert (root/'docs'/name).stat().st_mtime>=exe.stat().st_mtime
scope=read(source/'reports/steering-marker-export-scope.json');correspondence=read(source/'reports/steering-marker-source-correspondence.json');motion=read(source/'reports/steering-marker-motion.json');promotion=read(source/'reports/steering-marker-promotion.json');package=read(source/'reports/steering-marker-source-package.json')
assert all(x['passed'] for x in [scope,correspondence,motion]) and promotion['promoted']
assert sum(r['poses'] for r in motion['levels'])==138
manifest=read(source/'exports/gt3rs-lods.json');switching=read(source/'reports/lod-runtime.json');protected=read(source/'reports/lod-protected-surfaces.json')
assert switching['passed'] and switching['checks']==15 and protected['passed'] and len(protected['results'])==18
assert switching['hero_sha256']==protected['hero_sha256']==manifest['hero_sha256']
assert sha(root/'assets/porsche/Porsche_911_GT3RS_Source.blend')==package['package_sha256']
assert sha(source/'Porsche_911_GT3RS_Development.blend')==package['source_sha256']
assert package['used_images_packed']==52
levels=[]
for level,label in [('hero','Development'),('near','LOD1'),('far','LOD2')]:
 exported=source/f'exports/Porsche_911_GT3RS_{label}.glb';runtime=root/f'assets/porsche/gt3rs_{level}.glb'
 original=next(r for r in scope['levels'] if r['level']==label);author=next(r for r in correspondence['levels'] if r['level']==label);converted=next(r for r in conversion['levels'] if r['level']==level)
 assert sha(exported)==original['candidate_sha256']==author['runtime_sha256']==converted['source_sha256']==manifest['levels'][level]['sha256']
 assert sha(runtime)==converted['runtime_sha256']
 assert converted['stock_loader_texture_references']==0 and converted['full_materials_retained_in_native_metadata']
 assert max(p['maximum_position_error_m'] for p in author['parts'])==0 and max(p['maximum_normal_vector_error'] for p in author['parts'])==0
 levels.append({'level':level,'source_export_sha256':sha(exported),'native_sha256':sha(runtime),'triangles':converted['triangles_compared'],'native_primitives':converted['primitives_checked'],'marker_triangles':150,'marker_width_m':original['marker_width_m']})
assert read(root/'assets/porsche/manifest.json')['generator_sha256']==sha(root/'scripts/prepare_porsche_assets.py')
destination=root/'docs/porsche-steering-marker';destination.mkdir(exist_ok=True);evidence=[]
for name in ['steering-marker-export-scope','steering-marker-source-correspondence','steering-marker-motion','steering-marker-source-package','steering-marker-promotion','lod-runtime','lod-protected-surfaces']+[f'steering-marker-audit-{label}' for label in ['Development','DetailCandidate','LOD1','LOD2']]+[f'steering-marker-authoring-{label}' for label in ['Development','DetailCandidate','LOD1','LOD2']]:
 path=source/'reports'/f'{name}.json';target=destination/path.name;shutil.copy2(path,target);evidence.append({'file':str(target.relative_to(root)),'sha256':sha(target)})
for mode in ['qa','culling','performance']:
 path=root/f'.tools/porsche-marker-{mode}-resources.json';record=read(path);assert record['exit_code']==0
 target=destination/path.name;shutil.copy2(path,target);evidence.append({'file':str(target.relative_to(root)),'sha256':sha(target)})
captures=[]
for name in ['porsche-native-front.png','porsche-native-live-cockpit.png','porsche-native-steering-centre.png','porsche-native-steering-negative.png','porsche-native-steering-positive.png']:
 path=root/'screenshots'/name;assert path.stat().st_mtime>=exe.stat().st_mtime
 captures.append({'file':str(path.relative_to(root)),'sha256':sha(path)})
before=root/'screenshots/porsche-steering-before-marker.png';before.write_bytes(subprocess.check_output(['git','show','038cef9:screenshots/porsche-native-steering-centre.png'],cwd=root))
shutil.copy2(exe,root/'dist/Breakwater_Windows_x64.exe');assert sha(exe)==sha(root/'dist/Breakwater_Windows_x64.exe')
timings=[{'case':r['name'],'mean_frame_ms':r['frame_wall_ms']['mean'],'p95_frame_ms':r['frame_wall_ms']['p95'],'mean_gpu_ms':r['gpu_frame_ms']['mean'] if r['gpu_frame_ms'] else None} for r in performance['cases']]
report={'implemented':True,'baseline_commit':'038cef913395edefdd298b2d735689c40de7c522','levels':levels,'source_package':package,'native_checks':native['checks'],'native_steering_poses':69,'maximum_native_hub_axial_drift_m':native['maximum_axial_drift_m'],'maximum_native_hub_radial_drift_m':native['maximum_radial_drift_m'],'additional_marker_motion_poses':138,'detail_switch_cases':15,'culling_image_pairs':36,'mirror_target_pairs':54,'stock_loader_texture_references':0,'source_materials_and_image_bytes_retained':True,'executable_sha256':sha(exe),'code_hashes':{p:sha(root/p) for p in ['src/breakwater/porsche_native.cpp','src/breakwater/porsche_asset_data.hpp','src/breakwater/porsche_displays.cpp','scripts/prepare_porsche_assets.py','tests/check_porsche_assets.py']},'captures':captures,'before':{'file':str(before.relative_to(root)),'sha256':sha(before),'commit':'038cef9'},'evidence':evidence,'performance':{'file':'docs/porsche-native-performance.json','sha256':sha(root/'docs/porsche-native-performance.json'),'renderer':performance['renderer'],'resolution':[performance['width'],performance['height']],'warmup_frames':performance['warmup_frames'],'sample_frames_per_case':performance['sample_frames_per_case'],'cases':timings},'visual_review':'Native centre and both steering-direction captures inspected. Yellow band is flush with the existing rim crown and rotates with the wheel; white/red exterior design retained.','reference':'https://newsroom.porsche.com/en/2022/products/porsche-911-gt3-rs-world-premiere-29177.html','limits':['Finite source, motion, native and image coverage; not whole-car continuous collision clearance','Source-fitted visual joints and game controls, not OEM CAD or validated Porsche tire forces','Hidden-window instrumented frame times include flush/query overhead and depend on this machine; visible play and other hardware are separate','Final Forza-level visual parity remains unproven'],'complete':False}
(root/'docs/porsche-steering-marker-review.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'levels':levels,'native_checks':155,'executable_sha256':sha(exe),'performance':timings}),flush=True)
