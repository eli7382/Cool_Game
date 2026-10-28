"""Decode the bundled CC0 Radiance panorama into portable linear RGB32F.

Default raylib 5.5 builds disable HDR image decoding. The native runtime loads
this explicit float image instead, without changing the pinned library build.
"""
from pathlib import Path
import hashlib,json,struct,io
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
directory=ROOT/'assets/porsche/environments';path=directory/'daylight.hdr';data=path.read_bytes();stream=io.BytesIO(data)
assert stream.readline().startswith(b'#?')
while stream.readline().strip():pass
axis,h,axis2,w=stream.readline().split();assert axis==b'-Y' and axis2==b'+X';height,width=int(h),int(w)
rgbe=np.zeros((height,width,4),dtype=np.uint8)
for y in range(height):
    header=stream.read(4);assert header==bytes([2,2,width>>8,width&255]),'Expected scanline RLE'
    for c in range(4):
        x=0
        while x<width:
            n=stream.read(1)[0]
            if n>128:
                count=n-128;value=stream.read(1)[0];rgbe[y,x:x+count,c]=value
            else:
                count=n;assert count>0;rgbe[y,x:x+count,c]=np.frombuffer(stream.read(count),dtype=np.uint8)
            x+=count;assert x<=width
# Match the project's Three HDRLoader float conversion (RGBE exponent /255).
linear=(rgbe[:,:,:3].astype(np.float64)*np.exp2(rgbe[:,:,3].astype(np.float64)-128)[:,:,None]/255).astype('<f4')
output=directory/'daylight.rgb32f';encoded=struct.pack('<4sIII',b'PGHF',width,height,1)+linear.tobytes();output.write_bytes(encoded)
sha=lambda b:hashlib.sha256(b).hexdigest()
record={'source':'Kloofendal 48d Partly Cloudy','author':'Greg Zaal','url':'https://polyhaven.com/a/kloofendal_48d_partly_cloudy','license':'CC0','source_sha256':sha(data),'linear_sha256':sha(encoded),'width':width,'height':height,'format':'PGHF header (16 bytes), little-endian RGB float32, top scanline first','generator_sha256':sha(Path(__file__).read_bytes()),'limits':['Mipmap filtering of the panorama approximates native rough reflections; not preconvolved GGX IBL']}
(directory/'source.json').write_text(json.dumps(record,indent=2),encoding='utf-8');print(json.dumps(record))
