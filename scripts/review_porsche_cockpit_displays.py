"""Record the rendered cockpit revision and unchanged model asset hashes."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
native=read(root/'docs/porsche-native-qa.json');art=read(root/'docs/porsche-cluster-artwork-qa.json');culling=read(root/'docs/porsche-native-culling-qa.json');front=read(root/'docs/porsche-front-fit-review.json')
assert native['passed'] and native['checks']>=155 and native['failures']==0
assert art['passed'] and art['corner_order']==['FL','FR','RL','RR'] and all(n>50 for n in art['changed_pixels']) and art['pixels_changed_outside_expected_instrument']==[0,0,0,0]
assert art['unpowered_lit_pixels']==0 and art['immediate_power_on']
assert culling['passed'] and culling['image_pairs']==36
for level in front['levels']:assert sha(root/f'assets/porsche/gt3rs_{level["level"]}.glb')==level['native_sha256']
assert sha(root/'dist/Breakwater_Windows_x64.exe')==sha(root/'build-native/ninja/bin/game.exe')
old=subprocess.run(['git','show','990db89:screenshots/porsche-native-live-cockpit.png'],cwd=root,check=True,stdout=subprocess.PIPE).stdout
before=root/'screenshots/porsche-cockpit-before-dials.png';before.write_bytes(old)
captures=[]
for name in ['porsche-native-live-cockpit.png','porsche-display-0-driving.png','porsche-display-1-driving.png','porsche-unpowered-display-0.png','porsche-unpowered-display-1.png','porsche-unpowered-display-2.png']:
 captures.append({'file':'screenshots/'+name,'sha256':sha(root/'screenshots'/name)})
report={'implemented':True,'native_checks':native['checks'],'corner_pixel_test':art,'culling_image_pairs':culling['image_pairs'],'geometry_unchanged_from_commit':'990db89','asset_hashes':{r['level']:r['native_sha256'] for r in front['levels']},'renderer_sha256':sha(root/'src/breakwater/porsche_displays.cpp'),'controller_and_qa_sha256':sha(root/'src/breakwater/porsche_native.cpp'),'executable_sha256':sha(root/'dist/Breakwater_Windows_x64.exe'),'before':{'commit':'990db89','file':str(before.relative_to(root)),'sha256':sha(before)},'after':captures,'reference':'https://newsroom.porsche.com/en/2022/products/porsche-911-gt3-rs-world-premiere-29177.html','visual_review':'Four segmented circular readouts sit on the two modeled side displays; compact current gear and D indication occupy the central lower LCD. Native cockpit and flat GPU targets inspected. Parked main LCDs contain zero lit pixels.','limits':['The tyre telemetry page is authored game artwork inspired by the reference instrument silhouette, not OEM PASM controls','The independent Sport Chrono readout continues while parked; battery simulation is not implemented','No new manufacturer photographs or image pixels are included in the asset','Finite image/state checks do not prove final Forza visual parity or frame rate'],'complete':False}
(root/'docs/porsche-cockpit-displays-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'native_checks':native['checks'],'corner_changed_pixels':art['changed_pixels'],'off_lit_pixels':art['unpowered_lit_pixels'],'geometry_changed':False}))
