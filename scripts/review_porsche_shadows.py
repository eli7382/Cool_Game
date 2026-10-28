"""Bind the native self-shadow review to the tested executable and captures.

Run after build-native.ps1, --qa-porsche and the hidden tire-thermal regression.
Image differences measure the matched captures; they do not replace inspection.
"""
from pathlib import Path
import datetime,hashlib,json,re
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,data):p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
qa=read(ROOT/'docs/porsche-native-qa.json')
probes=read(ROOT/'docs/porsche-native-shadow-probes.json')
state=read(ROOT/'docs/porsche-native-shadow-state.json')
assert qa['passed'] and qa['checks']==103 and qa['failures']==0
assert probes['passed'] and probes['analytic_ray_box_probes']==48 and probes['self_shadow_plane_pixels']==320
assert all(state[k] for k in ['targets_and_presets','camera_orbit_reuses_map','world_translation_reuses_local_map','moving_parts_and_body_rotation_update_depth','vehicle_slot_targets_are_separate','main_matrices_and_framebuffer_restored','posed_caster_bounds_inside_map'])
assert state['coverage_poses']==72
log=(ROOT/'.tools/porsche-qa-session.log').read_text(encoding='utf-8',errors='replace')
log=log[log.rfind('porsche-qa-check,name=dedicated-native-shader-and-linear-environment-ready'):]
checks=[{'name':a,'passed':b=='1'} for a,b in re.findall(r'porsche-qa-check,name=([^,\r\n]+),pass=(\d)',log)]
assert len(checks)==qa['checks'] and all(c['passed'] for c in checks)
thermal=(ROOT/'.tools/hidden-qa-session.log').read_text(encoding='utf-8',errors='replace')
thermal=[line for line in thermal.splitlines() if 'qa-car-tire-thermal-result,' in line][-1]
assert 'pass=1,load-transfer-pass=1' in thermal
exe=ROOT/'build-native/ninja/bin/game.exe'
assert (ROOT/'docs/porsche-native-qa.json').stat().st_mtime>=exe.stat().st_mtime
source_names=['src/breakwater/porsche_native.cpp','src/breakwater/porsche_native.hpp','src/breakwater/porsche_shadow.cpp','src/breakwater/porsche_shadow.hpp','src/breakwater/world.cpp','assets/porsche/shaders/porsche.fs','assets/porsche/shaders/porsche_depth.vs','assets/porsche/shaders/porsche_depth.fs']
for name in source_names:assert (ROOT/name).stat().st_mtime<=(ROOT/'docs/porsche-native-qa.json').stat().st_mtime
images=[];differences=[]
for name in ['exterior','rear','cockpit','open']:
    pair=[]
    for setting in ['off','on']:
        path=f'screenshots/porsche-shadow-{name}-{setting}.png'
        pair.append(np.array(Image.open(ROOT/path).convert('RGB')).astype(int))
        images.append({'path':path,'sha256':sha(ROOT/path),'view':name,'self_shadows':setting=='on'})
    delta=pair[1]-pair[0];changed=int(np.any(abs(delta)>2,axis=2).sum());bright=int(np.any(delta>2,axis=2).sum())
    assert changed>100 and bright==0
    differences.append({'view':name,'changed_pixels_over_2_levels':changed,'brightened_pixels_over_2_levels':bright,'maximum_darken':int(-delta.min()),'maximum_brighten':int(delta.max())})
