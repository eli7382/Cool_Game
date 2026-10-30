"""Check actual posed front tire/red-rim surfaces against front mechanisms."""
import json,hashlib,argparse,sys,collections
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parents[1];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ap=argparse.ArgumentParser();ap.add_argument('--fixture',default='reports/front-fit-recentered-packaging-fixture.json');ap.add_argument('--report',default='reports/front-fit-recentered-wheel-packaging.json');ap.add_argument('--aabb',action='store_true')
opt=ap.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []);path=root/opt.fixture;f=json.loads(path.read_text());assert f['packaging']
predicate_path=root/'scripts/triangle_intersections.py'
exec(compile(predicate_path.read_text(),str(predicate_path),'exec'))
base=[np.array(g['vertices']).reshape(-1,3) for g in f['moving']];indices=[np.array(g['triangles']).reshape(-1,3) for g in f['moving']]
target_ids=[i for i,g in enumerate(f['moving']) if g['packaging_role'] in ['tire','rim']];mechanism_ids=[i for i,g in enumerate(f['moving']) if g['packaging_role']=='mechanism'];assert len(target_ids)>=4 and len(mechanism_ids)>20
cached={};rows=[];all_components=collections.Counter();component_samples={}
def posed(i,matrix,weights):
 key=(tuple(matrix),tuple(weights))
 if i in cached and cached[i][0]==key:return cached[i][1]
 g=f['moving'][i];points=base[i].copy()
 if g['morphTargets']:
  for target,w in zip(g['morphTargets'],weights):
   if abs(w)>1e-12:points+=w*(np.array(target).reshape(-1,3) if g['morphRelative'] else np.array(target).reshape(-1,3)-base[i])
 m=np.array(matrix).reshape(4,4,order='F');points=(np.c_[points,np.ones(len(points))]@m.T)[:,:3]
 triangles=points[indices[i]];tree=BVHTree.FromPolygons([Vector(p) for p in points],indices[i].tolist(),all_triangles=True,epsilon=1e-6)
 lower,upper=triangles.min(axis=1),triangles.max(axis=1)
 value=(triangles,tree,points.min(axis=0),points.max(axis=0),(lower,upper,np.argsort(lower[:,0])));cached[i]=(key,value);return value
def plane_candidates(a,b):
 na=np.cross(a[:,1]-a[:,0],a[:,2]-a[:,0]);nb=np.cross(b[:,1]-b[:,0],b[:,2]-b[:,0]);la=np.linalg.norm(na,axis=1);lb=np.linalg.norm(nb,axis=1);valid=(la>=1e-12)&(lb>=1e-12)
 na/=np.maximum(la,1e-30)[:,None];nb/=np.maximum(lb,1e-30)[:,None]
 da=((b-a[:,0,None,:])*na[:,None,:]).sum(axis=2);db=((a-b[:,0,None,:])*nb[:,None,:]).sum(axis=2)
 coplanar=np.maximum(abs(da).max(axis=1),abs(db).max(axis=1))<=1e-10
 straddle=(da.min(axis=1)<-plane_eps)&(da.max(axis=1)>plane_eps)&(db.min(axis=1)<-plane_eps)&(db.max(axis=1)>plane_eps)
 return valid&(coplanar|straddle)
# The vectorized rejection must retain every positive predicate control.
control_a=np.repeat(np.array([[(0,0,0),(1,0,0),(0,1,0)]],dtype=float),len(controls),axis=0)
assert all(keep or expected is None for keep,(_,expected) in zip(plane_candidates(control_a,np.array([t for t,_ in controls])),controls))
def candidates(ta,ma,ab,bb):
 # No float triangle-overlap predicate between broad and narrow phases.
 al,au,order=ab;bl,bu,_=bb;mins=al[order,0]
 active=np.flatnonzero(np.all(bu>=al.min(axis=0)-1e-6,axis=1)&np.all(bl<=au.max(axis=0)+1e-6,axis=1))
 for start in range(0,len(active),64):
  batch=active[start:start+64];upto=np.searchsorted(mins,bu[batch,0].max()+1e-6,side='right');ids=order[:upto]
  mask=np.all(au[ids][None,:,:]>=bl[batch][:,None,:]-1e-6,axis=2)&np.all(al[ids][None,:,:]<=bu[batch][:,None,:]+1e-6,axis=2)
  bi,ai=np.nonzero(mask);aa=ids[ai];bs=batch[bi]
  if not len(aa):continue
  keep=plane_candidates(ta[aa],ma[bs])
  for a,b in zip(aa[keep],bs[keep]):yield int(a),int(b)
