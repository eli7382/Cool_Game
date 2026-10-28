"""Preserve the Porsche glTF while splitting primitives for raylib's 16-bit indices.

Images become shared external files. Original vertex attributes, targets,
hierarchy, material definitions and triangle winding are retained. numpy is an
offline build dependency only; generated game assets require no Python runtime.
"""
from pathlib import Path
import argparse,copy,hashlib,json,struct,math
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
DTYPES={5120:np.int8,5121:np.uint8,5122:np.dtype('<i2'),5123:np.dtype('<u2'),5125:np.dtype('<u4'),5126:np.dtype('<f4')}
SIZES={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def digest(data):return hashlib.sha256(data).hexdigest()
def read_glb(path):
    b=path.read_bytes();size=struct.unpack_from('<I',b,12)[0];g=json.loads(b[20:20+size]);return b,g,memoryview(b)[28+size:]
def accessor(g,blob,index):
    a=g['accessors'][index];assert not a.get('sparse'),'Sparse source accessor requires explicit handling'
    v=g['bufferViews'][a['bufferView']];dt=DTYPES[a['componentType']];n=SIZES[a['type']];item=np.dtype(dt).itemsize
    offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',n*item)
    return np.ndarray((a['count'],n),dtype=dt,buffer=blob,offset=offset,strides=(stride,item)).copy()
def matrix(node):
    if 'matrix' in node:return np.array(node['matrix'],dtype=float).reshape(4,4,order='F')
    x,y,z,w=node.get('rotation',[0,0,0,1]);sx,sy,sz=node.get('scale',[1,1,1]);m=np.eye(4)
    m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])@np.diag([sx,sy,sz]);m[:3,3]=node.get('translation',[0,0,0]);return m
def world_matrices(g):
    parents={c:i for i,n in enumerate(g['nodes']) for c in n.get('children',[])};cache={}
    def world(i):
        if i not in cache:cache[i]=(world(parents[i]) if i in parents else np.eye(4))@matrix(g['nodes'][i])
        return cache[i]
    return parents,[world(i) for i in range(len(g['nodes']))]
def write_glb(path,g,blob):
    blob+=b'\0'*((-len(blob))%4);g['buffers']=[{'byteLength':len(blob)}];j=json.dumps(g,separators=(',',':'),ensure_ascii=False).encode('utf-8');j+=b' '*((-len(j))%4)
    b=struct.pack('<4sII',b'glTF',2,28+len(j)+len(blob))+struct.pack('<I4s',len(j),b'JSON')+j+struct.pack('<I4s',len(blob),b'BIN\0')+blob;path.write_bytes(b);return b
