"""Double-precision triangle predicates and analytic controls from the source clearance audit."""
import numpy as np
plane_eps=1e-7
area_eps=1e-10
def coplanar_area(a,b,n):
 drop=int(np.argmax(abs(n)));a=np.delete(a,drop,axis=1);b=np.delete(b,drop,axis=1)
 cross=lambda u,v:float(u[0]*v[1]-u[1]*v[0])
 if cross(b[1]-b[0],b[2]-b[0])<0:b=b[::-1]
 polygon=[p.copy() for p in a]
 for i in range(3):
  first,second=b[i],b[(i+1)%3];edge=second-first;out=[]
  if not polygon:break
  for p,q in zip(polygon,polygon[1:]+polygon[:1]):
   dp=cross(edge,p-first);dq=cross(edge,q-first);inside_p=dp>=0;inside_q=dq>=0
   if inside_p:out.append(p)
   if inside_p!=inside_q and abs(dp-dq)>1e-20:out.append(p+(q-p)*dp/(dp-dq))
  polygon=out
 return abs(sum(cross(p,q) for p,q in zip(polygon,polygon[1:]+polygon[:1])))*.5 if len(polygon)>=3 else 0.

def edge_hit(a,b,t):
 d=b-a;e=t[1]-t[0];g=t[2]-t[0];h=np.cross(d,g);det=float(e@h)
 if abs(det)<1e-15:return False
 inv=1/det;s=a-t[0];u=float(s@h)*inv;q=np.cross(s,e);v=float(d@q)*inv;k=float(g@q)*inv
 return -1e-10<=u<=1+1e-10 and v>=-1e-10 and u+v<=1+1e-10 and 1e-9<k<1-1e-9

def crossing_segment(a,b):
 na=np.cross(a[1]-a[0],a[2]-a[0]);nb=np.cross(b[1]-b[0],b[2]-b[0]);na/=np.linalg.norm(na);nb/=np.linalg.norm(nb)
 direction=np.cross(na,nb);length=np.linalg.norm(direction)
 if length<1e-12:return None
 direction/=length
 def cut(tri,origin,normal):
  distances=(tri-origin)@normal;points=[]
  for i in range(3):
   j=(i+1)%3
   if abs(distances[i])<1e-12:points.append(tri[i])
   if distances[i]*distances[j]<0:points.append(tri[i]+(tri[j]-tri[i])*distances[i]/(distances[i]-distances[j]))
  return points
 pa=cut(a,b[0],nb);pb=cut(b,a[0],na)
 if len(pa)<2 or len(pb)<2:return None
 aa=np.array(pa)@direction;bb=np.array(pb)@direction;lo=max(aa.min(),bb.min());hi=min(aa.max(),bb.max())
 if hi<lo:return None
 origin=pa[0];t=float(origin@direction);return np.array([origin+direction*(lo-t),origin+direction*(hi-t)])

def overlap_kind(a,b):
 a=np.asarray(a,dtype=np.float64);b=np.asarray(b,dtype=np.float64)
 na=np.cross(a[1]-a[0],a[2]-a[0]);nb=np.cross(b[1]-b[0],b[2]-b[0]);la=np.linalg.norm(na);lb=np.linalg.norm(nb)
 if min(la,lb)<1e-12:return None
 na/=la;nb/=lb;da=(b-a[0])@na;db=(a-b[0])@nb
 # Nearly parallel, separated sheets are not coplanar overlaps. Keep the
 # double-precision coplanarity threshold separate from crossing tolerance.
 if max(abs(da).max(),abs(db).max())<=1e-10:
  return 'coplanar_area' if coplanar_area(a,b,na)>area_eps else None
 if not (da.min()<-plane_eps and da.max()>plane_eps and db.min()<-plane_eps and db.max()>plane_eps):return None
 return 'crossing' if any(edge_hit(t[i],t[(i+1)%3],other) for t,other in [(a,b),(b,a)] for i in range(3)) else None

a=np.array([(0,0,0),(1,0,0),(0,1,0)],dtype=float)
controls=[(np.array([(.2,.2,-.3),(.2,.2,.3),(.8,.2,0)]),'crossing'),
 (a+np.array([2,0,.2]),None),(np.array([(0,0,0),(1,0,0),(.3,0,1)]),None),
 (np.array([(.1,.1,0),(.3,.1,0),(.1,.3,0)]),'coplanar_area'),
 (np.array([(0,0,0),(1,0,0),(0,-1,0)]),None),(a[::-1],'coplanar_area'),(a+np.array([0,0,1e-5]),None),
 (a+np.array([0,0,5e-8]),None),(a+np.array([0,0,1e-9]),None)]
for b,expected in controls:assert overlap_kind(a,b)==expected,(expected,overlap_kind(a,b))
