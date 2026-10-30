"""Independent triangle-corner comparison of source glTF and native asset chunks.

This checks real exported attributes/targets and image bytes, not converter
implementation details. It does not certify raylib rendering or gameplay.
"""
from pathlib import Path
import argparse,hashlib,json,struct
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
DT={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
SIZE={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
sha=lambda b:hashlib.sha256(b).hexdigest()
def load(path):
    b=path.read_bytes();assert b[:4]==b'glTF' and struct.unpack_from('<I',b,8)[0]==len(b)
    length=struct.unpack_from('<I',b,12)[0];assert b[16:20]==b'JSON'
    g=json.loads(b[20:20+length]);return b,g,b[28+length:]
def values(g,blob,index):
    a=g['accessors'][index];v=g['bufferViews'][a['bufferView']];dtype=np.dtype(DT[a['componentType']]);width=SIZE[a['type']]
    return np.ndarray((a['count'],width),dtype=dtype,buffer=blob,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',dtype.itemsize*width),dtype.itemsize))
def indices(g,blob,p):
    return values(g,blob,p['indices']).ravel() if 'indices' in p else np.arange(g['accessors'][p['attributes']['POSITION']]['count'])
def check(source,runtime):
    original,a,ab=load(source);encoded,b,bb=load(runtime)
    for field in ['asset','nodes','scenes','scene','textures','samplers','extensionsUsed','extensionsRequired']:
        assert a.get(field)==b.get(field),f'{field} changed'
    # Material values/maps are owned by the native shader. The stock GLB loader
    # must see no texture references, avoiding transient duplicate GPU uploads.
    meta=json.loads(runtime.with_name(runtime.stem+'_rig.json').read_text())
    assert meta['deferred_native_materials'] and meta['materials']==a['materials']
    placeholders=[{'name':m.get('name',''),'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':0,'roughnessFactor':1}} for m in a['materials']]
    assert b['materials']==placeholders,'Stock loader must receive untextured placeholders'
    assert len(a['meshes'])==len(b['meshes']);triangles=0;chunks=0;targets=0;maximum_vertices=0
    for am,bm in zip(a['meshes'],b['meshes']):
        assert {k:v for k,v in am.items() if k!='primitives'}=={k:v for k,v in bm.items() if k!='primitives'}
        current=0
        for ap in am['primitives']:
            ai=indices(a,ab,ap);offset=0
            while offset<len(ai):
                bp=bm['primitives'][current];current+=1;bi=indices(b,bb,bp);n=len(bi)
                assert n%3==0 and offset+n<=len(ai)
                assert {k:v for k,v in ap.items() if k not in ['attributes','indices','targets']}=={k:v for k,v in bp.items() if k not in ['attributes','indices','targets']}
                assert ap['attributes'].keys()==bp['attributes'].keys()
                vertex_count=b['accessors'][bp['attributes']['POSITION']]['count'];assert vertex_count<=60000 and int(bi.max())<vertex_count
                assert b['accessors'][bp['indices']]['componentType']==5123
                maximum_vertices=max(maximum_vertices,vertex_count)
                for semantic,access in ap['attributes'].items():
                    source_values=values(a,ab,access)[ai[offset:offset+n]];native_values=values(b,bb,bp['attributes'][semantic])[bi]
                    assert np.array_equal(source_values,native_values),f'{semantic} triangle corners changed'
                    assert a['accessors'][access].get('normalized',False)==b['accessors'][bp['attributes'][semantic]].get('normalized',False)
                assert len(ap.get('targets',[]))==len(bp.get('targets',[]))
                for at,bt in zip(ap.get('targets',[]),bp.get('targets',[])):
                    assert at.keys()==bt.keys()
                    for semantic,access in at.items():assert np.array_equal(values(a,ab,access)[ai[offset:offset+n]],values(b,bb,bt[semantic])[bi]),'Morph corners changed'
                    targets+=1
                offset+=n;triangles+=n//3;chunks+=1
        assert current==len(bm['primitives'])
    assert len(a['images'])==len(b['images'])
    for original_image,native_image in zip(a['images'],b['images']):
        view=a['bufferViews'][original_image['bufferView']];start=view.get('byteOffset',0)
        assert sha(ab[start:start+view['byteLength']])==sha((runtime.parent/native_image['uri']).read_bytes()),'Texture bytes changed'
    return {'source_sha256':sha(original),'runtime_sha256':sha(encoded),'triangles_compared':triangles,'primitives_checked':chunks,'morph_targets_checked':targets,'embedded_images_verified':len(a['images']),'maximum_vertices_per_primitive':maximum_vertices,'attributes_targets_winding_hierarchy_materials_images_retained':True,'full_materials_retained_in_native_metadata':True,'stock_loader_texture_references':0}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args();rows=[]
    for level,suffix in [('hero','Development'),('near','LOD1'),('far','LOD2')]:
        result=check(args.source/'exports'/f'Porsche_911_GT3RS_{suffix}.glb',ROOT/f'assets/porsche/gt3rs_{level}.glb');result['level']=level;rows.append(result)
    report={'passed':True,'checker_sha256':sha(Path(__file__).read_bytes()),'levels':rows,'limits':['Exact source triangle-corner data comparison; native renderer, animation, collision and performance need separate tests']}
    out=ROOT/'docs/porsche-asset-validation.json';out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
