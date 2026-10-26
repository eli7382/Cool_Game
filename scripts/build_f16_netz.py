"""Build Breakwater's original metric-scale F-16A Netz asset in Blender.

All geometry is authored here; no game geometry or textures are copied. Blender
exports the GLB and a small source-index map used by the raylib integration.
"""

import bpy
import bmesh
import json
import math
import os
import random
import tempfile
from mathutils import Vector
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ASSETS = os.path.join(ROOT, "assets")
SHOTS = os.path.join(ROOT, "screenshots")
SRC = os.path.join(ROOT, "src", "breakwater")
os.makedirs(ASSETS, exist_ok=True)
os.makedirs(SHOTS, exist_ok=True)
LOCAL_TMP = os.path.join(ROOT, "tmp", "blender-temp")
os.makedirs(LOCAL_TMP, exist_ok=True)
tempfile.tempdir = LOCAL_TMP

random.seed(1610)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for block in bpy.data.meshes:
    if block.users == 0:
        bpy.data.meshes.remove(block)

def material(name, color, metallic=0.0, roughness=0.5, alpha=1.0, transmission=0.12):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, alpha)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, alpha)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if alpha < 0.99:
        mat.surface_render_method = "BLENDED"
        bsdf.inputs["Alpha"].default_value = alpha
        bsdf.inputs["IOR"].default_value = 1.46
        bsdf.inputs["Transmission Weight"].default_value = transmission
    return mat

MAT = [
    material("Netz | desert sand polyurethane", (0.47, 0.405, 0.292), 0.11, 0.52),
    material("Netz | muted olive drab", (0.245, 0.275, 0.205), 0.08, 0.60),
    material("Netz | dark earth brown", (0.225, 0.175, 0.13), 0.07, 0.62),
    material("Netz | warm grey underside", (0.365, 0.366, 0.337), 0.10, 0.58),
    material("Canopy | warm olive bronze laminated glass", (0.12, 0.14, 0.105), 0.035, 0.13, 0.46, 0.58),
    material("Intake | deep carbon cavity", (0.012, 0.017, 0.019), 0.15, 0.78),
    material("Graphite | tyres and seals", (0.035, 0.042, 0.041), 0.02, 0.86),
    material("Oxidized titanium | exhaust", (0.205, 0.225, 0.22), 0.78, 0.32),
    material("Brushed aircraft alloy", (0.42, 0.455, 0.43), 0.76, 0.29),
    material("Cockpit | charcoal anti-glare", (0.055, 0.067, 0.065), 0.08, 0.72),
    material("Cockpit | fabric and leather", (0.16, 0.19, 0.16), 0.0, 0.9),
    material("Display | phosphor green", (0.19, 0.55, 0.37), 0.1, 0.22, 1.0),
    material("Marking | warm white", (0.83, 0.82, 0.72), 0.04, 0.48),
    material("Marking | deep blue", (0.045, 0.16, 0.42), 0.08, 0.40),
    material("Safety | faded vermilion", (0.67, 0.19, 0.105), 0.08, 0.46),
    material("Thermal | nozzle petal dark", (0.12, 0.145, 0.145), 0.82, 0.31),
    material("Thermal | nozzle petal light", (0.34, 0.365, 0.35), 0.82, 0.29),
    material("Warning stencil | dark charcoal", (0.055, 0.064, 0.061), 0.02, 0.68),
    material("Lamp | amber", (0.92, 0.38, 0.06), 0.05, 0.25),
    material("Lamp | red", (0.72, 0.045, 0.025), 0.04, 0.25),
    material("Radome | graphite composite", (0.31, 0.33, 0.32), 0.035, 0.43),
    material("Stores | pale aerospace enamel", (0.57, 0.59, 0.56), 0.24, 0.34),
    material("Stores | seeker glass and shadow", (0.035, 0.052, 0.052), 0.38, 0.16),
    material("Stores | faded yellow safety band", (0.72, 0.58, 0.25), 0.06, 0.42),
    material("Panel joint | graphite shadow", (0.055, 0.060, 0.055), 0.0, 0.88),
    material("Pilot | sage flight suit", (0.055, 0.074, 0.054), 0.0, 0.94),
    material("Pilot | charcoal helmet shell", (0.028, 0.037, 0.035), 0.03, 0.76),
    material("Pilot | smoked gold visor", (0.008, 0.015, 0.016), 0.06, 0.24),
    material("Pilot | oxygen kit metal", (0.043, 0.055, 0.050), 0.08, 0.81),
    material("Canopy | satin charcoal frame", (0.095, 0.11, 0.105), 0.24, 0.55),
    material("Panel | zinc primer", (0.30, 0.315, 0.29), 0.30, 0.63),
]

def make_camo_texture():
    image_path=os.path.join(ASSETS,"f16_netz_camo_generated.png")
    if not os.path.isfile(image_path):
        raise RuntimeError("ImageGen Netz livery map is missing: "+image_path)
    image=bpy.data.images.load(image_path,check_existing=True)
    image.pack()
    micro_path=os.path.join(ASSETS,"f16_netz_surface_microdetail_v1.png")
    if not os.path.isfile(micro_path):
        raise RuntimeError("ImageGen Netz paint microdetail tile is missing: "+micro_path)
    micro=bpy.data.images.load(micro_path,check_existing=True)
    micro.pack()
    micro.colorspace_settings.name="Non-Color"
    texmat=material("Netz | tri-colour desert camouflage PBR",(.47,.405,.292),.10,.54)
    nodes=texmat.node_tree.nodes;links=texmat.node_tree.links
    node=nodes.new("ShaderNodeTexImage");node.image=image;node.interpolation="Linear";node.extension="REPEAT"
    links.new(node.outputs["Color"],nodes.get("Principled BSDF").inputs["Base Color"])
    micro_node=nodes.new("ShaderNodeTexImage");micro_node.image=micro
    micro_node.interpolation="Linear";micro_node.extension="REPEAT"
    micro.colorspace_settings.name="Non-Color"
    micro_bw=nodes.new("ShaderNodeRGBToBW");links.new(micro_node.outputs["Color"],micro_bw.inputs["Color"])
    bump=nodes.new("ShaderNodeBump");bump.inputs["Strength"].default_value=.075;bump.inputs["Distance"].default_value=.009
    links.new(micro_bw.outputs["Val"],bump.inputs["Height"])
    # Blend a subdued paint micrograin over the atlas without replacing the broad
    # Netz camouflage fields. It also feeds the runtime normal/roughness maps below.
    normal_img=bpy.data.images.new("Netz | baked paint micrograin normal",width=micro.size[0],height=micro.size[1],alpha=True,float_buffer=False)
    rough_img=bpy.data.images.new("Netz | baked paint micrograin roughness",width=micro.size[0],height=micro.size[1],alpha=True,float_buffer=False)
    source=np.asarray(micro.pixels[:],dtype=np.float32).reshape((micro.size[1],micro.size[0],4))
    gray=np.dot(source[:,:,:3],np.array([.2126,.7152,.0722],dtype=np.float32))
    # Downsample once, then remove broad color mottling; the residual carries only
    # small enamel pores/scratches so it cannot create scale-sized fake dents.
    blur=gray.copy()
    for _ in range(8):
        blur=(blur+np.roll(blur,1,0)+np.roll(blur,-1,0)+np.roll(blur,1,1)+np.roll(blur,-1,1))*.2
    height=np.clip(gray-blur,-.18,.18)
    dx=np.gradient(height,axis=1)*8.0
    dy=np.gradient(height,axis=0)*8.0
    nx=-dx;ny=-dy;nz=np.ones_like(nx)
    norm=np.sqrt(nx*nx+ny*ny+nz*nz)
    normal=np.stack([.5+.5*nx/norm,.5+.5*ny/norm,.5+.5*nz/norm],axis=2)
    normal=np.concatenate([normal,np.ones((gray.shape[0],gray.shape[1],1),dtype=np.float32)],axis=2)
    normal_img.pixels.foreach_set(normal.astype(np.float32).ravel())
    normal_path=os.path.join(ASSETS,"f16_netz_normal.png");normal_img.filepath_raw=normal_path;normal_img.file_format="PNG";normal_img.save();normal_img.pack()
    rough_value=np.clip(.54+(gray-.48)*.22,.42,.69)
    rough=np.repeat(rough_value[...,None],3,axis=2)
    rough=np.concatenate([rough,np.ones((gray.shape[0],gray.shape[1],1),dtype=np.float32)],axis=2)
    rough_img.pixels.foreach_set(rough.astype(np.float32).ravel())
    rough_path=os.path.join(ASSETS,"f16_netz_roughness.png");rough_img.filepath_raw=rough_path;rough_img.file_format="PNG";rough_img.save();rough_img.pack()
    bsdf=nodes.get("Principled BSDF")
    normal_img.colorspace_settings.name="Non-Color"
    normal_node=nodes.new("ShaderNodeTexImage");normal_node.image=normal_img;normal_node.interpolation="Linear";normal_node.extension="REPEAT"
    normal_map=nodes.new("ShaderNodeNormalMap");normal_map.inputs["Strength"].default_value=.45
    links.new(normal_node.outputs["Color"],normal_map.inputs["Color"]);links.new(normal_map.outputs["Normal"],bsdf.inputs["Normal"])
    links.new(normal_map.outputs["Normal"],bump.inputs["Normal"]);links.new(bump.outputs["Normal"],bsdf.inputs["Normal"])
    rough_img.colorspace_settings.name="Non-Color"
    rough_node=nodes.new("ShaderNodeTexImage");rough_node.image=rough_img;rough_node.interpolation="Linear";rough_node.extension="REPEAT"
    links.new(rough_node.outputs["Color"],bsdf.inputs["Roughness"])
    return texmat

