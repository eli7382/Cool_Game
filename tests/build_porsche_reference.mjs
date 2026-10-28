// Generate an independent native pose reference with the existing Three rig.
// Usage: node tests/build_porsche_reference.mjs ../Porsche_911
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const source=path.resolve(process.argv[2]),root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const url=p=>pathToFileURL(path.join(source,p)).href,sha=b=>createHash('sha256').update(b).digest('hex');
const {loadRuntimeFixture}=await import(url('scripts/load_runtime_fixture.mjs'));
const adapter=await fs.readFile(path.join(source,'viewer/GT3RSCar.js'),'utf8');
const adapted=adapter.replace("import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';",'').replace("from 'three'",`from '${url('viewer/vendor/three.module.js')}'`).replace("from './SuspensionKinematics.js'",`from '${url('viewer/SuspensionKinematics.js')}'`);
const {GT3RSCar}=await import('data:text/javascript;base64,'+Buffer.from(adapted).toString('base64'));
const lines=['#pragma once','#include "raylib.h"','namespace bw::porsche_reference {','struct Node {const char* name;Matrix world;};'];
const n=v=>{const x=Number(v).toPrecision(10);return x.includes('.')||x.includes('e')?x+'f':x+'.0f';};
const matrix=m=>'{'+[0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15].map(i=>n(m.elements[i])).join(',')+'}';
const outputs=[];
for(const [label,suffix] of [['Hero','Development'],['Near','LOD1'],['Far','LOD2']]){
 const fixture=await loadRuntimeFixture(path.join(source,`exports/Porsche_911_GT3RS_${suffix}.glb`)),car=new GT3RSCar(fixture);
 car.update(0);fixture.scene.updateMatrixWorld(true);
 lines.push(`inline constexpr Node ${label}Rest[]={`);
 for(const node of fixture.nodes)lines.push('{'+JSON.stringify(node.name)+','+matrix(node.matrixWorld)+'},');
 lines.push('};');outputs.push({level:label,export_sha256:sha(fixture.buffer),nodes:fixture.nodes.length});
 if(label==='Hero')for(const [name,state] of [['DRS',{drs:1}],['Airbrake',{drs:1,airbrake:1}],['Open',{doorL:1,doorR:.6,windowL:.8,windowR:.2,brake:.7,throttle:.5,paddleL:.4,paddleR:.2}]]){
  Object.assign(car.state,car.target,state);car.setInstruments({rpm:name==='Open'?5000:0});car.setWheelAngles({FL:name==='Open'?.3:0,FR:name==='Open'?1:0,RL:name==='Open'?-.5:0,RR:name==='Open'?1.3:0});car.update(0);fixture.scene.updateMatrixWorld(true);lines.push(`inline constexpr Node ${name}[]={`);
  for(const node of fixture.nodes)if(name==='Open'||node.name.startsWith('WingActuator_')||node.name==='WingUpper_Hinge'||node.name.startsWith('FrontAero_'))lines.push('{'+JSON.stringify(node.name)+','+matrix(node.matrixWorld)+'},');
  lines.push('};');
 }
 if(label==='Hero')for(const [name,seconds] of [['ChronoShort',75.3],['ChronoWrap',1830.4],['ChronoLimit',5999.9]]){
  car.setChrono({elapsedSeconds:seconds,running:false});fixture.scene.updateMatrixWorld(true);lines.push(`inline constexpr Node ${name}[]={`);
  for(const node of fixture.nodes)if(node.name==='Chrono_Seconds'||node.name==='Chrono_Minutes')lines.push('{'+JSON.stringify(node.name)+','+matrix(node.matrixWorld)+'},');
  lines.push('};');
 }
 if(label==='Hero')for(const [name,yaw,pitch] of [['RearviewAdjusted',.12,-.08],['RearviewLimit',.18,.12]]){
  Object.assign(car.state,car.target,{mirrorYaw:yaw,mirrorPitch:pitch});car.update(0);fixture.scene.updateMatrixWorld(true);lines.push(`inline constexpr Node ${name}[]={`);
  for(const node of fixture.nodes){let inside=false;for(let p=node;p;p=p.parent)if(p.name==='Mirror_Adjust_Interior')inside=true;if(inside)lines.push('{'+JSON.stringify(node.name)+','+matrix(node.matrixWorld)+'},');}
  lines.push('};');
 }
 car.dispose();
}
lines.push('}');await fs.writeFile(path.join(root,'tests/porsche_reference.hpp'),lines.join('\n')+'\n');
await fs.writeFile(path.join(root,'docs/porsche-pose-reference.json'),JSON.stringify({adapter_sha256:sha(adapter),generator_sha256:sha(await fs.readFile(fileURLToPath(import.meta.url))),levels:outputs,scope:'Actual exported nodes under production Three adapter: zero, DRS, airbrake, independent doors/windows/pedals/paddles/needle/wheel-spin, three stopwatch hand and two interior-mirror joint fixtures'},null,2));
console.log(JSON.stringify({levels:outputs,adapter_sha256:sha(adapter)}));
