"""Blender: reproduce the delivered asset review from its recorded cameras/lights.

blender --background --threads 1 --python scripts/render_porsche_studio.py -- front
"""
import bpy,json,hashlib,sys
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
configuration=json.loads((root/'docs/porsche-studio/authoring-render-report.json').read_text())
source=root/'assets/porsche/portable/exports/Porsche_911_GT3RS_Development.glb'
with source.open('rb') as stream:assert hashlib.file_digest(stream,'sha256').hexdigest()==configuration['asset_sha256']
requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
requested=requested or [v['view'] for v in configuration['views']]
assert requested and set(requested).issubset({v['view'] for v in configuration['views']})
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(source))
scene=bpy.context.scene;meshes=[o for o in scene.objects if o.type=='MESH']
for obj in meshes:obj.data.calc_loop_triangles()
assert sum(len(o.data.loop_triangles) for o in meshes)==configuration['triangles']
scene.render.engine='CYCLES';scene.cycles.samples=configuration['samples'];scene.cycles.use_denoising=True;scene.cycles.max_bounces=10;scene.cycles.diffuse_bounces=4;scene.cycles.glossy_bounces=6;scene.cycles.transmission_bounces=8;scene.cycles.transparent_max_bounces=12
try:
 preferences=bpy.context.preferences.addons['cycles'].preferences;preferences.compute_device_type='OPTIX';preferences.get_devices()
 if any(d.type=='OPTIX' for d in preferences.devices):
  for device in preferences.devices:device.use=device.type=='OPTIX'
  scene.cycles.device='GPU'
except Exception as error:print('CPU_RENDER_FALLBACK',str(error),flush=True)
scene.render.resolution_x,scene.render.resolution_y=configuration['resolution'];scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGB';scene.render.image_settings.color_depth='8';scene.render.film_transparent=False
scene.view_settings.view_transform=configuration['view_transform'];scene.view_settings.look=configuration['look'];scene.view_settings.exposure=0;scene.view_settings.gamma=1
world=bpy.data.worlds.new('Neutral studio world');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.04,.04,.04,1);world.node_tree.nodes['Background'].inputs[1].default_value=.25;scene.world=world
lights={}
for spec in configuration['lighting']:
 data=bpy.data.lights.new(spec['name'],'AREA');data.shape='RECTANGLE';data.size,data.size_y=spec['size_m'];data.energy=spec['watts'];data.color=(1,1,1)
 obj=bpy.data.objects.new(spec['name'],data);scene.collection.objects.link(obj);lights[spec['name']]=(obj,spec)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.005));floor=bpy.context.object;floor.name='Review floor'
material=bpy.data.materials.new('Neutral floor');material.use_nodes=True;p=material.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(.035,.04,.045,1);p.inputs['Roughness'].default_value=.4;floor.data.materials.append(material)
data=bpy.data.cameras.new('Studio review camera');camera=bpy.data.objects.new('Studio review camera',data);scene.collection.objects.link(camera);scene.camera=camera
out=root/'screenshots/porsche-studio';out.mkdir(parents=True,exist_ok=True)
for view in configuration['views']:
 if view['view'] not in requested:continue
 camera.location=view['camera_location'];camera.rotation_euler=(Vector(view['target'])-camera.location).to_track_quat('-Z','Y').to_euler();data.lens=view['lens_mm']
 for actual in view['lights']:
  obj,spec=lights[actual['name']];obj.location=actual['location'];obj.rotation_euler=(Vector(spec['target'])-obj.location).to_track_quat('-Z','Y').to_euler();obj.data.energy=actual['watts']
 scene.render.filepath=str(out/(view['view']+'.png'));print('STUDIO_REPRODUCE '+view['view'],flush=True);bpy.ops.render.render(write_still=True)