write(ROOT/'docs/porsche-shadow-image-comparison.json',differences)
manifest=read(ROOT/'assets/porsche/manifest.json')
for level in manifest['levels']:assert sha(ROOT/f"assets/porsche/gt3rs_{level['level']}.glb")==level['runtime_sha256']
report={'reviewed_at':stamp,'stage':'Native directional sun self-shadowing with receiver-plane filtered comparisons',
    'baseline_commit':'dcebb8c80d1aa33feaaef635300cef376036601d',
    'executable_sha256':sha(exe),'reviewer_sha256':sha(Path(__file__)),'source_hashes':{p:sha(ROOT/p) for p in source_names},
    'asset_source_hashes':{m['level']:m['source_sha256'] for m in manifest['levels']},
    'native_qa':qa,'new_checks':[c for c in checks if c['name'] in ['native-sun-shadow-depth-and-lookup-match-analytic-occluders','native-shadow-high-and-low-targets-ready','parked-shadow-cache-reuses-map-under-camera-and-world-translation','moving-parts-and-body-orientation-refresh-shadow-depth','shadow-targets-follow-independent-vehicle-slots','shadow-pass-restores-main-camera-matrices-and-framebuffer','all-posed-car-caster-bounds-fit-shadow-map','export-matched-native-shadow-review-view']],
    'gpu_probes':probes,'car_state_and_bounds':state,'images':images,'matched_image_differences':differences,
    'render_policy':{'high_map':2048,'low_map':1024,'format':'R32F color depth plus depth renderbuffer','car_relative_span_m':7.2,'filter':'3x3 nearest comparisons, each evaluated at its actual texel centre and receiver-plane depth','normal_offset':'Small slope-aware geometric-normal offset plus constant depth epsilon','casters':'Posed source meshes and spring morphs; glazing, blended layers, fine grille wires, coplanar decals and live screens excluded; masked alpha retained','cache':'Quantized sun-local projection, caster transforms, spring weights, caster level and preset; unchanged world translation and camera orbit reuse the local map','order':'Before scene mirrors and main view','lighting':'Direct sun base and clearcoat only; existing environment and emission contributions retained'},
    'visual_observations':['Matched exterior and open-door views show recessed and overlapping parts shading one another','The door mirror casts a visible shadow onto the opened white door','Receiver-plane filtering removes the initial fine banding on broad hood and dashboard surfaces','Cockpit overlaps shade the dashboard without globally darkening the instrument emission','Source geometry, rig datums, material textures and the corrected horn-hub steering frame are unchanged'],
    'legacy_regression':{'passed':True,'evidence':thermal},
    'remaining':['Directional cast shadows on ground/world receivers and world-to-car occlusion','Local ambient occlusion and further glass/material/optical refinement','Full-game frame time, loading policy and memory review','Porsche-specific vehicle tuning, whole-model clearance and requested final fidelity'],
    'complete':False,'final_fidelity_verified':False}
write(ROOT/'docs/porsche-shadow-review.json',report)
for name in ['porsche-native-review.json','porsche-native-displays-qa.json','porsche-lighting-review.json','porsche-mirrors-review.json','porsche-steering-hub-review.json']:
    p=ROOT/'docs'/name;d=read(p);d['reviewed_at']=stamp
    for key in ['executable_sha256','current_executable_sha256','native_executable_sha256']:
        if key in d:d[key]=sha(exe)
    for image in d.get('images',[])+d.get('matched_views',[]):
        if image.get('path') and (ROOT/image['path']).exists():image['sha256']=sha(ROOT/image['path'])
    if 'native_qa'in d:d['native_qa']=qa
    if name=='porsche-native-review.json':
        d['stage']=report['stage'];d['source_hashes'].update(report['source_hashes']);d['native_self_shadows']={'review':'docs/porsche-shadow-review.json','analytic_gpu_probes':48,'sloped_plane_pixels':320,'posed_car_coverage_cases':72}
        d['limitations']=[x.replace('self-shadowing and close surface detailing','local ambient occlusion, ground/world cast shadows and close surface detailing') for x in d['limitations']]
        if 'native_lighting'in d:d['native_lighting']['limits']='Finite IBL probes and separate scene-mirror/self-shadow fixtures plus matched visual review; final fidelity and full-game performance remain unverified'
    if name=='porsche-lighting-review.json':d['remaining']=[x.replace('Vehicle self-shadowing and local ambient occlusion','Ground/world cast shadows and local ambient occlusion') for x in d['remaining']]
    if name=='porsche-mirrors-review.json':
        d['source_projection_validation']=read(ROOT/'docs/porsche-native-mirror-projection-qa.json');d['native_scene_validation']=read(ROOT/'docs/porsche-native-mirrors-qa.json')
    write(p,d)
print(json.dumps({'native_checks':qa['checks'],'gpu_visibility_probes':48,'sloped_plane_pixels':320,'coverage_cases':72,'matched_views':4,'complete':False}))
