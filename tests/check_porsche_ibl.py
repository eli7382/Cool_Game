"""Independent hemispherical quadrature and analytic limits for native IBL.

The DFG reference integrates light directions directly with Gauss-Legendre
quadrature; it does not reuse the baker's importance sampler or Hammersley set.
"""
from pathlib import Path
import hashlib,json,struct,importlib.util
import numpy as np

ROOT=Path(__file__).resolve().parents[1];DIRECTORY=ROOT/'assets/porsche/environments'
spec=importlib.util.spec_from_file_location('ibl_baker',ROOT/'scripts/prepare_porsche_ibl.py');baker=importlib.util.module_from_spec(spec);spec.loader.exec_module(baker)

def read_image(name,magic):
    raw=(DIRECTORY/name).read_bytes();tag,w,h,version=struct.unpack('<4sIII',raw[:16]);assert tag==magic and version==1 and len(raw)==16+w*h*12
    image=np.frombuffer(raw,dtype='<f4',offset=16).reshape(h,w,3).astype(float);assert np.isfinite(image).all() and image.min()>=0;return image

def lut_sample(lut,nv,rough):
    h,w=lut.shape[:2];x=np.clip(nv*(w-1),0,w-1);y=np.clip(rough*(h-1),0,h-1);ix=int(x);iy=int(y);fx=x-ix;fy=y-iy
    return (lut[iy,ix]*(1-fx)+lut[iy,min(ix+1,w-1)]*fx)*(1-fy)+(lut[min(iy+1,h-1),ix]*(1-fx)+lut[min(iy+1,h-1),min(ix+1,w-1)]*fx)*fy

def dfg_quadrature(nv,rough):
    nodes,weights=np.polynomial.legendre.leggauss(256);nl=(nodes+1)/2;weights=weights/2
    phi=(np.arange(512)+.5)*2*np.pi/512;sin=np.sqrt(1-nl*nl)
    light=np.stack(np.broadcast_arrays(sin[:,None]*np.cos(phi),sin[:,None]*np.sin(phi),nl[:,None]+np.zeros((1,512))),axis=-1)
    view=np.array([np.sqrt(1-nv*nv),0,nv]);half=light+view;half/=np.linalg.norm(half,axis=-1)[...,None]
    nh=half[:,:,2];vh=np.clip(half@view,0,1);a2=rough**4
    distribution=a2/(np.pi*(nh*nh*(a2-1)+1)**2)
    visibility=.5/(nl[:,None]*np.sqrt(nv*nv*(1-a2)+a2)+nv*np.sqrt(nl[:,None]**2*(1-a2)+a2))
    integral=distribution*visibility*nl[:,None]*weights[:,None]*2*np.pi/512;fc=(1-vh)**5
    return np.array([np.sum(integral*(1-fc)),np.sum(integral*fc),0])

