"""Review the actual current portable asset under reproducible neutral lighting."""
import bpy,json,hashlib,sys,time,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1];out=root/'renders/current-studio';out.mkdir(parents=True,exist_ok=True)
source=root/'exports/Porsche_911_GT3RS_Development.glb'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as stream:
  for chunk in iter(lambda:stream.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
source_sha=sha(source);manifest=json.loads((root/'exports/gt3rs-lods.json').read_text());assert source_sha==manifest['hero_sha256']
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [];requested=args or ['front','rear','side','wheel']
assert all(v in ['front','rear','side','wheel'] for v in requested)
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(source))
scene=bpy.context.scene;car_meshes=[o for o in scene.objects if o.type=='MESH'];triangles=sum(len(o.data.loop_triangles) for o in car_meshes)
for o in car_meshes:o.data.calc_loop_triangles()
triangles=sum(len(o.data.loop_triangles) for o in car_meshes);assert triangles==695220,triangles
scene.render.engine='CYCLES';scene.cycles.samples=64;scene.cycles.use_denoising=True
scene.cycles.max_bounces=10;scene.cycles.diffuse_bounces=4;scene.cycles.glossy_bounces=6;scene.cycles.transmission_bounces=8;scene.cycles.transparent_max_bounces=12
device='CPU';preferences=bpy.context.preferences.addons['cycles'].preferences
try:
 preferences.compute_device_type='OPTIX';preferences.get_devices()
 usable=[d for d in preferences.devices if d.type=='OPTIX']
 if usable:
  for d in preferences.devices:d.use=d.type=='OPTIX'
  scene.cycles.device='GPU';device='OPTIX: '+', '.join(d.name for d in usable)
except Exception as error:print('GPU_SELECTION_FALLBACK',str(error),flush=True)
scene.render.resolution_x=1920;scene.render.resolution_y=1080;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGB';scene.render.image_settings.color_depth='8'
scene.render.film_transparent=False;scene.view_settings.view_transform='AgX'
try:scene.view_settings.look='AgX - Medium High Contrast'
except TypeError:pass
scene.view_settings.exposure=0;scene.view_settings.gamma=1
world=bpy.data.worlds.new('Neutral studio world');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.04,.04,.04,1);world.node_tree.nodes['Background'].inputs[1].default_value=.25;scene.world=world
lighting=[]
light_objects=[]
def light(name,location,energy,size,size_y,target=(0,0,.65)):
 data=bpy.data.lights.new(name,'AREA');data.energy=energy;data.shape='RECTANGLE';data.size=size;data.size_y=size_y;data.color=(1,1,1)
 obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=location;obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
 lighting.append({'name':name,'location':list(location),'watts':energy,'size_m':[size,size_y],'target':list(target)})
 light_objects.append(obj)
light('Overhead softbox',(2.5,-3.6,5.5),1700,5.5,2.5)
light('Side softbox',(-4.5,-.3,2.8),450,4,1.8)
light('Rear strip',(-.5,4.5,4.8),250,3,.3)
light('Front fill',(-.5,-5,4),250,.5,2)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.005));floor=bpy.context.object;floor.name='Review floor'
material=bpy.data.materials.new('Neutral floor');material.use_nodes=True;principled=material.node_tree.nodes.get('Principled BSDF');principled.inputs['Base Color'].default_value=(.035,.04,.045,1);principled.inputs['Roughness'].default_value=.4;floor.data.materials.append(material)
data=bpy.data.cameras.new('Studio review camera');camera=bpy.data.objects.new('Studio review camera',data);scene.collection.objects.link(camera);scene.camera=camera
views={'front':((5.4,-7.5,2.75),(0,0,.65),65),'rear':((-5.4,7,2.7),(0,0,.65),65),'side':((10.4,0,2.1),(0,0,.65),70),'wheel':((2.2,-2.55,.68),(.82,-1.2,.45),75)}
report={'asset_sha256':source_sha,'asset':str(source.relative_to(root)),'script_sha256':sha(Path(__file__)),'triangles':triangles,'car_mesh_objects':len(car_meshes),'render_engine':'Cycles','device':device,'samples':64,'resolution':[1920,1080],'lighting':lighting,'view_transform':scene.view_settings.view_transform,'look':scene.view_settings.look,'views':[],'scope':'Studio renders of the actual portable GLB geometry/materials. This is a Blender material/shape review, not a native game screenshot or a frame-rate result.'}
previous_path=root/'reports/current-studio-review.json'
if previous_path.exists():
 previous=json.loads(previous_path.read_text())
 if previous.get('asset_sha256')==source_sha and previous.get('script_sha256')==report['script_sha256']:report['views']=previous['views']