CAMO_MAT=make_camo_texture()

def camo_index(x, y, z):
    # Large, restrained organic fields; exact placement is an authored livery,
    # not a claim that every production Netz carried this particular pattern.
    n = (0.42 * math.sin(1.32*x + 1.07*y + 0.8*math.sin(0.67*x-1.8*y))
         + 0.34 * math.sin(1.7*y - 0.73*z + 0.65*math.sin(x*1.1))
         + 0.24 * math.sin(1.15*x + 1.05*z))
    if n > 0.23:
        return 1
    if n < -0.30:
        return 2
    return 0

def obj_from_mesh(name, verts, faces, material_ids=None, slots=None, smooth=True, uv_mode=None):
    mesh = bpy.data.meshes.new(name + "Mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(ob)
    for m in (slots or [MAT[0]]):
        mesh.materials.append(m)
    if material_ids:
        for poly, idx in zip(mesh.polygons, material_ids):
            poly.material_index = idx
    if uv_mode:
        uv=mesh.uv_layers.new(name="Aircraft UV0")
        for poly in mesh.polygons:
            coords=[]
            for li in poly.loop_indices:
                x,y,z=mesh.vertices[mesh.loops[li].vertex_index].co
                if uv_mode=="cylinder":
                    ang=math.atan2(z-1.23,y)
                    coords.append(((x+7.12)/7.07,(2*ang/(2*math.pi))%1.0))
                elif uv_mode=="vertical":
                    coords.append(((x+7.5)/5.0,(z-1.2)/3.6))
                else:
                    coords.append(((x+3.25)/8.2,abs(y)/4.725))
            if uv_mode=="cylinder" and coords and max(q[1] for q in coords)-min(q[1] for q in coords)>.5:
                coords=[(u,v+1.0 if v<.5 else v) for u,v in coords]
            if uv_mode=="cylinder":coords=[(u*1.8,v*1.35) for u,v in coords]
            elif uv_mode=="vertical":coords=[(u*1.7,v*1.5) for u,v in coords]
            elif uv_mode=="wing":coords=[(u*1.55,v*1.5) for u,v in coords]
            for li,value in zip(poly.loop_indices,coords):uv.data[li].uv=value
    if smooth:
        for poly in mesh.polygons:
            poly.use_smooth = True
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    return ob

def add_ellipsoid(builder, center, scale, mat_id, rings=8, segments=12):
    start = len(builder[0])
    v, f, mids = builder
    for ri in range(rings + 1):
        lat = -math.pi/2 + math.pi*ri/rings
        for si in range(segments):
            lon = 2*math.pi*si/segments
            v.append((center[0]+scale[0]*math.cos(lat)*math.cos(lon),
                      center[1]+scale[1]*math.cos(lat)*math.sin(lon),
                      center[2]+scale[2]*math.sin(lat)))
    for ri in range(rings):
        for si in range(segments):
            a=start+ri*segments+si; b=start+ri*segments+(si+1)%segments
            c=start+(ri+1)*segments+(si+1)%segments; d=start+(ri+1)*segments+si
            f.append((a,b,c,d)); mids.append(mat_id)

def add_cylinder(builder, a, b, radius, mat_id, segments=12, radius2=None):
    v, f, mids = builder
    a, b = Vector(a), Vector(b)
    axis = (b-a).normalized()
    basis = axis.cross(Vector((0, 1, 0)))
    if basis.length < 1e-4:
        basis = axis.cross(Vector((1, 0, 0)))
    basis.normalize()
    other = axis.cross(basis).normalized()
    r2 = radius if radius2 is None else radius2
    start = len(v)
    for center, rad in ((a,radius),(b,r2)):
        for i in range(segments):
            angle=2*math.pi*i/segments
            p=center+rad*(basis*math.cos(angle)+other*math.sin(angle))
            v.append(tuple(p))
    f.append(tuple(start+i for i in reversed(range(segments)))); mids.append(mat_id)
    f.append(tuple(start+segments+i for i in range(segments))); mids.append(mat_id)
    for i in range(segments):
        f.append((start+i,start+(i+1)%segments,start+segments+(i+1)%segments,start+segments+i)); mids.append(mat_id)

def add_box(builder, center, size, mat_id, yaw=0.0):
    v, f, mids = builder
    cx,cy,cz=center; sx,sy,sz=[q*.5 for q in size]
    points=[(-sx,-sy,-sz),(sx,-sy,-sz),(sx,sy,-sz),(-sx,sy,-sz),
            (-sx,-sy,sz),(sx,-sy,sz),(sx,sy,sz),(-sx,sy,sz)]
    start=len(v); c,s=math.cos(yaw),math.sin(yaw)
    for x,y,z in points:
        v.append((cx+x*c-y*s,cy+x*s+y*c,cz+z))
    for face in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)):
        f.append(tuple(start+i for i in face)); mids.append(mat_id)

def profile_thickness(t, chord):
    t=max(0.001,min(.999,t))
    return 5*.12*chord*(.2969*math.sqrt(t)-.1260*t-.3516*t*t+.2843*t**3-.1015*t**4)

def interp_station(stations, u):
    if u<=stations[0][0]: return stations[0][1:]
    if u>=stations[-1][0]: return stations[-1][1:]
    for a,b in zip(stations,stations[1:]):
        if a[0]<=u<=b[0]:
            q=(u-a[0])/(b[0]-a[0])
            return tuple(a[k]+q*(b[k]-a[k]) for k in range(1,len(a)))

def wing_mesh(name, stations, span_axis="y", side=1, t0=0.0, t1=1.0,
              span_lo=None, span_hi=None, skip_aileron=False, slots=None, nspan=42, nchord=30):
    """Closed NACA 0012 wing panel. Stations are (span,x_le,chord,z_center)."""
    slots=slots or [CAMO_MAT,MAT[3]]
    v=[]; f=[]; mids=[]
    max_span=stations[-1][0]
    lo=stations[0][0] if span_lo is None else span_lo
    hi=max_span if span_hi is None else span_hi
    spans=[lo+(hi-lo)*i/nspan for i in range(nspan+1)]
    ts=[t0+(t1-t0)*i/nchord for i in range(nchord+1)]
    upper=[]; lower=[]
    for span in spans:
        xle,chord,zc=interp_station(stations,span)
        sign=side if span_axis=="y" else 1
        uprow=[]; lowrow=[]
        for t in ts:
            # The nose is local +X, so chord runs from the leading edge rearward.
            x=xle-chord*t
            thick=profile_thickness(t,chord)
            camber=0.012*chord*math.sin(math.pi*t)
            if span_axis=="y":
                uprow.append((x,sign*span,zc+camber+thick))
                lowrow.append((x,sign*span,zc+camber-thick))
            else:
                # Vertical tail airfoils run in X/Z; the thickness axis is lateral.
                uprow.append((x,0.5*sign*thick,span+camber))
                lowrow.append((x,-0.5*sign*thick,span+camber))
        upper.append(uprow);lower.append(lowrow)
    def mat_for(pt, lower_side=False):
        if len(slots)<=1:return 0
        # The vertical fin and rudder wear Netz camouflage on both faces;
        # horizontal lifting surfaces keep the light grey underside finish.
        if lower_side:return 1 if len(slots)>1 and span_axis=="y" else 0
        return 0
    for grid, is_lower in ((upper,False),(lower,True)):
        base=len(v)
        for row in grid:v.extend(row)
        for si in range(nspan):
            midspan=(spans[si]+spans[si+1])*.5
            for ci in range(nchord):
                midt=(ts[ci]+ts[ci+1])*.5
                if skip_aileron and (
                    (2.48<=midspan<=4.37 and .68<=midt<=.965)
                    or (1.52<=midspan<=4.34 and .016<=midt<=.105)
                ):
                    continue
                a=base+si*(nchord+1)+ci;b=a+nchord+1
                face=(a,b,b+1,a+1) if not is_lower else (a,a+1,b+1,b)
                f.append(face); mids.append(mat_for(v[a],is_lower))
    # Leading and trailing edge caps, plus root/tip end caps.
    for si in range(nspan):
        for ci in (0,nchord):
            a=si*(nchord+1)+ci;b=(si+1)*(nchord+1)+ci
            f.append((a,b,(nspan+1)*(nchord+1)+b,(nspan+1)*(nchord+1)+a));mids.append(0)
    stride=nchord+1
    for si in (0,nspan):
        for ci in range(nchord):
            a=si*stride+ci;b=a+1
            off=(nspan+1)*stride
            f.append((a,b,off+b,off+a));mids.append(0)
    return obj_from_mesh(name,v,f,mids,slots,uv_mode="vertical" if span_axis=="z" else "wing")

