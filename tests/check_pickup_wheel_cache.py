"""Compare dumped cache triangles with actual raylib 5.5 primitive emission.

Run with Blender Python for BVH surface sampling. Compiles a CPU-only recorder
from the unchanged local raylib source; no renderer or cache builder is reused.
"""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
import numpy as np
from mathutils.bvhtree import BVHTree
from mathutils import Vector
root=Path(__file__).resolve().parents[1];work=root/'.tools/pickup-wheel-cache-geometry'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=root/'.tools/raylib-5.5/src/rmodels.c';text=source.read_text()
def function(name):
 start=text.index('void '+name+'(');opening=text.index('{',start);depth=1;i=opening+1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
cpp=r'''
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#define DrawCube ReferenceCube
#define DrawSphere ReferenceSphere
#define DrawSphereEx ReferenceSphereEx
#define DrawCylinderEx ReferenceCylinderEx
namespace original {
constexpr int RL_TRIANGLES=4;
struct Vertex {Vector3 p;Color color;};
std::vector<Matrix> stack{MatrixIdentity()};std::vector<Vertex> vertices;Color pigment{};
void rlPushMatrix(){stack.push_back(stack.back());}
void rlPopMatrix(){stack.pop_back();}
void rlTranslatef(float x,float y,float z){stack.back()=MatrixMultiply(MatrixTranslate(x,y,z),stack.back());}
void rlScalef(float x,float y,float z){stack.back()=MatrixMultiply(MatrixScale(x,y,z),stack.back());}
void rlRotatef(float angle,float x,float y,float z){stack.back()=MatrixMultiply(MatrixRotate({x,y,z},angle*DEG2RAD),stack.back());}
void rlBegin(int){} void rlEnd(){} void rlNormal3f(float,float,float){}
void rlColor4ub(unsigned char r,unsigned char g,unsigned char b,unsigned char a){pigment={r,g,b,a};}
void rlVertex3f(float x,float y,float z){vertices.push_back({Vector3Transform({x,y,z},stack.back()),pigment});}
void DrawSphereEx(Vector3,float,int,int,Color);
'''
cpp+='\n'.join(function(n) for n in ['DrawCube','DrawSphere','DrawSphereEx','DrawCylinderEx'])
cpp+=r'''
}
int main(int argc,char** argv){using namespace original;static_assert(sizeof(Vertex)==16);
 for(int high=0;high<2;++high)for(int axle=0;axle<2;++axle)for(int side:{-1,1}){
  vertices.clear();const float x=axle==0?-1.43f:1.51f,y=.37f,z=side*.84f,cx=axle==0?-1.3781f:1.5105f;const int blocks=high?26:16;
  for(int row=0;row<3;++row)for(int lug=0;lug<blocks;++lug){
   const float phase=2.0f*PI*lug/blocks+(row==1?PI/blocks:0);
   rlPushMatrix();rlTranslatef(cx,y,z);rlRotatef(phase/DEG2RAD,0,0,1);
   DrawCube({0,.359f,(row-1)*.073f},.080f,.026f,.050f,row==1&&lug%4==0?Color{48,51,49,255}:Color{35,39,38,255});rlPopMatrix();
  }
  DrawCylinderEx({x,y,side*.905f},{x,y,side*.936f},.264f,.264f,32,{104,111,107,255});
  DrawCylinderEx({x,y,side*.936f},{x,y,side*.944f},.115f,.115f,24,{63,71,69,255});
  for(int hole=0;hole<12;++hole){const float angle=2.0f*PI*hole/12;DrawSphere({x+cosf(angle)*.218f,y+sinf(angle)*.218f,side*.947f},.012f,{43,50,49,255});}
  DrawCube({x+.196f,y+.012f,side*.936f},.085f,.156f,.052f,{117,67,51,255});DrawSphere({x,y,side*.951f},.054f,{164,153,128,255});
  const std::string name=std::string(argv[1])+"/reference-"+std::to_string(high)+"-"+std::to_string(axle)+"-"+std::to_string(side)+".bin";
  std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(vertices.data()),vertices.size()*sizeof(Vertex));
 }
}
'''
work.mkdir(parents=True,exist_ok=True);path=work/'original-reference.cpp';path.write_text(cpp)
compiler=root/'.tools/w64devkit/bin/g++.exe';exe=work/'original-reference.exe'
subprocess.run([str(compiler),'-std=c++17','-O2','-I'+str(source.parent),str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(work)],check=True)
cache_type=np.dtype([('p','<f4',3),('n','<f4',3),('c','u1',4)])
reference_type=np.dtype([('p','<f4',3),('c','u1',4)])
rows=[];failures=[]
for high in range(2):
 for axle in range(2):
  for side in [-1,1]:
   name=f'{high}-{axle}-{side}';cached=work/(name+'.bin');reference=work/('reference-'+name+'.bin')
   a=np.fromfile(cached,dtype=cache_type);b=np.fromfile(reference,dtype=reference_type)
   assert len(a)==len(b) and len(a)%3==0,(name,'triangle count')
   assert np.isfinite(a['p']).all() and np.isfinite(a['n']).all()
   assert np.max(np.abs(np.linalg.norm(a['n'],axis=1)-1))<1e-5
   def groups(values):
    points=values['p'].reshape(-1,3,3);colors=values['c'].reshape(-1,3,4)
    assert np.all(colors==colors[:,:1,:])
    return {tuple(c):points[np.all(colors[:,0,:]==c,axis=1)] for c in np.unique(colors[:,0,:],axis=0)}
   ga,gb=groups(a),groups(b);assert ga.keys()==gb.keys()
   maximum=0;count=0
   for color in ga:
    assert len(ga[color])==len(gb[color]),(name,color,'triangle count by color')
    for source_triangles,target_triangles in [(ga[color],gb[color]),(gb[color],ga[color])]:
     v=target_triangles.reshape(-1,3);tree=BVHTree.FromPolygons([Vector(p) for p in v],[(i,i+1,i+2) for i in range(0,len(v),3)],all_triangles=True)
     samples=np.concatenate([source_triangles.reshape(-1,3),source_triangles.mean(axis=1),(source_triangles[:,0]+source_triangles[:,1])/2,(source_triangles[:,1]+source_triangles[:,2])/2,(source_triangles[:,2]+source_triangles[:,0])/2])
     for p in samples:
      hit,normal,index,distance=tree.find_nearest(Vector(p));assert hit is not None
      maximum=max(maximum,distance);count+=1
   row={'name':name,'triangles':len(a)//3,'surface_samples_both_directions':count,'maximum_surface_distance_m':maximum,'cache_sha256':sha(cached),'reference_sha256':sha(reference)};rows.append(row)
   if maximum>5e-6:failures.append(row)
report={'passed':not failures,'rows':rows,'failures':failures,'raylib_source_sha256':sha(source),'cache_builder_sha256':sha(root/'src/breakwater/pickup_wheel_cache.cpp'),'checker_sha256':sha(Path(__file__)),'source_geometry':'Unmodified raylib DrawCube, DrawSphere, DrawSphereEx and DrawCylinderEx functions compiled with CPU matrix/vertex recording stubs','normal_change':'Cached spheres/cylinders use explicit surface normals; original immediate functions inherit prior normals. Pixel equivalence is not claimed.','limits':['Finite vertex, edge midpoint and triangle centroid distance samples in both directions by pigment; not an algebraic continuous-surface proof','Local rigid wheel geometry only; native motion, visibility, shading and performance need separate review'],'complete':False}
(root/'docs/pickup-wheel-cache-geometry-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':not failures,'maximum_surface_distance_m':max(r['maximum_surface_distance_m'] for r in rows),'samples':sum(r['surface_samples_both_directions'] for r in rows)}),flush=True)
assert not failures