packet={'packet_id':'PKT-gt3rs-studio-neutral-v2','from':'photography-director','to':'visual-qa','job':'Review the actual current GT3 RS asset under neutral commercial studio lighting.','decisions':[{'key':'craft_state.photography','value':{'camera':'Moderately compressed automotive perspective; 65 mm front/rear, 70 mm side, 75 mm wheel detail.','focus':'Depth of field disabled so relevant geometry and finish remain readable.','lighting':'Camera-relative neutral D65-white area lights; overhead key, approximately 2:1 key/fill emitter-radiance ratio, narrow lower-power rim and narrow front fill.','color':'White paint and red wheel/graphics colors remain unchanged; no product color grading.','materials':'Preserve the actual GLB factors, maps, geometry and rig. Adjust light-card size/direction to inspect carbon and glazing without broad white reflections.','composition':'Read silhouette and white/red design first, then aero and wheel detail. Large neutral floor avoids visible plane edges.'},'rationale':'The first lighting setup produced broad specular reflections that hid dark carbon and headlamp detail.'}],'evidence':['studies/studio-lighting-v1/front.png','studies/studio-lighting-v1/rear.png','reports/current-studio-review.json'],'confidence':.85,'hard_vetoes':[],'soft_warnings':['Studio appearance is not a native-game rendering or performance proof.'],'unresolved':['Inspect all four updated views for washed-out dark trim, glare, geometry fit and readable finish.'],'recommended_next':['Render the front view, inspect it, then complete the matching rear, side and wheel views.']}
(root/'reports/studio-photography-direction.json').write_text(json.dumps(packet,indent=2)+'\n')
print(json.dumps({'STUDIO_REVIEW_START':True,'device':device,'triangles':triangles,'views':requested}),flush=True)
for name in requested:
 location,target,lens=views[name];camera.location=location;camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();data.lens=lens
 angle=math.atan2(location[1],location[0])-math.atan2(-7.5,5.4);cosine,sine=math.cos(angle),math.sin(angle)
 actual_lights=[]
 for obj,spec in zip(light_objects,lighting):
  x,y,z=spec['location'];obj.location=(cosine*x-sine*y,sine*x+cosine*y,z);obj.rotation_euler=(Vector(spec['target'])-obj.location).to_track_quat('-Z','Y').to_euler();actual_lights.append({'name':obj.name,'location':list(obj.location),'watts':obj.data.energy})
 path=out/f'{name}.png';scene.render.filepath=str(path);started=time.time();print('STUDIO_VIEW_START '+name,flush=True);bpy.ops.render.render(write_still=True)
 assert sha(source)==source_sha
 report['views']=[v for v in report['views'] if v['view']!=name]
 report['views'].append({'view':name,'file':str(path.relative_to(root)),'sha256':sha(path),'camera_location':list(location),'target':list(target),'lens_mm':lens,'lights':actual_lights,'seconds':time.time()-started})
 (root/'reports/current-studio-review.json').write_text(json.dumps(report,indent=2)+'\n');print('STUDIO_VIEW_COMPLETE '+name,flush=True)
print('STUDIO_REVIEW_COMPLETE',flush=True)
