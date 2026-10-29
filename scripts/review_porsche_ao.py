"""Record native main-view contact shading after its GPU/scene QA passes."""
from pathlib import Path
import datetime,hashlib,json,re,subprocess,sys
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
qa=read(ROOT/'docs/porsche-native-qa.json');fixtures=read(ROOT/'docs/porsche-native-ao-fixtures.json');scene=read(ROOT/'docs/porsche-native-ao-scene.json')
assert qa['passed'] and qa['checks']==125 and qa['failures']==0
assert fixtures['passed'] and fixtures['linear_depth_ray_comparisons']>=100 and fixtures['maximum_linear_depth_error_m']<.002
assert fixtures['minimum_flat_plane_visibility']>.995 and fixtures['minimum_cube_contact_visibility']<.95 and fixtures['finite_unit_range']
assert all(scene[k] for k in ['glazing_excluded','visible_steering_changes_depth_and_AO','finite_visibility_and_normals','UI_matrices_restored'])
assert scene['maximum_unit_normal_error']<.001 and scene['pixels_below_095_visibility']>100
log=(ROOT/'.tools/porsche-qa-session.log').read_text(encoding='utf-8',errors='replace');log=log[log.rfind('porsche-qa-check,name=dedicated-native-shader-and-linear-environment-ready'):]
checks=[{'name':a,'passed':b=='1'} for a,b in re.findall(r'porsche-qa-check,name=([^,\r\n]+),pass=(\d)',log)]
assert len(checks)==qa['checks'] and all(c['passed'] for c in checks)
thermal=(ROOT/'.tools/hidden-qa-session.log').read_text(encoding='utf-8',errors='replace');thermal=[line for line in thermal.splitlines() if 'qa-car-tire-thermal-result,' in line][-1];assert 'pass=1,load-transfer-pass=1' in thermal
exe=ROOT/'build-native/ninja/bin/game.exe';assert (ROOT/'docs/porsche-native-qa.json').stat().st_mtime>=exe.stat().st_mtime
images=[];differences=[]
for view in ['exterior','cockpit','open']:
    pair=[]
    for state in ['off','on']:
        p=f'screenshots/porsche-ao-{view}-{state}.png';assert (ROOT/p).stat().st_mtime>=exe.stat().st_mtime
        pair.append(np.array(Image.open(ROOT/p).convert('RGB')).astype(int));images.append({'path':p,'sha256':sha(ROOT/p),'view':view,'contact_shading_enabled':state=='on','directional_shadows_enabled':True})
    d=pair[1]-pair[0];changed=int(np.any(abs(d)>2,axis=2).sum());brightened=int(np.any(d>2,axis=2).sum());assert changed>100 and brightened==0
    differences.append({'view':view,'changed_pixels_over_2_levels':changed,'brightened_pixels_over_2_levels':brightened,'maximum_darken':int(-d.min()),'maximum_brighten':int(d.max())})
for name in ['plane-fixture','far-plane-fixture','cube-fixture','cockpit-visibility']:
    p=f'screenshots/porsche-ao-{name}.png';assert (ROOT/p).stat().st_mtime>=exe.stat().st_mtime;images.append({'path':p,'sha256':sha(ROOT/p),'view':name})
