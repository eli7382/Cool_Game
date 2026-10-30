// Texture-free glTF fixture: load the actual exported geometry, morphs and nodes
// for mechanism verification without a DOM, GPU, or synthetic anchor positions.
import fs from 'node:fs/promises';
import * as THREE from '../viewer/vendor/three.module.js';
export async function loadRuntimeFixture(file){
 const buffer=await fs.readFile(file),length=buffer.readUInt32LE(12),g=JSON.parse(buffer.subarray(20,20+length).toString()),base=28+length;
 const sizes={SCALAR:1,VEC2:2,VEC3:3,VEC4:4},types={5121:[Uint8Array,1,'readUInt8'],5123:[Uint16Array,2,'readUInt16LE'],5125:[Uint32Array,4,'readUInt32LE'],5126:[Float32Array,4,'readFloatLE']};
 function accessor(id){const a=g.accessors[id],view=g.bufferViews[a.bufferView],[Type,bytes,read]=types[a.componentType],size=sizes[a.type],result=new Type(a.count*size),stride=view.byteStride||size*bytes,start=base+(view.byteOffset||0)+(a.byteOffset||0);for(let i=0;i<a.count;i++)for(let j=0;j<size;j++)result[i*size+j]=buffer[read](start+i*stride+j*bytes);return new THREE.BufferAttribute(result,size,Boolean(a.normalized));}
 const materials=(g.materials||[]).map(m=>{const material=new THREE.MeshStandardMaterial();material.name=m.name||'';material.userData={...m.extras};material.side=m.doubleSided?THREE.DoubleSide:THREE.FrontSide;if(m.emissiveFactor)material.emissive.fromArray(m.emissiveFactor);material.emissiveIntensity=m.extensions?.KHR_materials_emissive_strength?.emissiveStrength??1;return material;});
 const primitives=[];
 function primitive(p,m){
  const geometry=new THREE.BufferGeometry();for(const [semantic,name] of [['POSITION','position'],['NORMAL','normal'],['TEXCOORD_0','uv']])if(p.attributes[semantic]!==undefined)geometry.setAttribute(name,accessor(p.attributes[semantic]));
  if(p.indices!==undefined)geometry.setIndex(accessor(p.indices));
  if(p.targets){for(const [semantic,name] of [['POSITION','position'],['NORMAL','normal']])if(p.targets.every(t=>t[semantic]!==undefined))geometry.morphAttributes[name]=p.targets.map(t=>accessor(t[semantic]));geometry.morphTargetsRelative=true;}
  const mesh=new THREE.Mesh(geometry,materials[p.material]||new THREE.MeshStandardMaterial());
  if(p.targets){mesh.morphTargetDictionary=Object.fromEntries((m.extras?.targetNames||p.targets.map((_,i)=>String(i))).map((name,i)=>[name,i]));mesh.morphTargetInfluences=p.targets.map((_,i)=>m.weights?.[i]||0);}
  primitives.push(mesh);return mesh;
 }
 const nodes=g.nodes.map(n=>{let o;if(n.mesh!==undefined){const m=g.meshes[n.mesh];if(m.primitives.length===1)o=primitive(m.primitives[0],m);else{o=new THREE.Group();for(const p of m.primitives)o.add(primitive(p,m));}}else o=new THREE.Object3D();o.name=n.name||'';o.userData={...n.extras};if(n.matrix)o.applyMatrix4(new THREE.Matrix4().fromArray(n.matrix));else{if(n.translation)o.position.fromArray(n.translation);if(n.rotation)o.quaternion.fromArray(n.rotation);if(n.scale)o.scale.fromArray(n.scale);}return o;});
 g.nodes.forEach((n,i)=>(n.children||[]).forEach(c=>nodes[i].add(nodes[c])));const scene=new THREE.Group();for(const i of g.scenes[g.scene||0].nodes)scene.add(nodes[i]);scene.updateMatrixWorld(true);
 return {scene,nodes,primitives,g,buffer};
}