for pose in f['poses']:
 geometry=[posed(i,m,w) for i,(m,w) in enumerate(zip(pose['matrices'],pose['weights']))];counts=collections.Counter();kinds=collections.Counter();samples=[]
 for ti in target_ids:
  ta,tt,tmin,tmax,tab=geometry[ti]
  for mi in mechanism_ids:
   ma,mt,mmin,mmax,mab=geometry[mi]
   if np.any(tmin>mmax+1e-6) or np.any(mmin>tmax+1e-6):continue
   for a,b in (candidates(ta,ma,tab,mab) if opt.aabb else tt.overlap(mt)):
    kind=overlap_kind(ta[a],ma[b])
    if not kind:continue
    key=f['moving'][ti]['name']+' / '+f['moving'][mi]['name'];counts[key]+=1;kinds[kind]+=1;all_components[key]+=1
    saved=component_samples.setdefault(key,[])
    if len(saved)<6 and (not saved or saved[-1]['pose_index']!=len(rows)):
     g=f['moving'][mi];local=base[mi][indices[mi][b]];rest=np.array(g['matrix']).reshape(4,4,order='F');rest_points=(np.c_[local,np.ones(3)]@rest.T)[:,:3]
     saved.append({'pose_index':len(rows),**{k:pose[k] for k in ['steering','left','right','phase']},'kind':kind,'wheel_triangle':a,'mechanism_triangle':b,'wheel_points_m':ta[a].tolist(),'mechanism_points_m':ma[b].tolist(),'mechanism_rest_basis_points_m':rest_points.tolist()})
    if len(samples)<8:samples.append({'wheel_mesh':f['moving'][ti]['name'],'wheel_role':f['moving'][ti]['packaging_role'],'mechanism_mesh':f['moving'][mi]['name'],'kind':kind,'wheel_triangle':a,'mechanism_triangle':b,'wheel_points_m':ta[a].tolist(),'mechanism_points_m':ma[b].tolist()})
 row={k:pose[k] for k in ['steering','left','right','phase']};row.update(intersecting_triangle_pairs=sum(counts.values()),kinds=dict(kinds),pairs_by_component=dict(counts),samples=samples);rows.append(row)
 print('pose',len(rows),'pairs',row['intersecting_triangle_pairs'],flush=True)
report={'export_sha256':f['export_sha256'],'fixture_sha256':sha(path),'predicate_sha256':sha(predicate_path),'checker_sha256':sha(Path(__file__)),'broadphase':'double-precision AABB' if opt.aabb else 'Blender BVH triangle overlap','control_cases_passed':len(controls),'poses':len(rows),
 'wheel_target_meshes':[f['moving'][i]['name'] for i in target_ids],'mechanism_meshes':[f['moving'][i]['name'] for i in mechanism_ids],
 'poses_with_intersections':sum(bool(row['intersecting_triangle_pairs']) for row in rows),'total_intersecting_triangle_pairs':sum(row['intersecting_triangle_pairs'] for row in rows),
 'pairs_by_component':dict(all_components),'component_samples':component_samples,'results':rows,'passed':not any(row['intersecting_triangle_pairs'] for row in rows),
 'scope':'Actual source-adapter poses of front tire carcasses and red forged rim faces/barrels against selected front mechanisms, original front subframe and steering box, including spring morphs. Strict double-precision crossing and coplanar area predicates. Wheel lettering, centre hardware, rotor/pad interfaces, other bodies, containment and continuous motion are excluded. No OEM or native-game proof.','production_changed':False,'complete':False}
(root/opt.report).write_text(json.dumps(report,indent=2)+'\n');print('RESULT',report['poses_with_intersections'],'of',len(rows),flush=True)
