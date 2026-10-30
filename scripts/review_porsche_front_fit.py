"""Bind the front-fit delivery to its actual source, native assets and captures."""
import argparse,hashlib,json,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,default=root.parent/'Porsche_911');opt=ap.parse_args();source=opt.source.resolve()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
qa=read(root/'docs/porsche-native-qa.json');culling=read(root/'docs/porsche-native-culling-qa.json');conversion=read(root/'docs/porsche-asset-validation.json')
# The front-fit baseline has 152 checks; later additions must also pass.
assert qa['passed'] and qa['checks']>=152 and qa['failures']==0
assert culling['passed'] and culling['image_pairs']==36 and culling['mirror_target_differing_pairs']==0
assert conversion['passed']
out=root/'docs/porsche-front-fit';out.mkdir(exist_ok=True);evidence=[];levels=[]
def include(path,name=None):
 dest=out/(name or path.name);shutil.copy2(path,dest);evidence.append({'file':str(dest.relative_to(root)),'sha256':sha(dest)})
for level,num,prefix in [('hero',None,'front-mount-supports'),('near',1,'front-fit-lod1'),('far',2,'front-fit-lod2')]:
 suffix='Development' if num is None else 'LOD'+str(num);asset=source/f'exports/Porsche_911_GT3RS_{suffix}.glb';digest=sha(asset)
 packaging=source/f'reports/{prefix}-packaging-aabb-qa.json';p=read(packaging)
 assert p['passed'] and p['poses']==135 and p['total_intersecting_triangle_pairs']==0 and p['export_sha256']==digest
 assert p['checker_sha256']==sha(source/'scripts/check_front_wheel_packaging.py') and p['predicate_sha256']==sha(source/'scripts/audit_front_well_export_self_overlap.py')
 row=next(r for r in conversion['levels'] if r['level']==level);assert row['source_sha256']==digest and row['runtime_sha256']==sha(root/f'assets/porsche/gt3rs_{level}.glb')
 include(packaging,f'{level}-wheel-packaging.json')
 audit=source/'reports'/f'{prefix}-{"export-audit" if num is None else "audit"}.json';a=read(audit);assert a['sha256']==digest and all(c['pass'] for c in a['checks']);include(audit,f'{level}-export-audit.json')
 motion=source/'reports'/('front-fit-hero-suspension.json' if num is None else f'lod{num}-suspension.json');m=read(motion);assert m['passed'] and m['poses']==65 and m['export_sha256']==digest;include(motion,f'{level}-suspension.json')
 levels.append({'level':level,'source_sha256':digest,'native_sha256':row['runtime_sha256'],'triangles':row['triangles_compared'],'packaging_poses_clear':135,'export_checks':len(a['checks']),'suspension_poses':m['poses']})
body=source/'reports/front-mount-supports-clearance-qa.json';b=read(body);assert b['pose_count']==165 and b['poses_with_intersections']==0 and b['export_sha256']==levels[0]['source_sha256'];include(body,'hero-tire-body.json')
well=source/'reports/front-mount-supports-well-self-qa.json';w=read(well);assert w['passed'] and w['new_source_polygon_pairs']==0;include(well,'well-self-overlap.json')
for name in ['lod-runtime','lod-protected-surfaces']:
 path=source/f'reports/{name}.json';d=read(path);assert d['passed'] and d['hero_sha256']==levels[0]['source_sha256'];include(path)
package=read(source/'reports/front-fit-source-package.json');assert sha(root/'assets/porsche/Porsche_911_GT3RS_Source.blend')==package['package_sha256'];include(source/'reports/front-fit-source-package.json')
bundled=read(root/'.tools/front-fit-bundled-fixture-qa.json');assert bundled['passed'] and bundled['native_export_sha256']==levels[0]['native_sha256'];include(root/'.tools/front-fit-bundled-fixture-qa.json','bundled-native-fixture.json')
include(root/'tests/porsche_front_fit/reports/smoke-qa.json','bundled-checker-smoke.json')
for name in ['porsche-native-qa.json','porsche-native-suspension-qa.json','porsche-native-rack-qa.json','porsche-native-culling-qa.json']:include(root/'docs'/name)
assert sha(root/'dist/Breakwater_Windows_x64.exe')==sha(root/'build-native/ninja/bin/game.exe')
captures=[]
for tag,name in [('front','porsche-native-front.png'),('cockpit','porsche-native-live-cockpit.png'),('rest','porsche-native-suspension-rest.png'),('cross','porsche-native-suspension-cross.png')]:
 dest=root/f'screenshots/porsche-front-fit-{tag}.png';shutil.copy2(root/'screenshots'/name,dest);captures.append({'file':str(dest.relative_to(root)),'sha256':sha(dest)})
report={'front_fit_integrated':True,'levels':levels,'native_checks':qa['checks'],'native_suspension_poses':195,'native_culling_image_pairs':36,'tire_body_poses_clear':165,'well_original_overlap_pairs':w['before_source_polygon_pairs'],'well_after_overlap_pairs':w['after_source_polygon_pairs'],'well_new_overlap_pairs':0,'source_package':package,'executable_sha256':sha(root/'dist/Breakwater_Windows_x64.exe'),'native_controller_sha256':sha(root/'src/breakwater/porsche_native.cpp'),'asset_header_sha256':sha(root/'src/breakwater/porsche_asset_data.hpp'),'captures':captures,'evidence':evidence,'limits':['Finite selected-surface clearance, not whole-vehicle continuous collision or solid containment','Source-fitted game mechanics, not Porsche CAD or OEM vehicle physics','Native visual captures reviewed; exact Forza parity and 60 FPS are not established; earlier frame-time results belong to earlier asset hashes'],'complete':False}
(root/'docs/porsche-front-fit-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'levels':levels,'native_checks':qa['checks'],'source_bytes':package['bytes']}))
