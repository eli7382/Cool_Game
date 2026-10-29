"""Compare rear pose/spring data with a pinned pre-change Three fixture."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--baseline-ref',required=True);args=ap.parse_args()
assert not args.baseline_ref.startswith('-')
commit=subprocess.check_output(['git','rev-parse','--verify',args.baseline_ref+'^{commit}'],cwd=root,text=True).strip()
old=subprocess.check_output(['git','show',commit+':tests/porsche_suspension_reference.bin'],cwd=root);new=(root/'tests/porsche_suspension_reference.bin').read_bytes()
class Reader:
 def __init__(self,b):self.b=b;self.at=0
 def raw(self,n):v=self.b[self.at:self.at+n];assert len(v)==n;self.at+=n;return v
 def uints(self,n):return struct.unpack('<'+'I'*n,self.raw(4*n))
 def floats(self,n):return np.frombuffer(self.raw(4*n),dtype='<f4')
 def header(self):
  assert self.raw(4)==b'PSRF';version,levels,poses,names,springs,samples,width=self.uints(7);assert version==1 and springs==4
  self.names=[self.raw(width).split(b'\0')[0].decode() for _ in range(names)];self.levels=levels;self.poses=poses;self.springs=springs;self.samples=samples
 def metadata(self):
  result=[]
  for _ in range(self.springs):
   mesh,corner,n=self.uints(3);result.append((mesh,corner,self.uints(n)))
  return result
 def pose(self):
  return self.floats(6),self.floats(len(self.names)*16).reshape(-1,16),self.floats(8).reshape(4,2),self.floats(self.springs*self.samples*3).reshape(4,-1,3),self.floats(self.springs*self.samples*3).reshape(4,-1,3)
a,b=Reader(old),Reader(new);a.header();b.header();assert a.names==b.names and (a.levels,a.poses,a.samples)==(b.levels,b.poses,b.samples)
rear=[i for i,n in enumerate(a.names) if n.endswith(('_RL','_RR'))];front=[i for i,n in enumerate(a.names) if n.endswith(('_FL','_FR'))]
frames=vertices=poses=frontChanged=0
for level in range(a.levels):
 assert a.metadata()==b.metadata()
 for pose in range(a.poses):
  x,y=a.pose(),b.pose();assert np.array_equal(x[0],y[0]),'Input controls changed'
  assert np.array_equal(x[1][rear],y[1][rear]),('Rear frame changed',level,pose)
  assert np.array_equal(x[2][2:],y[2][2:]),('Rear spring weights changed',level,pose)
  for i in [3,4]:assert np.array_equal(x[i][2:],y[i][2:]),('Rear spring geometry changed',level,pose,i)
  frontChanged+=int(not np.array_equal(x[1][front],y[1][front]));poses+=1;frames+=len(rear);vertices+=2*a.samples
assert a.at==len(old) and b.at==len(new) and frontChanged>0
report={'passed':True,'baseline_commit':commit,'baseline_fixture_sha256':hashlib.sha256(old).hexdigest(),'current_fixture_sha256':hashlib.sha256(new).hexdigest(),'poses':poses,'rear_node_frames_compared':frames,'rear_spring_vertices_and_normals_compared':vertices,'poses_with_front_frame_changes':frontChanged,'rear_frames_spring_weights_positions_and_normals_numerically_exact':True,'limits':['Reference fixture scope: 65 steering/travel/rear-steer poses per detail level','Native equivalence is checked separately by the game; this compares actual Three-generated reference data','Does not establish rear body clearance or OEM suspension behavior']}
(root/'docs/porsche-shared-rack-rear-scope.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
