import json,struct,pathlib
keys=('cockpit','pilot','seat','console','instrument','canopy','windscreen','windshield','fuselage','cabin shell','steering rim','door','fender','bumper','hood','rocker','skin','wing','tailplane','vertical tail','propeller','blade')
for p in sorted(pathlib.Path('assets').glob('vehicle_*_cc0.glb')):
 b=p.read_bytes(); n,tag=struct.unpack_from('<I4s',b,12); d=json.loads(b[20:20+n].decode().strip()); off=20+n; off=(off+3)&~3; blen,btag=struct.unpack_from('<I4s',b,off); blob=b[off+8:off+8+blen]
 print('\n'+p.name)
 for i,node in enumerate(d.get('nodes',[])):
  name=node.get('name','')
  if any(k in name.lower() for k in keys):
   mi=node.get('mesh'); info=[]
   if mi is not None:
    for pr in d['meshes'][mi].get('primitives',[]):
     ai=pr.get('attributes',{}).get('POSITION'); acc=d['accessors'][ai] if ai is not None else {}; info.append((acc.get('min'),acc.get('max')))
   print(i,name[:46], 'mesh',mi,'tr',node.get('translation'),'bound',info[:1])
 for i,m in enumerate(d.get('materials',[])):
  pbr=m.get('pbrMetallicRoughness',{}); ti=pbr.get('baseColorTexture',{}).get('index')
  print('mat',i,m.get('name'), 'tex',ti,'color',pbr.get('baseColorFactor'),'alpha',m.get('alphaMode'))
 for i,img in enumerate(d.get('images',[])):
  bv=d['bufferViews'][img['bufferView']]; start=bv['byteOffset']; chunk=blob[start:start+bv['byteLength']]
  dim=struct.unpack_from('>II',chunk,16) if chunk[:8]==b'\x89PNG\r\n\x1a\n' else None
  print('image',i,dim)