FUSELAGE_KEYS=[(-7.12,.015,.025,1.16),(-6.72,.12,.13,1.18),(-6.05,.22,.22,1.20),
              (-5.15,.34,.33,1.22),(-4.05,.47,.45,1.24),(-2.80,.59,.55,1.25),
              (-1.40,.67,.62,1.26),(0.15,.70,.66,1.26),(1.48,.67,.63,1.25),
              (2.65,.59,.56,1.24),(3.75,.49,.45,1.23),(4.75,.38,.35,1.22),
              (5.72,.28,.25,1.20),(6.05,.235,.205,1.19),(6.55,.16,.14,1.18),(7.02,.07,.07,1.17)]
INTAKE_KEYS=[(-.12,.49,.63,.54),(.45,.52,.79,.68),(.91,.56,.92,.79)]

def fuselage_surface(x, angle, offset=0.0):
    ry,rz,zc=interp_station(FUSELAGE_KEYS,x)
    return (x,(ry+offset)*math.cos(angle),zc+(rz+offset)*math.sin(angle))

def inside_intake_aperture(x,y,z):
    if x<INTAKE_KEYS[0][0] or x>INTAKE_KEYS[-1][0]:return False
    zc,width,height=interp_station(INTAKE_KEYS,x)
    # Elliptical aperture is cut through the lower pressure shell. The mouth
    # lip and dark compressor duct remain separate meshes behind this opening.
    q=(y/(width*.5))**2+((z-zc)/(height*.5))**2
    return q<1.08 and z<zc+height*.49

def build_fuselage():
    keys=FUSELAGE_KEYS
    stations=[]
    for a,b in zip(keys,keys[1:]):
        for step in range(20):
            t=step/20
            stations.append(tuple(a[k]+(b[k]-a[k])*t for k in range(4)))
    stations.append(keys[-1])
    verts=[];faces=[];mids=[];rings=128
    for x,ry,rz,zc in stations:
        for k in range(rings):
            a=2*math.pi*k/rings
            y=ry*math.cos(a);z=zc+rz*math.sin(a)
            verts.append((x,y,z))
    for i in range(len(stations)-1):
        for k in range(rings):
            a=i*rings+k;b=i*rings+(k+1)%rings
            c=(i+1)*rings+(k+1)%rings;d=(i+1)*rings+k
            x=(verts[a][0]+verts[b][0]+verts[c][0]+verts[d][0])*.25
            y=(verts[a][1]+verts[b][1]+verts[c][1]+verts[d][1])*.25
            z=(verts[a][2]+verts[b][2]+verts[c][2]+verts[d][2])*.25
            if inside_intake_aperture(x,y,z):
                continue
            mi=2 if x>=6.05 else (0 if z>1.25 else 1)
            faces.append((a,b,c,d));mids.append(mi)
    faces.append(tuple(reversed(range(rings))));mids.append(0)
    faces.append(tuple((len(stations)-1)*rings+k for k in range(rings)));mids.append(0)
    return obj_from_mesh("Fuselage | 16-station pressure shell",verts,faces,mids,[CAMO_MAT,MAT[3],MAT[20]],uv_mode="cylinder")

def simple_mesh(name, builder, slots=None):
    slots=slots or [MAT[0]]
    material_ids=[]
    for source_id in builder[2]:
        # Detail builders use MAT indices. Blender polygons need indices local
        # to this object's compact material-slot list; leaving the global ids
        # here made many cockpit and landing-gear faces silently use wrong paint.
        local_id=next((i for i,slot in enumerate(slots)
                       if 0<=source_id<len(MAT) and slot==MAT[source_id]),None)
        if local_id is None and 0<=source_id<len(slots):
            local_id=source_id
        if local_id is None:
            raise RuntimeError("Material id %d is not present in slots for %s"%(source_id,name))
        material_ids.append(local_id)
    return obj_from_mesh(name,builder[0],builder[1],material_ids,slots)

aircraft=[]
def keep(ob):
    aircraft.append(ob);return ob

# Primary pressure shell and a rounded variable-geometry under-fuselage intake.
keep(build_fuselage())
def intake_section(x,zc,width,height,segments=48,power=.78):
    pts=[]
    for i in range(segments):
        a=2*math.pi*i/segments;c=math.cos(a);s=math.sin(a)
        pts.append((x,width*.5*math.copysign(abs(c)**power,c),zc+height*.5*math.copysign(abs(s)**power,s)))
    return pts
outer=[(.91,.56,.92,.79),(.45,.52,.79,.68),(-.12,.49,.63,.54)]
v=[];f=[];mi=[];n=48
for x,zc,w,h in outer:v.extend(intake_section(x,zc,w,h,n))
for i in range(len(outer)-1):
    for k in range(n):f.append((i*n+k,i*n+(k+1)%n,(i+1)*n+(k+1)%n,(i+1)*n+k));mi.append(0)
keep(obj_from_mesh("Intake | rounded variable-area lip",v,f,mi,[MAT[0]]))
black=[(.52,.54,.69,.57),(.16,.49,.56,.47),(-.64,.42,.35,.29)]
v=[];f=[];n=48
for x,zc,w,h in black:v.extend(intake_section(x,zc,w,h,n))
for i in range(len(black)-1):
    for k in range(n):f.append((i*n+k,i*n+(k+1)%n,(i+1)*n+(k+1)%n,(i+1)*n+k))
keep(obj_from_mesh("Intake | rounded shadowed variable duct",v,f,None,[MAT[5]]))
# Two thin inner ramp plates create a readable inlet cavity, rather than a dark sticker.
b=[[],[],[]]
add_box(b,(.13,0,.335),(.78,.68,.035),5,.0)
keep(simple_mesh("Intake | lower compression ramp",b,[MAT[5]]))

# Main cranked wing, leading-edge extensions and independently animated elevons.
wing_stations=[(.70,1.30,5.15,1.23),(1.48,.50,4.85,1.25),(2.45,-.35,4.20,1.30),
               (3.48,-1.28,3.15,1.36),(4.32,-2.25,1.90,1.42),(4.725,-2.88,1.35,1.44)]
for side,label in ((1,"Port"),(-1,"Starboard")):
    keep(wing_mesh("Wing | "+label+" cranked trapezoid",wing_stations,side=side,skip_aileron=True,nspan=84,nchord=48))
    keep(wing_mesh("Aileron | "+label+" elevon",wing_stations,side=side,t0=.69,t1=.965,span_lo=2.49,span_hi=4.36,nspan=32,nchord=16))
    # F-16 leading-edge flap segment, split from the wing along its hinge line.
    keep(wing_mesh("Leading edge flap | "+label,wing_stations,side=side,t0=.016,t1=.105,span_lo=1.52,span_hi=4.34,nspan=40,nchord=8))
    # Beefed-up trapezoidal LEX, blended visually into the root without a hard slab.
    lex=[(.62,2.55,.70,1.32),(1.15,1.78,1.18,1.28),(1.80,.30,1.42,1.25)]
    keep(wing_mesh("LEX | "+label+" leading-edge strake",lex,side=side,nspan=40,nchord=32,slots=[CAMO_MAT,CAMO_MAT]))

# Independent horizontal stabilators and vertical tail/rudder.
stab=[(.62,-4.10,2.48,1.25),(1.45,-4.90,1.96,1.27),(2.55,-5.94,.94,1.30)]
for side,label in ((1,"Port"),(-1,"Starboard")):
    keep(wing_mesh("Stabilator | "+label,stab,side=side,nspan=48,nchord=36))
fin=[(1.48,-4.40,2.42,0),(2.55,-4.62,2.05,0),(4.65,-5.36,1.05,0)]
keep(wing_mesh("Vertical tail | Netz fin",fin,span_axis="z",side=1,nspan=56,nchord=36,slots=[CAMO_MAT,MAT[3]]))
keep(wing_mesh("Rudder | independent trailing panel",fin,span_axis="z",side=1,t0=.70,t1=.985,span_lo=2.08,span_hi=4.47,nspan=36,nchord=16,slots=[CAMO_MAT,MAT[3]]))

# Twin dorsal airbrake petals sit flush on the spine and rotate from the centerline hinge.
for side,label in ((1,"Port"),(-1,"Starboard")):
    v=[];f=[];mids=[]
    for x in (-1.18,-3.38):
        for y in (0.0,side*.255):
            ry0,rz0,zc0=interp_station(FUSELAGE_KEYS,x)
            p=fuselage_surface(x,math.acos(max(-.999,min(.999,y/max(.001,ry0)))),.012)
            v.append(p)
    for p in list(v):v.append((p[0],p[1],p[2]-.017))
    f.extend([(0,1,3,2),(4,6,7,5),(0,2,6,4),(1,5,7,3),(0,4,5,1),(2,3,7,6)])
    mids.extend([0,1,0,0,0,0])
    keep(obj_from_mesh("Speedbrake | "+label+" dorsal petal",v,f,mids,[CAMO_MAT,MAT[3]]))

