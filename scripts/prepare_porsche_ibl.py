"""Bake a static daylight probe: cosine irradiance, GGX radiance and DFG LUT.

Conventions and split-sum equations: https://google.github.io/filament/Filament.html
All data stays linear RGB32F. The atlas retains directional resolution at high
roughness instead of collapsing a lat-long mip chain to a single polar texel.
"""
from pathlib import Path
import hashlib,json,struct,math
import sys
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'assets/porsche/environments'
PI=np.pi
SH_CONSTANTS=np.array([.28209479177387814,.4886025119029199,.4886025119029199,.4886025119029199,1.0925484305920792,1.0925484305920792,.31539156525252005,1.0925484305920792,.5462742152960396])

def directions(width,height):
    phi=((np.arange(width)+.5)/width-.5)*2*PI
    theta=(np.arange(height)+.5)/height*PI
    return np.stack(np.broadcast_arrays(np.sin(theta)[:,None]*np.cos(phi),np.cos(theta)[:,None]+np.zeros((1,width)),np.sin(theta)[:,None]*np.sin(phi)),axis=-1)

def solid_angles(width,height):
    edges=np.arange(height+1)/height*PI
    return np.broadcast_to((np.cos(edges[:-1])-np.cos(edges[1:]))[:,None]*2*PI/width,(height,width))

def polynomials(d):
    x,y,z=np.moveaxis(d,-1,0)
    return np.stack([np.ones_like(x),y,z,x,x*y,y*z,3*z*z-1,z*x,x*x-y*y],axis=-1)

def diffuse_sh(image):
    height,width=image.shape[:2]
    basis=polynomials(directions(width,height))*SH_CONSTANTS
    coefficients=np.einsum('hwc,hwi,hw->ic',image,basis,solid_angles(width,height))
    # Cosine convolution divided by pi, then pre-scale for shader polynomials.
    return coefficients*np.array([1,2/3,2/3,2/3,1/4,1/4,1/4,1/4,1/4])[:,None]*SH_CONSTANTS[:,None]

def hammersley(count):
    indices=np.arange(count,dtype=np.uint32)
    bits=indices.copy();reverse=np.zeros(count,dtype=np.uint32)
    for _ in range(32):reverse=(reverse<<1)|(bits&1);bits>>=1
    return np.stack([indices.astype(float)/count,reverse.astype(float)/2**32],axis=-1)

def half_vectors(roughness,count):
    xi=hammersley(count);alpha=roughness**2
    cosine=np.sqrt((1-xi[:,1])/(1+(alpha*alpha-1)*xi[:,1]))
    sine=np.sqrt(np.maximum(0,1-cosine*cosine));phi=2*PI*xi[:,0]
    return np.stack([sine*np.cos(phi),sine*np.sin(phi),cosine],axis=-1)

def panorama_mips(image):
    result=[image.astype(float)]
    while result[-1].shape[0]>1 or result[-1].shape[1]>1:
        src=result[-1];height,width=src.shape[:2]
        if height>1:
            area=solid_angles(width,height)[:,0]
            src=(src[0::2]*area[0::2,None,None]+src[1::2]*area[1::2,None,None])/(area[0::2]+area[1::2])[:,None,None]
        if width>1:src=(src[:,0::2]+src[:,1::2])*.5
        result.append(src)
    return result

def bilinear(image,uv):
    height,width=image.shape[:2];x=uv[...,0]*width-.5;y=np.clip(uv[...,1]*height-.5,0,height-1)
    ix=np.floor(x).astype(int);iy=np.floor(y).astype(int);fx=(x-ix)[...,None];fy=(y-iy)[...,None]
    a=image[iy,ix%width]*(1-fx)+image[iy,(ix+1)%width]*fx
    b=image[np.minimum(iy+1,height-1),ix%width]*(1-fx)+image[np.minimum(iy+1,height-1),(ix+1)%width]*fx
    return a*(1-fy)+b*fy

def sample_mips(mips,light,lod):
    uv=np.stack([np.arctan2(light[...,2],light[...,0])/(2*PI)+.5,np.arccos(np.clip(light[...,1],-1,1))/PI],axis=-1)
    lod=np.clip(lod,0,len(mips)-1);lo=np.floor(lod).astype(int);fraction=(lod-lo)[...,None]
    low=np.empty(light.shape);high=np.empty(light.shape)
    for level,image in enumerate(mips):
        select=lo==level
        if np.any(select):low[select]=bilinear(image,uv[select])
        select=np.minimum(lo+1,len(mips)-1)==level
        if np.any(select):high[select]=bilinear(image,uv[select])
    return low*(1-fraction)+high*fraction

