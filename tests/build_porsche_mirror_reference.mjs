// Independent projection fixture from the production Three mirror implementation.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),source=path.resolve(process.argv[2]);
const url=p=>pathToFileURL(path.join(source,p)).href,sha=b=>createHash('sha256').update(b).digest('hex');
const THREE=await import(url('viewer/vendor/three.module.js')),{loadRuntimeFixture}=await import(url('scripts/load_runtime_fixture.mjs'));
const adapter=await fs.readFile(path.join(source,'viewer/GT3RSCar.js'),'utf8'),projectionSource=await fs.readFile(path.join(source,'viewer/MirrorProjection.js'),'utf8');
const {GT3RSCar}=await import('data:text/javascript;base64,'+Buffer.from(adapter.replace("import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';",'').replace("from 'three'",`from '${url('viewer/vendor/three.module.js')}'`).replace("from './SuspensionKinematics.js'",`from '${url('viewer/SuspensionKinematics.js')}'`)).toString('base64'));
const {MirrorProjection}=await import('data:text/javascript;base64,'+Buffer.from(projectionSource.replace("from 'three'",`from '${url('viewer/vendor/three.module.js')}'`)).toString('base64'));
const chunks=[];function uint(...v){const b=Buffer.alloc(v.length*4);v.forEach((x,i)=>b.writeUInt32LE(x,i*4));chunks.push(b);}function floats(...v){const b=Buffer.alloc(v.length*4);v.forEach((x,i)=>{assert.ok(Number.isFinite(x));b.writeFloatLE(x,i*4);});chunks.push(b);}const vector=v=>floats(v.x,v.y,v.z),matrix=m=>floats(...m.elements);
chunks.push(Buffer.from('MRRF'));uint(1,54);const cases=[],exports=[];
for(const [level,file] of ['Development','LOD1','LOD2'].entries()){
 const fixture=await loadRuntimeFixture(path.join(source,`exports/Porsche_911_GT3RS_${file}.glb`)),car=new GT3RSCar(fixture),nodes=car.nodes;
 exports.push({level,file:`Porsche_911_GT3RS_${file}.glb`,export_sha256:sha(fixture.buffer)});
 for(let pose=0;pose<6;pose++){
  fixture.scene.position.set(pose?7:0,pose?3:0,pose?-5:0);fixture.scene.rotation.set(pose?.12:0,pose?.7:0,pose?-.08:0);fixture.scene.scale.set(pose===2?1.35:1,pose===2?1.1:1,pose===2?.8:1);
  Object.assign(car.state,car.target,{doorL:pose===3?.65:0,doorR:pose===3?.7:0,mirrorYaw:pose===4?.12:0,mirrorPitch:pose===4?-.08:0});car.update(0);fixture.scene.updateMatrixWorld(true);
  for(const [kind,id] of ['L','R','Interior'].entries()){
   const frame=nodes.get('Mirror_Frame_'+id),meshes=[];frame.traverse(o=>{if(o.isMesh&&(Array.isArray(o.material)?o.material:[o.material]).some(m=>m.userData.mirror_role===id))meshes.push(o);});assert.equal(meshes.length,1);
   const camera=new THREE.PerspectiveCamera(pose===5?100:72,16/9,.01,1000);nodes.get('Driver_Eye').getWorldPosition(camera.position);camera.up.set(0,1,0).transformDirection(fixture.scene.matrixWorld);const target=frame.getWorldPosition(new THREE.Vector3());if(pose===5)target.add(new THREE.Vector3(.1,.04,0));camera.lookAt(target);camera.updateMatrixWorld(true);
   const projection=new MirrorProjection();assert.ok(projection.update(frame,meshes,camera),`${level}/${pose}/${id} valid`);const vertices=[];for(const mesh of meshes){const p=mesh.geometry.attributes.position;for(let i=0;i<p.count;i++)vertices.push(new THREE.Vector3().fromBufferAttribute(p,i).applyMatrix4(mesh.matrixWorld));}
   uint(level,kind,vertices.length);matrix(frame.matrixWorld);vector(camera.position);vector(camera.getWorldDirection(new THREE.Vector3()).add(camera.position));vector(camera.up);floats(camera.fov,camera.aspect,camera.near,camera.far);for(const p of vertices)vector(p);
   vector(projection.camera.position);vector(projection.point);vector(projection.normal);floats(projection.eyeDistance);matrix(projection.textureMatrix);matrix(projection.camera.projectionMatrix);
   cases.push({level,pose,id,vertices:vertices.length});
  }
 }
 car.dispose();
}
const binary=Buffer.concat(chunks);await fs.writeFile(path.join(root,'tests/porsche_mirror_reference.bin'),binary);
await fs.writeFile(path.join(root,'docs/porsche-mirror-reference.json'),JSON.stringify({generator_sha256:sha(await fs.readFile(fileURLToPath(import.meta.url))),adapter_sha256:sha(adapter),projection_sha256:sha(projectionSource),binary_sha256:sha(binary),exports,cases,scope:'Actual source optical vertices and production reflected/cropped/oblique projection across all three detail levels, root movement/nonuniform scale, doors, interior adjustment and camera pan'},null,2)+'\n');console.log(JSON.stringify({cases:cases.length,bytes:binary.length,sha256:sha(binary)}));