# Paired hydraulic jacks and hinge knuckles make the twin brake leaves read as
# working aircraft hardware instead of thin painted rectangles.
b=[[],[],[]]
for side in (-1,1):
    for x in (-1.38,-2.06):
        add_cylinder(b,(x,side*.16,1.82),(x-.36,side*.28,1.79),.018,8,10,radius2=.014)
        add_ellipsoid(b,(x,side*.16,1.82),(.045,.035,.026),8,8,12)
        add_ellipsoid(b,(x-.36,side*.28,1.79),(.034,.028,.022),8,8,12)
keep(simple_mesh("Speedbrake | exposed hydraulic jacks and hinge pins",b,[MAT[8]]))

# Two ventral strakes, a dorsal spine, tail bumper and intake-side boundary seams.
b=[[],[],[]]
for side in (-1,1):
    # thin swept ventral fins under the aft fuselage
    pts=[(-5.95,side*.38,.89),(-4.55,side*.38,.89),(-5.05,side*.63,.26),(-6.15,side*.49,.28)]
    start=len(b[0]);b[0].extend(pts);b[1].append((start,start+1,start+2,start+3));b[2].append(0)
    add_box(b,(-3.65,side*.42,1.48),(2.65,.12,.16),0,.0)
keep(simple_mesh("Aft | paired ventral fins and spine fairing",b,[CAMO_MAT,MAT[3]]))

# Nose, pitot probe, static ports and chin antennae.
b=[[],[],[]]
add_cylinder(b,(7.00,0,1.17),(7.57,0,1.17),.018,8,segments=8,radius2=.006)
add_ellipsoid(b,(6.70,0,1.18),(.20,.12,.105),17,8,16)
for side in (-1,1):
    add_cylinder(b,(6.12,side*.285,1.18),(6.40,side*.285,1.18),.012,8,segments=8,radius2=.012)
    add_ellipsoid(b,(4.58,side*.386,1.16),(.034,.012,.025),18,6,10)
    add_ellipsoid(b,(-2.82,side*.54,1.16),(.028,.012,.021),19,6,10)
# M61 muzzle is a recessed dark port on the port-side nose, followed by the
# characteristic small cooling/scupper slots in the forward gun-bay skin.
gun_x=3.17;gun_angle=2.39
gun_point=fuselage_surface(gun_x,gun_angle,.010)
gun_normal=Vector((0,math.cos(gun_angle),math.sin(gun_angle))).normalized()
add_cylinder(b,Vector(gun_point)-gun_normal*.018,Vector(gun_point)+gun_normal*.004,.025,5,16,radius2=.021)
for slot in range(5):
    x=3.44+slot*.072;a=2.22+slot*.025
    p=fuselage_surface(x,a,.007);q=fuselage_surface(x+.048,a-.012,.007)
    add_cylinder(b,p,q,.006,17,8,radius2=.004)
keep(simple_mesh("Nose | pitot, static ports, lights",b,[MAT[0],MAT[17],MAT[18],MAT[19],MAT[8],MAT[5]]))

# Bubble canopy: a polished, smooth one-piece shell with an uninterrupted glazing area.
v=[];f=[];rings=28;segments=64
cx,cy,cz=2.15,0,1.72;rx,ry,rz=1.31,.61,.91
for ri in range(rings+1):
    theta=(math.pi*.5)*ri/rings
    for si in range(segments):
        ang=2*math.pi*si/segments
        v.append((cx+rx*math.sin(theta)*math.cos(ang),cy+ry*math.sin(theta)*math.sin(ang),cz+rz*math.cos(theta)))
for ri in range(rings):
    for si in range(segments):
        a=ri*segments+si;b0=ri*segments+(si+1)%segments;c=(ri+1)*segments+(si+1)%segments;d=(ri+1)*segments+si
        f.append((a,b0,c,d))
keep(obj_from_mesh("Canopy | one-piece smoked bubble glass",v,f,None,[MAT[4]]))
b=[[],[],[]]
for i in range(128):
    a=2*math.pi*i/128
    x=cx+rx*math.cos(a);y=ry*math.sin(a);z=cz+.014
    if i:add_cylinder(b,prev,(x,y,z),.008,29,5)
    prev=(x,y,z)
# A formed forward bow follows the canopy curvature, instead of two straight diagonal bars.
t=.56;factor=math.sqrt(1-t*t);bowx=cx+rx*t
prev=None
for i in range(25):
    a=math.pi*i/24
    p=(bowx,ry*factor*math.cos(a),cz+rz*factor*math.sin(a))
    if prev:add_cylinder(b,prev,p,.010,29,6)
    prev=p
for i in range(24):
    a=2*math.pi*i/24
    add_ellipsoid(b,(cx+rx*.97*math.cos(a),ry*.97*math.sin(a),cz+.018),(.007,.007,.006),8,6,8)
keep(simple_mesh("Canopy | satin sill, bow and hidden fasteners",b,[MAT[29],MAT[8]]))

# A dressed single-seat office and seated pilot. All eye-line parts remain below
# the canopy crown so the glazings read as a bubble rather than a solid dark block.
b=[[],[],[]]
add_box(b,(1.80,0,1.47),(1.78,.88,.11),9)
add_ellipsoid(b,(1.42,0,1.82),(.17,.285,.38),10,16,24)
add_ellipsoid(b,(1.39,0,2.10),(.115,.205,.135),9,14,22)
add_ellipsoid(b,(1.76,0,1.57),(.29,.33,.13),9,14,22)
add_box(b,(1.39,0,2.01),(.045,.34,.035),14,-.08)
# Pilot shoulders, flight suit, harness, helmet and oxygen gear.
add_ellipsoid(b,(1.99,0,1.81),(.245,.30,.205),25,20,28)
add_ellipsoid(b,(2.03,0,2.05),(.165,.205,.135),25,18,24)
add_ellipsoid(b,(2.07,0,2.285),(.126,.142,.124),26,20,28)
add_ellipsoid(b,(2.192,0,2.285),(.018,.108,.050),27,16,24)
add_ellipsoid(b,(2.18,0,2.202),(.033,.055,.038),28,12,16)
# Fine helmet ear cups, visor edge, oxygen-hose coupling and a lower chin strap.
for side in (-1,1):
    add_ellipsoid(b,(2.055,side*.136,2.285),(.045,.018,.052),28,10,14)
    add_cylinder(b,(2.12,side*.104,2.335),(2.21,side*.086,2.335),.006,8,8)
    add_cylinder(b,(2.13,side*.10,2.245),(2.055,side*.115,2.17),.009,8,8)
add_cylinder(b,(2.16,-.060,2.18),(2.04,-.21,1.96),.012,28,9)
add_cylinder(b,(2.16,.060,2.18),(2.04,.21,1.96),.012,28,9)
add_cylinder(b,(1.94,-.22,2.02),(1.79,-.285,1.78),.018,14,10)
add_cylinder(b,(1.94,.22,2.02),(1.79,.285,1.78),.018,14,10)
for side in (-1,1):
    # Sleeved arms reach the stick and throttle; legs stay clear of the pedals.
    add_cylinder(b,(2.00,side*.18,1.94),(2.26,side*.23,1.78),.056,25,12)
    add_cylinder(b,(2.26,side*.23,1.78),(2.55,side*.14,1.75),.046,25,12)
    add_ellipsoid(b,(2.57,side*.14,1.75),(.052,.045,.040),10,10,14)
    add_ellipsoid(b,(1.99,side*.17,1.58),(.19,.12,.11),25,14,18)
    add_cylinder(b,(2.08,side*.17,1.57),(2.39,side*.16,1.49),.052,25,10)
    add_ellipsoid(b,(2.40,side*.16,1.49),(.095,.065,.055),10,12,16)
for side in (-1,1):
    add_box(b,(1.82,side*.39,1.75),(.90,.10,.18),9)
    add_box(b,(1.72,side*.38,1.86),(.66,.065,.035),14,-.05)
    add_cylinder(b,(2.43,side*.32,1.66),(2.47,side*.34,1.88),.026,8,10)
    # Seat harnesses and ejection rails
    add_cylinder(b,(1.13,side*.25,1.63),(1.28,side*.25,2.04),.022,8,10)
    add_cylinder(b,(1.53,side*.22,1.65),(1.61,side*.22,2.03),.020,8,10)
    add_cylinder(b,(1.72,side*.23,1.98),(1.88,-side*.23,1.94),.018,14,10)
    add_cylinder(b,(1.86,side*.23,2.05),(2.02,-side*.23,2.01),.017,14,10)
# A real-looking layered glareshield, MFD bezels, standby dials, stick and throttle.
add_ellipsoid(b,(2.93,0,1.77),(.24,.39,.13),9,14,24)
for side in (-1,0,1):
    add_box(b,(2.98,side*.25,1.88),(.018,.18,.085),11,-.05)
    add_box(b,(2.994,side*.25,1.88),(.009,.145,.061),17,-.05)
