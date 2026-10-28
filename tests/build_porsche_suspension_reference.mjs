// Independent pose/vertex oracle: production Three linkage solver + actual
// converted glTF geometry. The native game does not require JavaScript.
import fs from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const source=path.resolve(process.argv[2]),root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const url=p=>pathToFileURL(path.join(source,p)).href,sha=b=>createHash('sha256').update(b).digest('hex');
const THREE=await import(url('viewer/vendor/three.module.js')),{loadRuntimeFixture}=await import(url('scripts/load_runtime_fixture.mjs'));
const adapter=await fs.readFile(path.join(source,'viewer/GT3RSCar.js'),'utf8'),solver=await fs.readFile(path.join(source,'viewer/SuspensionKinematics.js'),'utf8');
const adapted=adapter.replace("import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';",'').replace("from 'three'",`from '${url('viewer/vendor/three.module.js')}'`).replace("from './SuspensionKinematics.js'",`from '${url('viewer/SuspensionKinematics.js')}'`);
const {GT3RSCar}=await import('data:text/javascript;base64,'+Buffer.from(adapted).toString('base64'));
const keys=['FL','FR','RL','RR'],cases=[];
for(const travel of [-.05,0,.05])for(const steering of [-.55,0,.55])for(const rear of [-.08,0,.08])cases.push({travel:keys.map(()=>travel),steering,rear});
for(const d of [-1,1])for(const steering of [-.55,0,.55])cases.push({travel:[.05*d,-.05*d,-.05*d,.05*d],steering,rear:.08*d});
let seed=48151;const random=()=>((seed=Math.imul(1664525,seed)+1013904223>>>0)/4294967296);
for(let i=0;i<32;i++)cases.push({travel:keys.map(()=>(random()-.5)*.1),steering:(random()-.5)*1.1,rear:(random()-.5)*.16});
assert.equal(cases.length,65);
const prefixes=['Suspension_','Steer_','RearSteer_','WheelSpin_','LowerArm_','UpperArm_','TrailingArm_','UpperTrailingArm_','LowerBall_','UpperBall_','HubLowerBall_','HubUpperBall_','TieInner_','TieOuter_','TieRod_','DamperTop_','DamperBottom_','DamperBody_','DamperRod_','Spring_','ARB_Pivot_','ARB_Link_','ARB_End_','ARB_LinkBottom_','DriveInner_','DriveOuter_','DriveShaft_','TrailingOuter_','UpperTrailingOuter_'];
const blocks=[],levels=[],samples=32,nameBytes=64;
const uints=a=>{const b=Buffer.alloc(a.length*4);a.forEach((v,i)=>b.writeUInt32LE(v,i*4));return b;};
const floats=a=>{const b=Buffer.alloc(a.length*4);a.forEach((v,i)=>b.writeFloatLE(v,i*4));return b;};
let names;
for(const level of ['hero','near','far']){
 const fixture=await loadRuntimeFixture(path.join(root,`assets/porsche/gt3rs_${level}.glb`)),meta=JSON.parse(await fs.readFile(path.join(root,`assets/porsche/gt3rs_${level}_rig.json`),'utf8')),car=new GT3RSCar(fixture);
 car.update(0);fixture.scene.updateMatrixWorld(true);
 if(!names){names=prefixes.flatMap(prefix=>keys.map(k=>prefix+k)).filter(n=>car.nodes.has(n));blocks.push(Buffer.from('PSRF'),uints([1,3,cases.length,names.length,4,samples,nameBytes]));for(const name of names){assert.ok(Buffer.byteLength(name)<nameBytes);const b=Buffer.alloc(nameBytes);b.write(name);blocks.push(b);}}
 for(const name of names)assert.ok(car.nodes.has(name),level+' has '+name);
 const springs=meta.morphs.map(m=>{const node=meta.nodes[meta.bindings[m.mesh].node],parent=meta.nodes[node.parent].name,corner=keys.indexOf(parent.slice(7));assert.ok(corner>=0);const mesh=fixture.primitives[m.mesh];assert.ok(mesh.morphTargetDictionary.Compressed===0&&mesh.morphTargetDictionary.Extended===1);const indices=Array.from({length:samples},(_,i)=>Math.floor(i*(m.vertex_count-1)/(samples-1)));return{nativeMesh:m.mesh,corner,mesh,indices};}).sort((a,b)=>a.corner-b.corner);
 assert.equal(springs.length,4);
 for(const spring of springs){assert.equal(fixture.primitives.indexOf(spring.mesh),spring.nativeMesh);blocks.push(uints([spring.nativeMesh,spring.corner,samples]),uints(spring.indices));}
 for(const pose of cases){
  car.setSuspension(Object.fromEntries(keys.map((k,i)=>[k,pose.travel[i]])));car.setSteering(pose.steering,pose.rear);Object.assign(car.state,car.target);car.update(0);fixture.scene.updateMatrixWorld(true);
  for(const diagnostic of Object.values(car.getSuspensionState()))assert.ok(diagnostic.converged);
  blocks.push(floats([pose.steering,pose.rear,...pose.travel]));for(const name of names)blocks.push(floats(car.nodes.get(name).matrixWorld.elements));
  for(const spring of springs)blocks.push(floats(spring.mesh.morphTargetInfluences));
  const positions=[],normals=[];
  for(const spring of springs){const mesh=spring.mesh,a=mesh.geometry.attributes,m=mesh.geometry.morphAttributes,weights=mesh.morphTargetInfluences,nm=new THREE.Matrix3().getNormalMatrix(mesh.matrixWorld);
   for(const i of spring.indices){const p=new THREE.Vector3().fromBufferAttribute(a.position,i),n=new THREE.Vector3().fromBufferAttribute(a.normal,i);for(let t=0;t<2;t++){p.addScaledVector(new THREE.Vector3().fromBufferAttribute(m.position[t],i),weights[t]);n.addScaledVector(new THREE.Vector3().fromBufferAttribute(m.normal[t],i),weights[t]);}positions.push(...p.applyMatrix4(mesh.matrixWorld).toArray());normals.push(...n.applyNormalMatrix(nm).toArray());}
  }
  blocks.push(floats(positions),floats(normals));
 }
 levels.push({level,runtime_sha256:sha(fixture.buffer),poses:cases.length,spring_samples_per_pose:samples*4});car.dispose();
}
const binary=Buffer.concat(blocks);await fs.writeFile(path.join(root,'tests/porsche_suspension_reference.bin'),binary);
const report={fixture_sha256:sha(binary),generator_sha256:sha(await fs.readFile(fileURLToPath(import.meta.url))),adapter_sha256:sha(adapter),solver_sha256:sha(solver),levels,node_frames_each_pose:names.length,bytes:binary.length,scope:'65 travel/front/rear-steer poses per level; actual control frames, spring weights, sampled morphed positions and normals from the production Three rig'};
await fs.writeFile(path.join(root,'docs/porsche-suspension-reference.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));
