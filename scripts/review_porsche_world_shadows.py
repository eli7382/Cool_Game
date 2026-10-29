"""Verify and record native Porsche-to-world shadow evidence after native QA."""
from pathlib import Path
import csv,datetime,hashlib,json,re,subprocess,sys
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
qa=read(ROOT/'docs/porsche-native-qa.json');probes=read(ROOT/'docs/porsche-world-shadow-probes.json');car=read(ROOT/'docs/porsche-world-car-shadow-qa.json')
assert qa['passed'] and qa['checks']>=112 and qa['failures']==0
assert probes['passed'] and probes['analytic_ray_box_probes']==114 and probes['sloped_plane_pixels']==320
assert probes['partial_overlap_cases']>0 and probes['maximum_duplicate_shadow_difference']<.01
assert car['passed'] and car['world_gpu_points']==126 and car['road_mesh_points']>0 and car['terrain_or_floor_mesh_points']>0 and car['partial_filter_edge_points']>0
rows=list(csv.DictReader((ROOT/'docs/porsche-world-car-shadow-samples.csv').open()))
assert len(rows)==car['world_gpu_points'] and all(float(r['error'])<.02 for r in rows)
assert abs(max(float(r['error']) for r in rows)-car['maximum_visibility_error'])<1e-6
assert {int(r['pose']) for r in rows}=={0,1,2}
log=(ROOT/'.tools/porsche-qa-session.log').read_text(encoding='utf-8',errors='replace');log=log[log.rfind('porsche-qa-check,name=dedicated-native-shader-and-linear-environment-ready'):]
checks=[{'name':a,'passed':b=='1'} for a,b in re.findall(r'porsche-qa-check,name=([^,\r\n]+),pass=(\d)',log)]
assert len(checks)==qa['checks'] and all(c['passed'] for c in checks)
thermal=(ROOT/'.tools/hidden-qa-session.log').read_text(encoding='utf-8',errors='replace');thermal=[s for s in thermal.splitlines() if 'qa-car-tire-thermal-result,' in s][-1];assert 'pass=1,load-transfer-pass=1' in thermal
exe=ROOT/'build-native/ninja/bin/game.exe';assert exe.stat().st_mtime<=(ROOT/'docs/porsche-native-qa.json').stat().st_mtime
images=[];differences=[]
for view in ['closed','open','raised']:
    pair=[]
    for state in ['off','on']:
        p=f'screenshots/porsche-world-shadow-{view}-{state}.png';pair.append(np.array(Image.open(ROOT/p).convert('RGB')).astype(int));images.append({'path':p,'sha256':sha(ROOT/p),'view':view,'world_receivers_enabled':state=='on','self_shadows_enabled':True,'placeholder_blob_suppressed':True})
    delta=pair[1]-pair[0];changed=int(np.any(abs(delta)>2,axis=2).sum());bright=int(np.any(delta>2,axis=2).sum());assert changed>100 and bright==0
    differences.append({'view':view,'changed_pixels_over_2_levels':changed,'brightened_pixels_over_2_levels':bright,'maximum_darken':int(-delta.min()),'maximum_brighten':int(delta.max())})
stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
sources=['src/breakwater/world.cpp','src/breakwater/porsche_native.cpp','src/breakwater/porsche_native.hpp','src/breakwater/porsche_shadow.cpp','src/breakwater/porsche_shadow.hpp']
report={'reviewed_at':stamp,'stage':'Porsche directional shadows on native world receivers','baseline_commit':'38a9f9535e37eb62000f2c1b2982b1b21210e0c2','executable_sha256':sha(exe),'source_hashes':{p:sha(ROOT/p) for p in sources},'reviewer_sha256':sha(Path(__file__)),
    'native_qa':qa,'receiver_gpu_fixtures':probes,'real_car_triangle_reference':car,'triangle_reference_samples':{'path':'docs/porsche-world-car-shadow-samples.csv','sha256':sha(ROOT/'docs/porsche-world-car-shadow-samples.csv'),'ground_points':126,'triangle_rays_at_filter_texel_centres':1134},
    'new_checks':[c for c in checks if c['name'] in ['world-sun-shadow-receivers-match-analytic-occlusion-and-filtering','actual-road-and-terrain-car-shadows-match-posed-triangle-rays','directional-shadow-replaces-only-the-Porsche-placeholder-blob','export-matched-native-world-shadow-view']],
    'images':images,'matched_image_differences':differences,'render_policy':{'maps':'Reuse posed, car-relative R32F caster maps; no additional shadow-map draw for world receivers','samplers':[12,13],'overlap':'Minimum visibility, preserving partial PCF weights','lighting':'Direct world sun diffuse/specular/clearcoat, wet glint and foliage sun terms; environment, emission and local lamps retain prior contributions','caster_depth_range':'Occupied texels extrude along the light ray behind the far plane; clear texels and receivers before the near plane stay lit','placeholder':'Suppress Porsche oval when its directional map and shader bindings are available; retain existing fallback and other vehicle behavior','state':'Bind before world/mirror drawing, flush and clear after each pass; invalidate cached shader locations at shutdown'},
    'visual_observations':['Car-shaped shadows follow the runway and adjacent ground rather than a flattened oval mesh','Opened doors extend the ground footprint','The deliberately raised fixture produces a displaced shadow on the surface below','The white/red finish and corrected steering shaft remain intact'],
    'legacy_regression':{'passed':True,'evidence':thermal},'remaining':['World geometry casting onto the Porsche','Local ambient occlusion and glass/material/optical refinements','Full-game frame time, memory/loading policy and LOD transition review','Porsche-specific handling, full mechanical clearance and requested final fidelity'],'complete':False,'final_fidelity_verified':False}
write(ROOT/'docs/porsche-world-shadow-review.json',report)
# The self-shadow suite reruns in this same executable. Refresh its shared
# screenshot/executable receipts, then add this receiver review to the overview.
subprocess.run([sys.executable,str(ROOT/'scripts/review_porsche_shadows.py')],cwd=ROOT,check=True,capture_output=True)
p=ROOT/'docs/porsche-native-review.json';d=read(p);d['stage']=report['stage'];d['native_world_shadows']={'review':'docs/porsche-world-shadow-review.json','analytic_gpu_probes':114,'sloped_plane_pixels':320,'real_ground_points':126,'triangle_ray_visibility_maximum_error':car['maximum_visibility_error']};write(p,d)
p=ROOT/'docs/porsche-shadow-review.json';d=read(p);d['world_receiver_review']='docs/porsche-world-shadow-review.json';write(p,d)
print(json.dumps({'native_checks':qa['checks'],'world_gpu_probes':114,'ground_points':126,'triangle_ray_visibility_maximum_error':car['maximum_visibility_error'],'matched_views':3,'complete':False}))