for side in (-1,1):
    add_cylinder(b,(3.12,side*.40,1.78),(3.14,side*.40,1.88),.040,8,20)
    add_cylinder(b,(3.145,side*.40,1.83),(3.155,side*.40,1.83),.031,9,20)
    for button in range(5):
        y=side*(.13+button*.045)
        add_ellipsoid(b,(3.014,y,1.72),(.010,.010,.012),14 if button==0 else 8,6,8)
    for dial in range(3):
        y=side*(.12+dial*.10)
        add_cylinder(b,(3.00,y,1.71),(3.026,y,1.71),.014,8,10)
add_cylinder(b,(2.58,0,1.60),(2.62,0,1.83),.018,8,10)
add_ellipsoid(b,(2.62,0,1.86),(.060,.039,.043),9,10,14)
add_box(b,(2.28,-.34,1.62),(.25,.085,.085),8)
keep(simple_mesh("Cockpit | pilot, ejection seat, MFD bezels and controls",b,[MAT[9],MAT[10],MAT[8],MAT[11],MAT[14],MAT[17],MAT[25],MAT[26],MAT[27],MAT[28]]))
# HUD combiner plate as a separately identifiable transparent-looking tinted plate.
b=[[],[],[]]
b[0].extend([(2.84,-.14,1.94),(2.84,.14,1.94),(2.94,.14,2.14),(2.94,-.14,2.14)]);b[1].append((0,1,2,3));b[2].append(0)
keep(simple_mesh("Cockpit | HUD combiner",b,[MAT[4]]))

# Engine nozzle: concentric hot section rings, 16 overlapping articulated petals, inner dark tube.
b=[[],[],[]]
for x,rad,mid in ((-6.66,.60,7),(-6.91,.58,7),(-7.13,.51,7),(-7.20,.43,5)):
    for i in range(32):
        a=2*math.pi*i/32
        b[0].append((x,rad*math.cos(a),1.17+rad*math.sin(a)))
for ring in range(3):
    for i in range(32):
        a=ring*32+i;bb=ring*32+(i+1)%32;c=(ring+1)*32+(i+1)%32;d=(ring+1)*32+i
        b[1].append((a,bb,c,d));b[2].append(0 if i%2 else 1)
keep(simple_mesh("Exhaust | concentric hot-section rings",b,[MAT[7],MAT[5]]))
for i in range(16):
    a=2*math.pi*i/16
    b=[[],[],[]]
    for x,rad,w in ((-6.67,.58,.105),(-7.08,.50,.076)):
        for side in (-1,1):
            q=a+side*w/rad
            b[0].append((x,rad*math.cos(q),1.17+rad*math.sin(q)))
    b[1].extend([(0,1,3,2)]);b[2].append(0)
    keep(simple_mesh("Exhaust petal | %02d"%i,b,[MAT[15 if i%2 else 16]]))
    # small outer cooling louvre fastener
    bb=[[],[],[]]
    add_ellipsoid(bb,(-6.82,.60*math.cos(a),1.17+.60*math.sin(a)),(.025,.018,.018),8,5,8)
    keep(simple_mesh("Exhaust detail | %02d"%i,bb,[MAT[8]]))

# Main gear legs and rotating tires. The model's service pose is extended.
for x,side,label,radius in ((2.25,0,"Nose",.255),(-1.55,1,"Port",.405),(-1.55,-1,"Starboard",.405)):
    b=[[],[],[]]
    y=0 if side==0 else side*1.38
    top=(x,y*.72,.92 if side==0 else 1.04)
    # Tire centers sit one radius above the runway in the service pose.
    hub=(x-.08,y,radius)
    add_cylinder(b,top,hub,.046 if side==0 else .057,8,12,radius2=.036 if side==0 else .047)
    # Polished oleo piston, scissor link, brake hose and axle shoulder.
    add_cylinder(b,(top[0]-.03,top[1],top[2]-.09),(hub[0]+.005,hub[1],hub[2]+.17),.020,8,12,radius2=.022)
    add_cylinder(b,(hub[0]-.06,y*.78,hub[2]+.08),(hub[0]+.08,y*.87,hub[2]+.24),.014,8,8)
    add_cylinder(b,(hub[0]+.08,y*.87,hub[2]+.24),(hub[0]+.17,y*.84,hub[2]+.10),.013,8,8)
    add_cylinder(b,(hub[0]+.01,y*.75,hub[2]+.24),(hub[0]+.15,y*.82,hub[2]+.06),.009,8,8)
    add_ellipsoid(b,(hub[0],hub[1],hub[2]),(.066,.045,.066),8,10,12)
    add_box(b,(x+.12,y*.65,.72),(.42,.25,.055),3)
    add_box(b,(x+.34,y*.65,.67),(.29,.19,.04),3,-.16)
    keep(simple_mesh("Landing gear | "+label+" shock strut and doors",b,[MAT[8],MAT[3]]))
    # wheel itself is a tire ring aligned to the lateral axle.
    b=[[],[],[]]
    center=Vector(hub)
    for j in range(5):
        yy=y-.12+j*.06
        rad=radius*(.88 if j in (0,4) else (1.0 if j in (1,3) else .985))
        for i in range(32):
            ang=2*math.pi*i/32
            b[0].append((center.x+rad*math.cos(ang),yy,center.z+rad*math.sin(ang)))
    for j in range(4):
        for i in range(32):
            a=j*32+i;bb=j*32+(i+1)%32;c=(j+1)*32+(i+1)%32;d=(j+1)*32+i
            b[1].append((a,bb,c,d));b[2].append(0)
    keep(simple_mesh("Wheel | "+label+" high-pressure tyre",b,[MAT[6],MAT[8]]))
    # Three shallow circumferential channels and staggered shoulder cuts make the rubber read as a tyre.
    b=[[],[],[]]
    for groove_y in (y-.045,y,y+.045):
        major=radius*.978;minor=.004
        base=len(b[0]);segments=64;tube=6
        for i in range(segments):
            a=2*math.pi*i/segments
            for j in range(tube):
                q=2*math.pi*j/tube
                rr=major+minor*math.cos(q)
                b[0].append((center.x+rr*math.cos(a),groove_y+minor*math.sin(q),center.z+rr*math.sin(a)))
        for i in range(segments):
            for j in range(tube):
                a=base+i*tube+j;bb=base+i*tube+(j+1)%tube
                c=base+((i+1)%segments)*tube+(j+1)%tube;d=base+((i+1)%segments)*tube+j
                b[1].append((a,bb,c,d));b[2].append(0)
    keep(simple_mesh("Wheel | "+label+" circumferential tread channels",b,[MAT[17]]))
    b=[[],[],[]]
    for hubside in (-1,1):
        yy=y+hubside*.125
        add_cylinder(b,(center.x,yy-.006,center.z),(center.x,yy+.006,center.z),radius*.43,8,24)
        for bolt in range(10):
            a=2*math.pi*bolt/10
            add_ellipsoid(b,(center.x+radius*.30*math.cos(a),yy+hubside*.012,center.z+radius*.30*math.sin(a)),(.022,.012,.022),8,5,8)
    keep(simple_mesh("Wheel hub | "+label+" ventilated disc",b,[MAT[8]]))

# Gear bay recesses, retract doors, fasteners and brake line details.
b=[[],[],[]]
for side in (-1,1):
    add_box(b,(-1.55,side*.77,.72),(1.18,.64,.045),5)
    add_box(b,(-1.55,side*1.13,.95),(.92,.16,.048),3,-.10)
    add_cylinder(b,(-1.55,side*1.22,.47),(-1.55,side*1.39,.47),.012,8,8)
add_box(b,(2.24,0,.75),(.84,.35,.042),5)
add_box(b,(2.26,0,.98),(.70,.18,.042),3)
keep(simple_mesh("Landing gear | wheel wells, brake lines and doors",b,[MAT[5],MAT[3],MAT[8]]))