def convert(path,out):
    original,g,source=read_glb(path);result=copy.deepcopy(g);result['bufferViews']=[];result['accessors']=[];result['meshes']=[];data=bytearray();triangles=0;split=0;max_vertices=0;cached={}
    def append(values,template,target=None):
        nonlocal data
        data+=b'\0'*((-len(data))%4);view={'buffer':0,'byteOffset':len(data),'byteLength':values.nbytes}
        if target:view['target']=target
        vi=len(result['bufferViews']);result['bufferViews'].append(view);data+=values.tobytes()
        a={k:copy.deepcopy(v) for k,v in template.items() if k not in ['bufferView','byteOffset','sparse','min','max','count']};a.update(bufferView=vi,count=len(values))
        if template.get('min') is not None:a['min']=values.min(axis=0).tolist();a['max']=values.max(axis=0).tolist()
        ai=len(result['accessors']);result['accessors'].append(a);return ai
    def attr(index,selection):
        key=(index,selection.tobytes());
        if key not in cached:cached[key]=append(accessor(g,source,index)[selection],g['accessors'][index],34962)
        return cached[key]
    for mi,mesh in enumerate(g['meshes']):
        m={k:copy.deepcopy(v) for k,v in mesh.items() if k!='primitives'};m['primitives']=[]
        for primitive in mesh['primitives']:
            assert primitive.get('mode',4)==4
            count=g['accessors'][primitive['attributes']['POSITION']]['count'];indices=accessor(g,source,primitive['indices']).ravel() if 'indices' in primitive else np.arange(count,dtype=np.uint32)
            assert len(indices)%3==0;triangles+=len(indices)//3
            chunks=[];mapping={};selection=[];local=[]
            def flush():
                if local:chunks.append((np.array(selection,dtype=np.uint32),np.array(local,dtype='<u2').reshape(-1,1)))
            for tri in indices.reshape(-1,3):
                needed=sum(int(i) not in mapping for i in set(tri.tolist()))
                if len(mapping)+needed>60000:flush();mapping={};selection=[];local=[]
                for value in tri:
                    i=int(value)
                    if i not in mapping:mapping[i]=len(selection);selection.append(i)
                    local.append(mapping[i])
            flush();split+=len(chunks)-1
            for selected,local_indices in chunks:
                max_vertices=max(max_vertices,len(selected));p={k:copy.deepcopy(v) for k,v in primitive.items() if k not in ['attributes','indices','targets']}
                p['attributes']={k:attr(v,selected) for k,v in primitive['attributes'].items()};p['indices']=append(local_indices,{'componentType':5123,'type':'SCALAR'},34963)
                if primitive.get('targets'):p['targets']=[{k:attr(v,selected) for k,v in t.items()} for t in primitive['targets']]
                m['primitives'].append(p)
        result['meshes'].append(m)
    textures=out/'textures';textures.mkdir(exist_ok=True)
    for i,image in enumerate(g.get('images',[])):
        view=g['bufferViews'][image['bufferView']];raw=bytes(source[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]);suffix='.png' if image.get('mimeType')=='image/png' else '.jpg';filename=digest(raw)+suffix
        target=textures/filename
        if not target.exists():target.write_bytes(raw)
        result['images'][i]={k:copy.deepcopy(v) for k,v in image.items() if k not in ['bufferView','mimeType']};result['images'][i]['uri']='textures/'+filename
    # Bufferless animation/skin structures are absent in this authored rig.
    assert not g.get('animations') and not g.get('skins')
    level='hero' if 'DetailCandidate' in path.name or 'Development' in path.name else 'near' if 'LOD1' in path.name else 'far'
    output=out/f'gt3rs_{level}.glb';encoded=write_glb(output,result,data)
    parents,worlds=world_matrices(result);bindings=[];morph_blob=bytearray();morphs=[]
    for ni,node in enumerate(result['nodes']):
        if 'mesh' not in node:continue
        for primitive in result['meshes'][node['mesh']]['primitives']:
            binding={'node':ni,'material':primitive.get('material',0),'vertices':result['accessors'][primitive['attributes']['POSITION']]['count']};bindings.append(binding)
            if primitive.get('targets'):
                record={'mesh':len(bindings)-1,'vertex_count':binding['vertices'],'targets':[]}
                for target in primitive['targets']:
                    row={}
                    for semantic in ['POSITION','NORMAL']:
                        if semantic not in target:continue
                        values=accessor(result,memoryview(data),target[semantic]);linear=worlds[ni][:3,:3] if semantic=='POSITION' else np.linalg.inv(worlds[ni][:3,:3]).T
                        values=(values@linear.T).astype('<f4');row[semantic]=len(morph_blob);morph_blob+=values.tobytes()
                    record['targets'].append(row)
                morphs.append(record)
    (out/f'gt3rs_{level}_morphs.bin').write_bytes(morph_blob)
    metadata={'level':level,'source_sha256':digest(original),'runtime_sha256':digest(encoded),'triangles':triangles,'raylib_meshes':len(bindings),'additional_primitives':split,'maximum_vertices_per_primitive':max_vertices,
        'nodes':[{'name':n.get('name',''),'parent':parents.get(i,-1),'local':matrix(n).flatten(order='F').tolist(),'world':worlds[i].flatten(order='F').tolist(),'extras':n.get('extras',{})} for i,n in enumerate(result['nodes'])],
        'bindings':bindings,'materials':result['materials'],'images':result.get('images',[]),'textures':result.get('textures',[]),'morphs':morphs}
    (out/f'gt3rs_{level}_rig.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8');print(json.dumps({k:metadata[k] for k in ['level','source_sha256','runtime_sha256','triangles','raylib_meshes','additional_primitives','maximum_vertices_per_primitive']}),flush=True)
    return metadata