def main():
    metadata=json.loads((DIRECTORY/'ibl.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    assert metadata['source_sha256']==sha(DIRECTORY/'daylight.rgb32f')
    for name,digest in metadata['files'].items():assert sha(DIRECTORY/name)==digest
    source=read_image('daylight.rgb32f',b'PGHF');atlas=read_image('daylight_ggx.bin',b'PGGA');lut=read_image('ggx_dfg.bin',b'PGDF')
    assert [atlas.shape[1],atlas.shape[0]]==metadata['atlas_size'] and len(metadata['rectangles'])==11
    last=0
    for x,y,w,h in metadata['rectangles']:
        assert y==last and x==0 and w>=128 and h>=64;last=y+h+2;layer=atlas[y:y+h+2,:w+2]
        assert np.array_equal(layer[:,0],layer[:,-2]) and np.array_equal(layer[:,-1],layer[:,1])
        assert np.array_equal(layer[0],layer[1]) and np.array_equal(layer[-1],layer[-2])
    assert last==atlas.shape[0]
    x,y,w,h=metadata['rectangles'][0];assert np.array_equal(atlas[y+1:y+h+1,1:w+1],source)
    # Under a uniform environment, convolution must preserve its RGB value.
    constant=np.broadcast_to([.3,.7,1.4],(32,64,3)).copy();mips=baker.panorama_mips(constant);constant_error=0
    for rough in [.1,.4,.7,1.0]:constant_error=max(constant_error,float(np.abs(baker.prefilter(mips,16,8,rough,256)-constant[0,0]).max()))
    assert constant_error<1e-12
    constant_quadrature_error=max(float(np.abs(baker.quadrature_prefilter(mips,16,8,r)-constant[0,0]).max()) for r in [.2,.5,1])
    assert constant_quadrature_error<3e-6
    # Direct pixel-solid-angle cosine integration is independent of SH bases.
    normals=baker.directions(128,64);flat=source.reshape(-1,3);directions=baker.directions(source.shape[1],source.shape[0]).reshape(-1,3)
    omega=baker.solid_angles(source.shape[1],source.shape[0]).ravel();weighted=flat*omega[:,None]/np.pi
    sh=baker.diffuse_sh(source) # Characterize the rejected low-band candidate.
    x,y,w,h=metadata['rectangles'][-1];samples=[];sh_errors=[];prefilter_errors=[]
    for row,col in [(0,0),(7,13),(15,43),(23,77),(31,0),(31,64),(40,20),(47,93),(55,55),(63,127)]:
        n=normals[row,col];reference=np.maximum(directions@n,0)@weighted
        actual_sh=np.maximum(baker.polynomials(n)@sh,0);actual_filter=atlas[y+1+row,x+1+col]
        sh_errors.append(float(np.linalg.norm(actual_sh-reference)/np.linalg.norm(reference)));prefilter_errors.append(float(np.linalg.norm(actual_filter-reference)/np.linalg.norm(reference)))
        samples.append({'normal':n.tolist(),'reference':reference.tolist(),'sh':actual_sh.tolist(),'roughness_one_prefilter':actual_filter.tolist()})
    assert max(prefilter_errors)<.012,prefilter_errors
    specular_errors=[]
    for level in [2,4,7]:
        x,y,w,h=metadata['rectangles'][level];r=level/10;grid=baker.directions(w,h)
        for row,col in [(h//3,w//5),(h//2,w//2),(h*4//5,w*3//4)]:
            nl=directions@grid[row,col];kernel=np.maximum(nl,0)*r**4/(4*np.pi*(.5*(nl+1)*(r**4-1)+1)**2)
            reference=(kernel@ (flat*omega[:,None]))/(kernel@omega);actual=atlas[y+1+row,x+1+col]
            specular_errors.append(float(np.linalg.norm(actual-reference)/np.linalg.norm(reference)))
    assert max(specular_errors)<.04,specular_errors
    quadrature=[];dfg_error=0
    for rough in [.4,.7,1.0]:
        for nv in [.25,.5,1.0]:
            reference=dfg_quadrature(nv,rough);actual=lut_sample(lut,nv,rough);error=float(np.abs(reference-actual).max());dfg_error=max(dfg_error,error);quadrature.append({'nv':nv,'roughness':rough,'reference':reference.tolist(),'lookup':actual.tolist(),'error':error})
    assert dfg_error<.006,quadrature
    furnace=abs(float(lut_sample(lut,1,1)[:2].sum())-(1-np.log(2)));assert furnace<.004
    sharp=0
    for nv in [.2,.5,.9]:sharp=max(sharp,float(np.abs(lut_sample(lut,nv,0)[:2]-[1-(1-nv)**5,(1-nv)**5]).max()))
    assert sharp<.002
    # Reference shader probes exercise real GPU atlas/DFG/irradiance bindings.
    probes=[];angle=1.35;rotation=np.array([[np.cos(angle),0,-np.sin(angle)],[0,1,0],[np.sin(angle),0,np.cos(angle)]])
    def atlas_value(n,rough):
        d=rotation@np.array(n);uv=np.array([np.arctan2(d[2],d[0])/(2*np.pi)+.5,np.arccos(d[1])/np.pi]);lod=rough*10;lo=int(lod);t=lod-lo
        def layer(i):
            x,y,w,h=metadata['rectangles'][i];return baker.bilinear(atlas[y+1:y+h+1,x+1:x+w+1],uv)
        return layer(lo)*(1-t)+layer(min(lo+1,10))*t
    for n in [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]]:
        probes.append((1,n,0,atlas_value(n,1)/8))
    for n,rough in [([1,0,0],.2),([0,1,0],.6),([0,0,-1],1.0),([-1,0,0],.43)]:
        probes.append((2,n,rough,atlas_value(n,rough)/8))
    for nv,rough in [(.25,.4),(.5,.7),(1,1),(.9,.1)]:probes.append((3,[nv,rough,0],rough,lut_sample(lut,nv,rough)))
    for f0,nv,rough in [(1,.3,.2),(1,.7,1),(.04,.3,.5)]:
        a,b=lut_sample(lut,nv,rough)[:2];value=(f0*a+b)*(1+f0*(1/(a+b)-1));probes.append((4,[f0,nv,0],rough,np.full(3,value)))
    header=['#pragma once','#include "raylib.h"','namespace bw::porsche_ibl_reference {','struct Probe {int mode;Vector3 direction;float roughness;Vector3 expected;};','inline constexpr Probe Probes[]={']
    literal=lambda values:'{'+','.join(f'{float(v):.10f}f' for v in values)+'}'
    for mode,n,rough,value in probes:
        assert np.all(value>=0) and np.all(value<=1)
        header.append('{'+f'{mode},'+literal(n)+f',{rough:.8f}f,'+literal(value)+'},')
    header+=['};','}'];(ROOT/'tests/porsche_ibl_reference.hpp').write_text('\n'.join(header)+'\n')
    report={'passed':True,'checker_sha256':sha(Path(__file__)),'generator_sha256':metadata['generator_sha256'],'files':metadata['files'],'constant_importance_filter_max_error':constant_error,'constant_quadrature_max_error':constant_quadrature_error,'specular_full_resolution_reference_max_relative_error':max(specular_errors),'rejected_nine_sh_candidate':{'maximum_relative_error':max(sh_errors),'runtime_used':False,'reason':'Exceeded 10 percent hemispherical reference error; production uses the convolved irradiance map'},'roughness_one_reference_max_relative_error':max(prefilter_errors),'dfg_quadrature_max_error':dfg_error,'white_furnace_roughness_one_error':furnace,'sharp_fresnel_limit_max_error':sharp,'directional_references':samples,'dfg_quadrature':quadrature,'gpu_probe_reference_count':len(probes),'limits':['Independent numerical and analytic checks cover the bake; native GPU probes and matched vehicle views are separate','Finite directional/split-sum accuracy, not an exact path tracer or local scene-lighting solution']}
    (ROOT/'docs/porsche-ibl-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if not isinstance(v,(list,dict))}))

if __name__=='__main__':main()