# Inboard tanks, pylons and empty hardpoints retain a practical low-drag ferry fit.
for side,label in ((-1,"Port"),(1,"Starboard")):
    b=[[],[],[]]
    y=side*2.05
    # pylon profile, mounted close to the wing's underside
    add_box(b,(-1.95,y,1.06),(.72,.18,.40),0)
    # 370-gallon class external tank represented with a multi-station ogive shell
    tank_st=[(-3.60,.02,.025,0),(-3.34,.16,.18,0),(-2.90,.25,.24,0),(-1.95,.27,.25,0),(-1.12,.20,.18,0),(-.86,.02,.03,0)]
    tv=[];tf=[];rings=20
    for x,ry,rz,zc0 in tank_st:
        for k in range(rings):
            ang=2*math.pi*k/rings
            tv.append((x,y+ry*math.cos(ang),.91+rz*math.sin(ang)))
    for si in range(len(tank_st)-1):
        for k in range(rings):
            a=si*rings+k;bb=si*rings+(k+1)%rings;c=(si+1)*rings+(k+1)%rings;d=(si+1)*rings+k
            tf.append((a,bb,c,d))
    keep(obj_from_mesh("External fuel tank | "+label,tv,tf,None,[MAT[3]]))
    keep(simple_mesh("Pylon | "+label+" station",b,[MAT[0]]))
    b=[[],[],[]]
    add_box(b,(-3.56,side*4.58,1.43),(.44,.10,.17),0)
    add_ellipsoid(b,(-3.32,side*4.60,1.43),(.13,.052,.050),18,8,14)
    keep(simple_mesh("Wingtip launcher | "+label,b,[MAT[0],MAT[18]]))
    # Correctly proportioned wingtip AIM-9 class store: seeker, body bands,
    # and four cruciform tail/forward control fins.
    my=side*4.66;mz=1.43;stations=[(-4.96,.012),(-4.82,.045),(-4.65,.064),(-4.03,.064),(-3.99,.066),(-3.95,.064),(-2.35,.064),(-2.31,.066),(-2.27,.064),(-2.15,.055),(-2.08,.025)]
    mv=[];mf=[];mm=[];segments=24
    for x,radius in stations:
        for k in range(segments):
            a=2*math.pi*k/segments
            mv.append((x,my+radius*math.cos(a),mz+radius*math.sin(a)))
    for j in range(len(stations)-1):
        for k in range(segments):
            a=j*segments+k;bb=j*segments+(k+1)%segments;c=(j+1)*segments+(k+1)%segments;d=(j+1)*segments+k
            mat_id=2 if (-4.03<stations[j][0]<-3.95 or -2.35<stations[j][0]<-2.27) else (1 if j>=8 else 0)
            mf.append((a,bb,c,d));mm.append(mat_id)
    for fore in (False,True):
        finx=-4.58 if not fore else -2.43;span=.22 if not fore else .15;chord=.34 if not fore else .28
        for axis in ("y","z"):
            for sign in (-1,1):
                if axis=="y":
                    pts=[(finx-chord*.5,my,mz),(finx+chord*.45,my,mz),(finx+chord*.10,my+sign*span,mz)]
                else:
                    pts=[(finx-chord*.5,my,mz),(finx+chord*.45,my,mz),(finx+chord*.10,my,mz+sign*span)]
                base=len(mv);mv.extend(pts);mf.append((base,base+1,base+2));mm.append(0)
    keep(obj_from_mesh("AIM-9 | "+label+" wingtip air-to-air missile",mv,mf,mm,[MAT[21],MAT[22],MAT[23]]))
    # Navigation light housings, red port and green starboard.
    b=[[],[],[]]
    add_ellipsoid(b,(-2.84,side*4.68,1.27),(.085,.045,.038),19 if side<0 else 18,8,12)
    keep(simple_mesh("Wingtip nav lamp | "+label,b,[MAT[18 if side>0 else 19]]))

# Streamlined 300-gallon class centerline tank and small ventral antennas.
b=[[],[],[]]
add_box(b,(-1.18,0,.90),(.92,.22,.25),0)
center_tank=[(-3.64,.015,.02,0),(-3.36,.17,.16,0),(-2.86,.265,.25,0),(-2.10,.31,.29,0),(-.85,.29,.27,0),(.18,.21,.20,0),(.73,.09,.09,0),(.90,.015,.02,0)]
tv=[];tf=[];rings=32
for x,ry0,rz0,_ in center_tank:
    for k in range(rings):
        a=2*math.pi*k/rings;tv.append((x,ry0*math.cos(a),.48+rz0*math.sin(a)))
for si in range(len(center_tank)-1):
    for k in range(rings):
        a=si*rings+k;bb=si*rings+(k+1)%rings;c=(si+1)*rings+(k+1)%rings;d=(si+1)*rings+k;tf.append((a,bb,c,d))
keep(obj_from_mesh("Centerline tank | 300 gallon streamlined drop tank",tv,tf,None,[MAT[3]]))
for side in (-1,1):
    add_cylinder(b,(4.5,side*.24,.91),(4.67,side*.26,.66),.026,8,10)
keep(simple_mesh("Centerline store | conformal tank and antennas",b,[MAT[0],MAT[3],MAT[8]]))

# Airframe surface detail builders: fine panel seams, fasteners and service hatches.
def wing_top_z(y, t):
    xle,chord,zc=interp_station(wing_stations,abs(y))
    return zc+0.012*chord*math.sin(math.pi*t)+profile_thickness(t,chord)+.008

b=[[],[],[]]
for side in (-1,1):
    # Leading/trailing edges, flap and aileron outlines, access panels.
    for t in (.035,.115,.66,.69,.965):
        last=None
        for yi in range(12,47):
            y=side*yi*.1
            if abs(y)>4.62:continue
            xle,chord,zc=interp_station(wing_stations,abs(y));p=(xle-chord*t,y,wing_top_z(y,t))
            if last:add_cylinder(b,last,p,.0025,24,5)
            last=p
    for y in (1.85,2.10,2.75,3.12,3.78,4.12):
        xle,chord,zc=interp_station(wing_stations,y)
        for t in (.20,.42,.58):
            add_ellipsoid(b,(xle-chord*t,side*y,wing_top_z(side*y,t)+.002),(.008,.008,.003),8,6,8)
    # Raised inspection panels represented by fine clipped perimeter seams.
    for y0,y1,t0,t1 in ((1.85,2.38,.22,.43),(2.78,3.31,.26,.43),(3.55,3.96,.32,.51)):
        corners=[]
        for y,t in ((y0,t0),(y1,t0),(y1,t1),(y0,t1),(y0,t0)):
            xle,chord,zc=interp_station(wing_stations,y);corners.append((xle-chord*t,side*y,wing_top_z(side*y,t)))
        for a,p in zip(corners,corners[1:]):add_cylinder(b,a,p,.0025,24,6)
        # Visible flush-head fasteners at each cover corner, still low enough to
        # read as countersunk hardware rather than exaggerated raised bolts.
        for y,t in ((y0,t0),(y1,t0),(y1,t1),(y0,t1)):
            xle,chord,zc=interp_station(wing_stations,y)
            add_ellipsoid(b,(xle-chord*t,side*y,wing_top_z(side*y,t)+.004),(.010,.010,.003),8,6,8)
    # Short metal hinge knuckles articulate the leading flap and elevon seams.
    for y in (1.72,2.46,3.18,3.87):
        xle,chord,zc=interp_station(wing_stations,y)
        x=xle-chord*.69;z=wing_top_z(side*y,.69)+.006
        add_cylinder(b,(x,side*(y-.055),z),(x,side*(y+.055),z),.012,8,10)
keep(simple_mesh("Wing detail | panel seams, inspection covers and flush fasteners",b,[MAT[24],MAT[8]]))

# Fuselage skin joints and service panels follow the actual curved airframe skin.
b=[[],[],[]]
def fuselage_top_point(x,y,offset=.008):
    ry,rz,zc=interp_station(FUSELAGE_KEYS,x)
    a=math.acos(max(-.999,min(.999,y/max(.001,ry))))
    return fuselage_surface(x,a,offset)
for x in (-5.55,-4.18,-1.22,1.36,3.84,5.82,6.055):
    # Structural joints are fine, dark breaks rather than bright raised hoops.
    for k in range(65):
        a0=2*math.pi*k/64;a1=2*math.pi*(k+1)/64
        p=fuselage_surface(x,a0,.005);q=fuselage_surface(x,a1,.005)
        add_cylinder(b,p,q,.0017,24,5)
for side in (-1,1):
    for x in (-5.30,-4.70,-3.10,-2.34,-1.42,-.45,.62,1.28,3.92,4.55):
        ry,rz,zc=interp_station(FUSELAGE_KEYS,x)
        center=math.acos(side*.89)
        for k in range(5):
            a=center+(k-2)*.10
            p=fuselage_surface(x,a,.011)
            add_ellipsoid(b,p,(.005,.005,.005),8,5,7)
    # Flush top-side avionics panels mapped to the local fuselage curvature.
    for x,length,width in ((-4.52,.48,.23),(-3.72,.34,.17),(-.42,.42,.20),(4.18,.38,.19)):
        corners=[(x-length/2,-width/2),(x+length/2,-width/2),(x+length/2,width/2),(x-length/2,width/2),(x-length/2,-width/2)]
        points=[fuselage_top_point(px,py) for px,py in corners]
        for a,p in zip(points,points[1:]):add_cylinder(b,a,p,.0024,24,6)
    # pairs of shallow cooling slots on the fuselage side
    for i in range(7):
        x=-3.3+i*.12
        a=math.acos(side*.92)
        p=fuselage_surface(x,a,.012);q=fuselage_surface(x+.035,a-.075,.012)
        add_cylinder(b,p,q,.0035,24,6)
keep(simple_mesh("Fuselage detail | fasteners, access panels and cooling slots",b,[MAT[24],MAT[8]]))