def prefilter(mips,width,height,roughness,count=1024):
    normal=directions(width,height).reshape(-1,3)
    if roughness==0:
        return bilinear(mips[0],np.stack([np.arctan2(normal[:,2],normal[:,0])/(2*PI)+.5,np.arccos(normal[:,1])/PI],axis=-1)).reshape(height,width,3)
    half=half_vectors(roughness,count);hz=half[:,2];nl=np.maximum(2*hz*hz-1,0)
    alpha2=roughness**4;pdf=alpha2/(4*PI*(hz*hz*(alpha2-1)+1)**2)
    output=np.zeros_like(normal)
    source_height,source_width=mips[0].shape[:2]
    for start in range(0,len(normal),256):
        n=normal[start:start+256];up=np.broadcast_to([0.,0.,1.],n.shape).copy();up[np.abs(n[:,2])>.999]=[1,0,0]
        tangent=np.cross(up,n);tangent/=np.linalg.norm(tangent,axis=1)[:,None];bitangent=np.cross(n,tangent)
        h=tangent[:,None]*half[None,:,0,None]+bitangent[:,None]*half[None,:,1,None]+n[:,None]*hz[None,:,None]
        light=2*hz[None,:,None]*h-n[:,None]
        # Lat-long texels have latitude-dependent solid angle. Importance-
        # filtered input samples reduce bright-source variance without clipping HDR.
        omega=(2*PI/source_width)*(PI/source_height)*np.maximum(np.sqrt(np.maximum(0,1-light[:,:,1]**2)),1e-5)
        lod=.5*np.log2(1/np.maximum(count*pdf[None,:]*omega,1e-30))
        samples=sample_mips(mips,light,lod)
        output[start:start+len(n)]=np.sum(samples*nl[None,:,None],axis=1)/nl.sum()
    return output.reshape(height,width,3)

def quadrature_prefilter(mips,width,height,roughness):
    # For N=V the light-direction PDF is D(sqrt((NoL+1)/2))/4.
    # Direct spherical quadrature resolves the HDR sun deterministically. The
    # 512x256 source mip retains each row's exact solid-angle radiance energy.
    image=mips[1];sh,sw=image.shape[:2];source=directions(sw,sh).reshape(-1,3).astype(np.float32)
    omega=solid_angles(sw,sh).ravel().astype(np.float32);energy=image.reshape(-1,3).astype(np.float32)*omega[:,None]
    normal=directions(width,height).reshape(-1,3).astype(np.float32);output=np.empty_like(normal);a2=np.float32(roughness**4)
    for start in range(0,len(normal),64):
        nl=normal[start:start+64]@source.T
        kernel=np.maximum(nl,0)*a2/(4*PI*(.5*(nl+1)*(a2-1)+1)**2)
        output[start:start+64]=(kernel@energy)/(kernel@omega)[:,None]
    return output.reshape(height,width,3).astype(float)

def dfg_lut(size=128,count=4096):
    result=np.zeros((size,size,3));nv=np.maximum(np.arange(size)/(size-1),1e-4)
    for row in range(size):
        rough=row/(size-1);half=half_vectors(rough,count)
        vh=np.maximum(np.sqrt(1-nv*nv)[:,None]*half[None,:,0]+nv[:,None]*half[None,:,2],0)
        nl=np.maximum(2*vh*half[None,:,2]-nv[:,None],0);a2=rough**4
        visibility=.5/np.maximum(nl*np.sqrt(nv[:,None]**2*(1-a2)+a2)+nv[:,None]*np.sqrt(nl*nl*(1-a2)+a2),1e-30)
        gv=4*visibility*nl*vh/np.maximum(half[None,:,2],1e-20);fc=(1-vh)**5
        result[row,:,0]=np.mean(gv*(1-fc),axis=1);result[row,:,1]=np.mean(gv*fc,axis=1)
    return result

def write_float(path,magic,image):
    height,width=image.shape[:2];path.write_bytes(struct.pack('<4sIII',magic,width,height,1)+image.astype('<f4').tobytes())