def header(levels):
    text=['#pragma once','#include "raylib.h"','namespace bw::porsche_data {','struct RigNode { const char* name; int parent; Matrix local; Matrix world; };','struct Binding { int node,material,vertices; };',
        'struct MapRef { const char* path; int uv; float strength; };',
        'struct Surface { const char* name; Vector4 base; Vector3 emission; float metallic,roughness,coat,coatRoughness,transmission,ior,alphaCutoff,emissiveStrength; MapRef albedo,normal,orm,coatNormal,emissive; int alphaMode; bool doubleSided; };']
    def floats(values):return ','.join(format(float(v),'.9g')+'f' if '.' in format(float(v),'.9g') or 'e' in format(float(v),'.9g') else format(float(v),'.9g')+'.0f' for v in values)
    def mat(values):return '{'+floats(np.array(values).reshape(4,4,order='F').flatten())+'}'
    for meta in levels:
        label=meta['level'].title();text.append(f'inline constexpr RigNode {label}Nodes[]={{')
        for node in meta['nodes']:text.append('{'+json.dumps(node['name'])+','+str(node['parent'])+','+mat(node['local'])+','+mat(node['world'])+'},')
        text.append('};');text.append(f'inline constexpr Binding {label}Bindings[]={{'+','.join('{'+','.join(str(b[k]) for k in ['node','material','vertices'])+'}' for b in meta['bindings'])+'};')
        def map_ref(info):
            if not info:return '{nullptr,0,1.0f}'
            texture=meta['textures'][info['index']];uri=meta['images'][texture['source']]['uri']
            return '{'+json.dumps('assets/porsche/'+uri)+','+str(info.get('texCoord',0))+','+floats([info.get('scale',1)])+'}'
        text.append(f'inline constexpr Surface {label}Surfaces[]={{')
        for m in meta['materials']:
            p=m.get('pbrMetallicRoughness',{});ex=m.get('extensions',{});coat=ex.get('KHR_materials_clearcoat',{})
            fields=[json.dumps(m.get('name','')),'{'+floats(p.get('baseColorFactor',[1,1,1,1]))+'}','{'+floats(m.get('emissiveFactor',[0,0,0]))+'}',
                floats([p.get('metallicFactor',1),p.get('roughnessFactor',1),coat.get('clearcoatFactor',0),coat.get('clearcoatRoughnessFactor',0),ex.get('KHR_materials_transmission',{}).get('transmissionFactor',0),ex.get('KHR_materials_ior',{}).get('ior',1.5),m.get('alphaCutoff',.5),ex.get('KHR_materials_emissive_strength',{}).get('emissiveStrength',1)]),
                map_ref(p.get('baseColorTexture')),map_ref(m.get('normalTexture')),map_ref(p.get('metallicRoughnessTexture')),map_ref(coat.get('clearcoatNormalTexture')),map_ref(m.get('emissiveTexture')),str({'OPAQUE':0,'MASK':1,'BLEND':2}[m.get('alphaMode','OPAQUE')]),'true' if m.get('doubleSided',False) else 'false']
            text.append('{'+','.join(fields)+'},')
        text.append('};')
    text.append('}')
    (ROOT/'src/breakwater/porsche_asset_data.hpp').write_text('\n'.join(text)+'\n',encoding='utf-8')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);args=parser.parse_args();out=ROOT/'assets/porsche';out.mkdir(parents=True,exist_ok=True)
    levels=[convert(args.source/'exports'/name,out) for name in ['Porsche_911_GT3RS_Development.glb','Porsche_911_GT3RS_LOD1.glb','Porsche_911_GT3RS_LOD2.glb']];header(levels)
    (out/'manifest.json').write_text(json.dumps({'format':1,'source':'Porsche GT3 RS by Black Snow / CC BY 4.0; adapted white/red articulated model','generator_sha256':digest(Path(__file__).read_bytes()),'levels':[{k:v for k,v in m.items() if k not in ['nodes','bindings','materials','images','textures','morphs']} for m in levels],'limits':['Geometry-preserving 16-bit chunking; source materials retain unsupported glTF extensions for the native renderer to interpret','Runtime rendering, rig controls and performance require native game tests']},indent=2),encoding='utf-8')