# Star of David roundels on upper and lower wing panels: white circle, blue ring/star.
def flat_roundel(name, x, y, z, radius, underside=False):
    v=[];f=[];mids=[]
    v.append((x,y,z))
    for i in range(64):
        a=2*math.pi*i/64
        v.append((x+radius*math.cos(a),y+radius*math.sin(a),z))
    for i in range(64):f.append((0,1+i,1+(i+1)%64));mids.append(0)
    ob=obj_from_mesh(name+" | white field",v,f,mids,[MAT[12]])
    keep(ob)
    # Continuous blue rim and a filled six-point Star of David.
    b=[[],[],[]]
    for i in range(64):
        a=2*math.pi*i/64
        b[0].extend(((x+radius*.91*math.cos(a),y+radius*.91*math.sin(a),z+.004),
                     (x+radius*.97*math.cos(a),y+radius*.97*math.sin(a),z+.004)))
    for i in range(64):
        a=i*2;bb=((i+1)%64)*2
        b[1].append((a,bb,bb+1,a+1));b[2].append(0)
    keep(obj_from_mesh(name+" | blue circumference",b[0],b[1],b[2],[MAT[13]]))
    star_verts=[(x,y,z+.009)]
    for i in range(12):
        a=-math.pi/2+i*math.pi/6;r=radius*(.53 if i%2==0 else .245)
        star_verts.append((x+r*math.cos(a),y+r*math.sin(a),z+.009))
    star_faces=[(0,1+i,1+(i+1)%12) for i in range(12)]
    keep(obj_from_mesh(name+" | filled blue star",star_verts,star_faces,None,[MAT[13]],smooth=False))
    return ob

for side in (-1,1):
    x=-1.95;y=side*2.65;z=wing_top_z(y,.42)+.018
    flat_roundel("IDF upper wing roundel | "+str(side),x,y,z,.39)
    # Small rescue triangle stencil and fuel markings.
    b=[[],[],[]]
    add_box(b,(-.18,side*2.18,wing_top_z(side*2.18,.39)+.02),(.30,.13,.012),14)
    keep(simple_mesh("Wing | safety stencil "+str(side),b,[MAT[14]]))

def side_roundel(name,x,y,z,radius,side):
    # Side insignia is wrapped onto the elliptical pressure shell instead of
    # floating as a flat plate across the changing fuselage cross-section.
    def skin(px,pz,offset=.010):
        ry,rz,zc=interp_station(FUSELAGE_KEYS,px)
        a=math.asin(max(-.98,min(.98,(pz-zc)/max(.001,rz))))
        return fuselage_surface(px,a if side>0 else math.pi-a,offset)
    verts=[skin(x,z)];faces=[]
    for i in range(64):
        a=2*math.pi*i/64
        verts.append(skin(x+radius*math.cos(a),z+radius*math.sin(a)))
    for i in range(64):faces.append((0,1+i,1+(i+1)%64))
    keep(obj_from_mesh(name+" | white field",verts,faces,None,[MAT[12]]))
    b=[[],[],[]]
    for i in range(64):
        a=2*math.pi*i/64
        b[0].extend((skin(x+radius*.91*math.cos(a),z+radius*.91*math.sin(a),.015),
                     skin(x+radius*.97*math.cos(a),z+radius*.97*math.sin(a),.015)))
    for i in range(64):
        a=i*2;bb=((i+1)%64)*2;b[1].append((a,bb,bb+1,a+1));b[2].append(0)
    keep(obj_from_mesh(name+" | blue circumference",b[0],b[1],b[2],[MAT[13]]))
    star=[skin(x,z,.018)]
    for i in range(12):
        a=-math.pi/2+i*math.pi/6;r=radius*(.53 if i%2==0 else .245)
        star.append(skin(x+radius*(r/radius)*math.cos(a),z+radius*(r/radius)*math.sin(a),.018))
    faces=[(0,1+i,1+(i+1)%12) for i in range(12)]
    keep(obj_from_mesh(name+" | filled blue Star of David",star,faces,None,[MAT[13]],smooth=False))

for side in (-1,1):
    ry,rz,zc=interp_station(FUSELAGE_KEYS,-.22)
    side_roundel("IDF fuselage roundel | "+str(side),-.22,0,zc+.10,.245,side)

# Tail serial and warning stencils as low-profile, individually placed lettering.
def add_text(name, body, loc, size, rotation, mat):
    bpy.ops.object.text_add(location=loc, rotation=rotation)
    ob=bpy.context.object;ob.name=name;ob.data.body=body;ob.data.size=size;ob.data.extrude=.0008;ob.data.bevel_depth=.0005
    ob.data.materials.append(mat)
    ob.data.align_x="CENTER";ob.data.align_y="CENTER"
    bpy.ops.object.convert(target="MESH")
    ob=bpy.context.object;ob.name=name
    aircraft.append(ob)
    return ob

for side in (-1,1):
    face_rot=math.pi/2 if side<0 else -math.pi/2
    add_text("Tail | serial stencil", "118", (-5.19,side*.055,3.56),.24,(face_rot,0,0),MAT[12])
    add_text("Tail | rescue triangle", "DANGER", (-5.05,side*.055,2.38),.105,(face_rot,0,0),MAT[17])
    add_text("Fuselage | intake warning", "NO STEP", (.20,side*.57,1.48),.095,(math.pi/2,0,0),MAT[17])
    add_text("Wing | station stencil", "120", (-1.15,side*1.88,wing_top_z(side*1.88,.29)+.012),.075,(0,0,math.pi/2),MAT[17])
    # Small maintenance lettering gives the broad wing panels a believable
    # service scale without turning the operational camouflage into a decal sheet.
    xle,chord,zc=interp_station(wing_stations,2.36)
    add_text("Wing | fuel service stencil", "FUEL", (xle-chord*.40,side*2.36,wing_top_z(side*2.36,.40)+.014),.073,(0,0,math.pi/2),MAT[17])
    xle,chord,zc=interp_station(wing_stations,3.55)
    add_text("Wing | no-step stencil", "NO STEP", (xle-chord*.46,side*3.55,wing_top_z(side*3.55,.46)+.014),.066,(0,0,0),MAT[17])

# Drain masts, antenna fairings, static wicks and tie-down fittings.
b=[[],[],[]]
for side in (-1,1):
    for x in (-3.65,-2.25,-.65,1.05):
        add_cylinder(b,(x,side*.27,.64),(x-.03,side*.28,.42),.012,8,8)
    for x,y in ((-5.9,.0),(-2.9,.0),(3.9,.0)):
        add_box(b,(x,side*(y+.23),.84),(.28,.10,.16),3)
    add_cylinder(b,(-6.98,side*.13,1.12),(-7.18,side*.13,.98),.018,8,8)
keep(simple_mesh("Underbody | drains, antennas and static wicks",b,[MAT[3],MAT[8]]))

# Surface-level livery paint weathering and basecoat variation on the roughness is
# carried by the Blender material values; geometry stays within a light runtime budget.

# A controlled hero render for visual review. Only the aircraft collection is exported.
scene=bpy.context.scene
world=bpy.data.worlds.new("Storm coast studio")
scene.world=world;world.use_nodes=True
world.node_tree.nodes["Background"].inputs[0].default_value=(.075,.094,.105,1)
world.node_tree.nodes["Background"].inputs[1].default_value=.38
scene.render.engine="BLENDER_EEVEE"
scene.render.resolution_x=1920;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.view_settings.view_transform="AgX"
scene.render.film_transparent=False

def area(name, location, energy, color, size, target=(0,0,1.6)):
    data=bpy.data.lights.new(name,"AREA");data.energy=energy;data.color=color;data.shape="DISK";data.size=size
    ob=bpy.data.objects.new(name,data);scene.collection.objects.link(ob);ob.location=location
    ob.rotation_euler=(Vector(target)-ob.location).to_track_quat("-Z","Y").to_euler()

area("Large softbox | warm key",(8,-11,12),2300,(1.0,.86,.72),9)
area("Cool sky strip",(-8,4,8),1750,(.62,.77,1.0),8)
area("Nose fill",(10,4,4),1050,(1.0,.94,.84),6)
area("Intake bounce",(5,0,-5),720,(.62,.73,.80),5,target=(0,0,.7))

# Ground plane with a matte desaturated coastal apron finish.
groundmat=material("Studio ground | wet basalt",(.085,.105,.11),.14,.42)
bpy.ops.mesh.primitive_plane_add(size=200, location=(0,0,-.005))
floor=bpy.context.object;floor.name="Preview-only floor";floor.data.materials.append(groundmat)

camdata=bpy.data.cameras.new("F16 Netz product camera");cam=bpy.data.objects.new("F16 Netz product camera",camdata);scene.collection.objects.link(cam)
cam.location=(18,-19,12.2);target=Vector((0,0,1.75));cam.rotation_euler=(target-Vector(cam.location)).to_track_quat("-Z","Y").to_euler()
camdata.lens=55;scene.camera=cam
scene.render.image_settings.color_mode="RGBA"
scene.render.filepath=os.path.join(SHOTS,"F16_Netz_Blender_HighDetail.png")
scene.render.resolution_percentage=100
scene.render.engine="BLENDER_EEVEE"
scene.render.image_settings.compression=20
scene.render.filepath=os.path.join(SHOTS,"F16_Netz_Blender_HighDetail.png")
scene.eevee.taa_render_samples=72

# Keep editable source with studio setup, then render.
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ASSETS,"f16_netz_source.blend"))
bpy.ops.render.render(write_still=True)

