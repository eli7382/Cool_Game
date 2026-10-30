import fs from 'node:fs/promises';
import {createHash} from 'node:crypto';
import * as THREE from '../viewer/vendor/three.module.js';
import {loadRuntimeFixture} from './load_runtime_fixture.mjs';
const root=new URL('../',import.meta.url),file=process.argv[2]||new URL('exports/Porsche_911_GT3RS_Development.glb',root),fixture=await loadRuntimeFixture(file);
const packaging=process.argv.includes('--packaging');
const source=await fs.readFile(new URL('viewer/GT3RSCar.js',root),'utf8'),adapted=source.replace("import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';",'').replace("from 'three'",`from '${new URL('viewer/vendor/three.module.js',root).href}'`).replace("from './SuspensionKinematics.js'",`from '${new URL('viewer/SuspensionKinematics.js',root).href}'`);
const {GT3RSCar}=await import('data:text/javascript;base64,'+Buffer.from(adapted).toString('base64')),car=new GT3RSCar(fixture);
function lineage(o){const names=[];for(let n=o;n&&n!==fixture.scene;n=n.parent)names.push(n.name);return names;}
function role(o){const spin=lineage(o).some(n=>/^WheelSpin_F[LR]$/.test(n));return spin?(o.material.name==='Tire | culled satin micrograin'||o.material.name==='LOD Tire Front'?'tire':o.material.name.startsWith('Pyro red |')?'rim':'excluded_wheel_detail'):'mechanism';}
function geometry(o){return {name:o.name,material:o.material.name,packaging_role:role(o),ancestors:lineage(o),vertices:Array.from(o.geometry.attributes.position.array),triangles:o.geometry.index?Array.from(o.geometry.index.array):Array.from({length:o.geometry.attributes.position.count},(_,i)=>i),matrix:o.matrixWorld.toArray()};}
fixture.scene.updateMatrixWorld(true);
const selected=fixture.primitives.filter(o=>{for(let n=o;n&&n!==fixture.scene;n=n.parent)if(/(?:_| )F[LR](?:_| |$)|Axle_Front|SteeringRack_F/.test(n.name)||(packaging&&n.name==='Subframe_F'))return true;return false;}).filter(o=>packaging?role(o)!=='excluded_wheel_detail':!o.material.name.startsWith('Tire |'));
const moving=selected.map(o=>({...geometry(o),morphTargets:(o.geometry.morphAttributes.position||[]).map(a=>Array.from(a.array)),morphRelative:o.geometry.morphTargetsRelative})),poses=[];
for(const steering of [-.55,-.275,0,.275,.55])for(const left of [-.05,0,.05])for(const right of [-.05,0,.05])for(const phase of [0,Math.PI/3,2*Math.PI/3]){
 car.setSteering(steering);car.state.steering=steering;car.setSuspension({FL:left,FR:right,RL:0,RR:0});for(const k of ['FL','FR'])car.wheelAngles[k]=phase;car.update(0);fixture.scene.updateMatrixWorld(true);
 poses.push({steering,left,right,phase,matrices:selected.map(o=>o.matrixWorld.toArray()),weights:selected.map(o=>o.morphTargetInfluences||[])});
}
await fs.writeFile(process.argv[3]||new URL('reports/front-mechanism-clearance-fixture.json',root),JSON.stringify({export_sha256:createHash('sha256').update(fixture.buffer).digest('hex'),packaging,moving,poses}));console.log(JSON.stringify({moving_meshes:moving.length,poses:poses.length,morph_meshes:moving.filter(g=>g.morphTargets.length).length}));