manifest=read(ROOT/'assets/porsche/manifest.json')
for level in manifest['levels']:assert sha(ROOT/f"assets/porsche/gt3rs_{level['level']}.glb")==level['runtime_sha256']
source_names=['src/breakwater/porsche_ao.cpp','src/breakwater/porsche_ao.hpp','src/breakwater/porsche_native.cpp','src/breakwater/porsche_native.hpp','src/breakwater/world.cpp','assets/porsche/shaders/porsche.fs','assets/porsche/shaders/porsche_depth.vs','assets/porsche/shaders/porsche_depth.fs','assets/porsche/shaders/porsche_ao.fs','assets/porsche/shaders/porsche_ao_blur.fs','assets/porsche/shaders/porsche_ao_lookup.glsl']
for p in source_names:assert (ROOT/p).stat().st_mtime<=(ROOT/'docs/porsche-native-qa.json').stat().st_mtime
stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
report={'reviewed_at':stamp,'stage':'Main-view dynamic screen-space contact shading','baseline_commit':'0d74f38d98b2a8af2f7743dfd112bd9d5ff67169','executable_sha256':sha(exe),'source_hashes':{p:sha(ROOT/p) for p in source_names},'reviewer_sha256':sha(Path(__file__)),
    'native_qa':qa,'gpu_fixtures':fixtures,'scene_checks':scene,'images':images,'matched_image_differences':differences,
    'new_checks':[c for c in checks if c['name'] in ['floating-AO-depth-normal-plane-and-contact-fixtures','export-matched-native-contact-shading-view','main-view-floating-normal-depth-and-AO-targets-ready','moving-only-the-glazing-does-not-create-AO-occluders','posed-steering-updates-visible-depth-and-contact-shading','actual-scene-AO-buffers-have-finite-unit-normals-and-visibility','low-preset-computes-contact-shading-at-half-resolution','AO-depth-compute-and-blur-restore-main-UI-matrices']],
    'render_policy':{'normal_depth':'Full-resolution RGBA32F world normals and positive linear view depth; color blending disabled','geometry':'Actual posed opaque car and world geometry; alpha masks retained; glazing, water and blended layers excluded','evaluation':'Half-resolution R32F; cosine-hemisphere samples with a normal-facing rejection and 3D distance falloff; reconstruct at the actual sampled depth-texel centres','high_samples':32,'low_samples':16,'radius_m':.28,'bias_m':.004,'filter':'7-tap horizontal and vertical depth/normal bilateral blur, visibility clamped to [0,1], then a four-sample edge-aware upsample','application':'Indirect/environment diffuse and roughness-dependent specular visibility; direct sun, local lamps and emission contributions retained','specular_reference':'https://google.github.io/filament/Filament.html','samplers':[14,15],'scope':'Camera-matched main framebuffer while a Porsche is within 24 m; mirror and other offscreen views disabled','state':'Matrices/framebuffer restored; shader readiness, depth mode and sampler bindings cleared; reflection rendering preserves main adaptive grille state'},
    'observations':['Matched cockpit captures show contact shading around vents, steering controls, trim and console overlaps','Exterior/open views preserve the white/red design while adding depth in local recesses','Windows-only motion produces identical depth and visibility buffers; steering motion changes both','The fixture has full visibility on a lone plane and contact occlusion beside a cube','Geometry, textures, source rig and corrected steering pivot are unchanged'],
    'legacy_regression':{'passed':True,'evidence':thermal},'remaining':['Offscreen occluders and AO in planar reflections','Temporal stability, silhouette/halo tuning and full-game frame time/memory review','World-to-car directional occlusion and remaining glass/material/optical refinements','Porsche-specific vehicle tuning, whole-model clearance and requested final fidelity'],'complete':False,'final_fidelity_verified':False}
write(ROOT/'docs/porsche-ao-review.json',report)
subprocess.run([sys.executable,str(ROOT/'scripts/review_porsche_world_shadows.py')],cwd=ROOT,check=True,capture_output=True)
for name in ['porsche-native-review.json','porsche-lighting-review.json','porsche-shadow-review.json','porsche-world-shadow-review.json']:
    p=ROOT/'docs'/name;d=read(p);d['contact_shading_review']='docs/porsche-ao-review.json'
    if name=='porsche-native-review.json':
        d['stage']=report['stage'];d['source_hashes'].update(report['source_hashes']);d['main_view_contact_shading']={'review':'docs/porsche-ao-review.json','linear_depth_ray_samples':fixtures['linear_depth_ray_comparisons'],'geometry_cases':fixtures['geometry_cases'],'glazing_invariance':True,'steering_response':True}
        d['limitations']=[s.replace('local ambient occlusion,','offscreen/reflection AO,') for s in d['limitations']]
    if 'remaining'in d:d['remaining']=[s.replace('Local ambient occlusion and','Offscreen/reflection AO and').replace('local ambient occlusion','offscreen/reflection AO') for s in d['remaining']]
    write(p,d)
print(json.dumps({'native_checks':qa['checks'],'linear_depth_ray_samples':fixtures['linear_depth_ray_comparisons'],'glazing_invariance':True,'steering_response':True,'matched_views':3,'complete':False}))