# Orthographic product checks expose proportions that the hero angle can hide.
render_views=(
    ("Top",(0,0,22),(0,0,1.30),"ORTHO",18.0),
    ("Side",(0,-22,5.2),(0,0,1.55),"ORTHO",18.0),
    ("Nose",(15,0,4.0),(0,0,1.45),"PERSP",55.0),
    ("Intake",(15,0,-2.0),(0,0,.45),"PERSP",55.0),
)
for label,location,target,kind,lens in render_views:
    floor.hide_render=(label=="Intake")
    cam.location=location;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat("-Z","Y").to_euler()
    camdata.type=kind
    if kind=="ORTHO":camdata.ortho_scale=lens
    else:camdata.lens=lens
    scene.render.filepath=os.path.join(SHOTS,"F16_Netz_Blender_"+label+".png")
    bpy.ops.render.render(write_still=True)
floor.hide_render=False
camdata.type="PERSP";camdata.lens=55
cam.location=(18,-19,12.2);cam.rotation_euler=(Vector((0,0,1.75))-cam.location).to_track_quat("-Z","Y").to_euler()

# Export only aircraft parts; stage camera/lights/floor remain in the .blend source.
bpy.ops.object.select_all(action="DESELECT")
for ob in aircraft:
    if ob.name in bpy.data.objects:
        ob.select_set(True)
bpy.context.view_layer.objects.active=aircraft[0]
glb=os.path.join(ASSETS,"f16_netz.glb")
bpy.ops.export_scene.gltf(filepath=glb, export_format="GLB", use_selection=True,
                          export_yup=True, export_apply=True, export_materials="EXPORT",
                          export_keep_originals=False, export_cameras=False,
                          export_lights=False, export_animations=False,
                          export_image_format="AUTO", export_texcoords=True,
                          export_normals=True, export_tangents=False)

# Build a stable mesh/material index map from the actual GLB document.
def read_glb_json(path):
    with open(path,"rb") as fp:
        header=fp.read(12); magic,version,length=__import__("struct").unpack("<III",header)
        chunk_len,chunk_type=__import__("struct").unpack("<II",fp.read(8))
        if magic!=0x46546C67 or chunk_type!=0x4E4F534A:
            raise RuntimeError("Unexpected GLB header")
        return json.loads(fp.read(chunk_len).decode("utf-8"))

gltf=read_glb_json(glb)
mesh_names=[mesh.get("name","") for mesh in gltf.get("meshes",[])]
mat_names=[mat.get("name","") for mat in gltf.get("materials",[])]
mesh_starts=[];raylib_mesh_count=0
for mesh in gltf.get("meshes",[]):
    mesh_starts.append(raylib_mesh_count)
    raylib_mesh_count+=len(mesh.get("primitives",[]))
node_meshes=[]
for node in gltf.get("nodes",[]):
    if "mesh" in node:
        mesh_idx=node["mesh"]
        node_meshes.append({"node":node.get("name",""),"mesh":mesh_idx,
                            "raylib_mesh_start":mesh_starts[mesh_idx],
                            "raylib_mesh_count":len(gltf["meshes"][mesh_idx].get("primitives",[]))})
mapping={"mesh_count":len(mesh_names),"raylib_mesh_count":raylib_mesh_count,"material_count":len(mat_names),"meshes":mesh_names,
         "materials":mat_names,"nodes":node_meshes,"dimensions_m":{"length_including_probe":15.15,"wingspan":9.45,"height_to_tail":4.94},
         "model_origin":"aircraft centerline near wing root; nose is local +X; vertical local +Y in raylib after glTF Y-up conversion"}
with open(os.path.join(ASSETS,"f16_netz_mesh_map.json"),"w",encoding="utf-8") as fp:
    json.dump(mapping,fp,indent=2)

def find_mesh(prefix):
    for i,name in enumerate(mesh_names):
        if name.startswith(prefix): return mesh_starts[i]
    return -1
def find_mesh_range(prefix):
    for i,name in enumerate(mesh_names):
        if name.startswith(prefix):return (mesh_starts[i],len(gltf["meshes"][i].get("primitives",[])))
    return (-1,0)
def find_mat(needle):
    for i,name in enumerate(mat_names):
        if needle in name:return i
    return -1

ids={"Canopy":find_mesh("Canopy | one-piece"),"NoseWheel":find_mesh("Wheel | Nose"),
     "PortMainWheel":find_mesh("Wheel | Port"),"StarboardMainWheel":find_mesh("Wheel | Starboard"),
     "PortAileron":find_mesh("Aileron | Port"),"StarboardAileron":find_mesh("Aileron | Starboard"),
     "PortLeadingEdgeFlap":find_mesh("Leading edge flap | Port"),"StarboardLeadingEdgeFlap":find_mesh("Leading edge flap | Starboard"),
     "PortStabilator":find_mesh("Stabilator | Port"),"StarboardStabilator":find_mesh("Stabilator | Starboard"),
     "Rudder":find_mesh("Rudder | independent"),"GearNose":find_mesh("Landing gear | Nose"),
     "GearPort":find_mesh("Landing gear | Port"),"GearStarboard":find_mesh("Landing gear | Starboard"),
     "NoseWheelHub":find_mesh("Wheel hub | Nose"),"PortWheelHub":find_mesh("Wheel hub | Port"),
     "StarboardWheelHub":find_mesh("Wheel hub | Starboard"),"CanopyMaterial":find_mat("Canopy | warm olive bronze")+1,
     "GlassMaterial":find_mat("Canopy | warm olive bronze")+1,"IntakeMaterial":find_mat("Intake | deep carbon")+1,
     "PortSpeedbrake":find_mesh("Speedbrake | Port"),"StarboardSpeedbrake":find_mesh("Speedbrake | Starboard"),
     "PortMissile":find_mesh("AIM-9 | Port"),"StarboardMissile":find_mesh("AIM-9 | Starboard")}
dynamic_ranges={
    "CanopyFrame":find_mesh_range("Canopy | satin sill, bow and hidden fasteners"),
    "CockpitHud":find_mesh_range("Cockpit | HUD combiner"),
    "CockpitInterior":find_mesh_range("Cockpit | pilot, ejection seat, MFD bezels and controls"),
    "PortAileron":find_mesh_range("Aileron | Port"),"StarboardAileron":find_mesh_range("Aileron | Starboard"),
    "PortLeadingEdgeFlap":find_mesh_range("Leading edge flap | Port"),"StarboardLeadingEdgeFlap":find_mesh_range("Leading edge flap | Starboard"),
    "PortStabilator":find_mesh_range("Stabilator | Port"),"StarboardStabilator":find_mesh_range("Stabilator | Starboard"),
    "Rudder":find_mesh_range("Rudder | independent"),
    "PortSpeedbrake":find_mesh_range("Speedbrake | Port"),"StarboardSpeedbrake":find_mesh_range("Speedbrake | Starboard"),
    "GearNose":find_mesh_range("Landing gear | Nose"),"GearPort":find_mesh_range("Landing gear | Port"),
    "GearStarboard":find_mesh_range("Landing gear | Starboard"),"GearBays":find_mesh_range("Landing gear | wheel wells"),
    "NoseWheel":find_mesh_range("Wheel | Nose"),"PortMainWheel":find_mesh_range("Wheel | Port"),
    "StarboardMainWheel":find_mesh_range("Wheel | Starboard"),"NoseWheelHub":find_mesh_range("Wheel hub | Nose"),
    "PortWheelHub":find_mesh_range("Wheel hub | Port"),"StarboardWheelHub":find_mesh_range("Wheel hub | Starboard"),
    "PortMissile":find_mesh_range("AIM-9 | Port"),"StarboardMissile":find_mesh_range("AIM-9 | Starboard")}
if any(value<0 for value in ids.values()) or any(start<0 or count<1 for start,count in dynamic_ranges.values()):
    raise RuntimeError("A required F-16 mesh/material name was not exported: "+repr(ids))
header=["#pragma once","// Generated by scripts/build_f16_netz.py from the adjacent GLB.",
        "namespace F16NetzMesh {"]
for name,value in ids.items():header.append("inline constexpr int %s = %d;"%(name,value))
for name,(start,count) in dynamic_ranges.items():
    header.append("inline constexpr int %sStart = %d;"%(name,start))
    header.append("inline constexpr int %sCount = %d;"%(name,count))
header.extend(["inline constexpr int MeshCount = %d;"%raylib_mesh_count,"}"])
with open(os.path.join(SRC,"f16_netz_mesh_ids.hpp"),"w",encoding="utf-8") as fp:
    fp.write("\n".join(header)+"\n")

print("F16_GLTF_MESH_COUNT",len(mesh_names))
print("F16_RAYLIB_PRIMITIVE_COUNT",raylib_mesh_count)
print("F16_GLTF_MATERIAL_COUNT",len(mat_names))
print("F16_GLTF_IDS",json.dumps(ids,sort_keys=True))
print("F16_GLTF_BYTES",os.path.getsize(glb))
print("F16_BLEND_BYTES",os.path.getsize(os.path.join(ASSETS,"f16_netz_source.blend")))
print("F16_PREVIEW",scene.render.filepath)