def main():
    source=DIRECTORY/'daylight.rgb32f';raw=source.read_bytes();magic,width,height,version=struct.unpack('<4sIII',raw[:16]);assert magic==b'PGHF' and version==1
    image=np.frombuffer(raw,dtype='<f4',offset=16).reshape(height,width,3).astype(float)
    if '--dfg-only' in sys.argv:
        metadata=json.loads((DIRECTORY/'ibl.json').read_text());assert metadata['source_sha256']==hashlib.sha256(raw).hexdigest()
        lut=dfg_lut();write_float(DIRECTORY/'ggx_dfg.bin',b'PGDF',lut);metadata['files']['ggx_dfg.bin']=hashlib.sha256((DIRECTORY/'ggx_dfg.bin').read_bytes()).hexdigest();metadata['generator_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();metadata['dfg']='128x128 endpoint grid, 4096 samples, height-correlated Smith visibility, channels A/B/unused'
        (DIRECTORY/'ibl.json').write_text(json.dumps(metadata,indent=2)+'\n');print('Endpoint DFG table saved',flush=True);return
    mips=panorama_mips(image);layers=[];rectangles=[];offset=0;levels=11;atlas_width=width+2
    reuse='--refine-convolution' in sys.argv
    if reuse:
        old=json.loads((DIRECTORY/'ibl.json').read_text());previous=(DIRECTORY/'daylight_ggx.bin').read_bytes();tag,aw,ah,version=struct.unpack('<4sIII',previous[:16]);assert tag==b'PGGA' and version==1
        old_atlas=np.frombuffer(previous,dtype='<f4',offset=16).reshape(ah,aw,3)
        assert old['source_sha256']==hashlib.sha256(raw).hexdigest() and old['files']['daylight_ggx.bin']==hashlib.sha256(previous).hexdigest()
    for level in range(levels):
        w=max(128,width>>level);h=max(64,height>>level);rough=level/(levels-1)
        print(f'GGX {level+1}/{levels}: {w}x{h}, roughness {rough:.2f}',flush=True)
        if level==0:filtered=image.copy()
        elif level==1 and reuse:
            x,y,ow,oh=old['rectangles'][level];assert ow==w and oh==h;filtered=old_atlas[y+1:y+h+1,x+1:x+w+1].copy()
        elif level==1:filtered=prefilter(mips,w,h,rough)
        else:filtered=quadrature_prefilter(mips,w,h,rough)
        pad=np.pad(filtered,((1,1),(1,1),(0,0)),mode='edge');pad[:,0]=pad[:,-2];pad[:,-1]=pad[:,1]
        layer=np.zeros((h+2,atlas_width,3));layer[:,:w+2]=pad;layers.append(layer);rectangles.append([0,offset,w,h]);offset+=h+2
    atlas=np.concatenate(layers,axis=0);write_float(DIRECTORY/'daylight_ggx.bin',b'PGGA',atlas)
    if not reuse:
        print('Integrating correlated-Smith GGX DFG table',flush=True);lut=dfg_lut();write_float(DIRECTORY/'ggx_dfg.bin',b'PGDF',lut)
    else:assert old['files']['ggx_dfg.bin']==hashlib.sha256((DIRECTORY/'ggx_dfg.bin').read_bytes()).hexdigest()
    header=['#pragma once','#include "raylib.h"','namespace bw::porsche_ibl {',f'inline constexpr int LayerCount={levels};','inline constexpr Vector4 Rectangles[]={']
    header+=['{'+','.join(f'{v}.0f' for v in rect)+'},' for rect in rectangles];header+=['};','}'];(ROOT/'src/breakwater/porsche_ibl_data.hpp').write_text('\n'.join(header)+'\n')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    record={'format':1,'source_sha256':sha(source),'generator_sha256':sha(Path(__file__)),'equations':'https://google.github.io/filament/Filament.html','diffuse':'Cosine-convolved irradiance/pi in final atlas layer: GGX roughness-one N=V kernel equals Lambert','specular':'GGX N=V normalized radiance convolution; 1024 filtered importance samples at roughness .1, direct 512x256 spherical quadrature at roughness >=.2','dfg':'128x128 endpoint grid, 4096 samples, height-correlated Smith visibility, channels A/B/unused','atlas_size':[atlas_width,offset],'rectangles':rectangles,'roughness':[i/(levels-1) for i in range(levels)],'files':{name:sha(DIRECTORY/name) for name in ['daylight_ggx.bin','ggx_dfg.bin']},'limits':['Static distant probe; scene reflection, local occlusion and shadowing are separate','Split-sum N=V approximation and finite directional/sample resolution; no exact path-traced equivalence']}
    (DIRECTORY/'ibl.json').write_text(json.dumps(record,indent=2)+'\n');print('IBL bake saved',flush=True)

if __name__=='__main__':main()
