import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {SuspensionKinematics} from './SuspensionKinematics.js';

/** Metres, +Y up, +Z forward. Visual controls; vehicle physics drives this adapter.
 * Development motion envelopes remain provisional until full reference/clearance review.
 */
export class GT3RSCar {
  static async load(url,onProgress) {
    const gltf=await new GLTFLoader().loadAsync(url,onProgress);
    return new GT3RSCar(gltf);
  }
  constructor(gltf,{coolingMaterialFactory}={}) {
    this.root=gltf.scene; this.nodes=new Map(); this.rest=new Map(); this.disposed=false;
    this.state={steering:0,rearSteering:0,doorL:0,doorR:0,windowL:0,windowR:0,drs:0,airbrake:0,brake:0,throttle:0,paddleL:0,paddleR:0,mirrorYaw:0,mirrorPitch:0};
    this.target={...this.state}; this.wheelAngles={FL:0,FR:0,RL:0,RR:0};this.travel={FL:0,FR:0,RL:0,RR:0};
    this.root.traverse(o=>{
      this.nodes.set(o.name,o);
      this.rest.set(o.uuid,{position:o.position.clone(),quaternion:o.quaternion.clone()});
      if(o.isMesh){const ms=Array.isArray(o.material)?o.material:[o.material],fineScreen=ms.some(m=>m.name==='Cooling | satin protective grille wire');
        // The submillimetre wires are smaller than a shadow-map texel. Their
        // mapped shadows produce moire; the radiator backing supplies occlusion.
        o.castShadow=!fineScreen&&!ms.some(m=>m.transmission>0||m.transparent||['GT3 RS white lettering','Red GT3 RS graphics'].includes(m.name));o.receiveShadow=!fineScreen;
        for(const m of ms)for(const t of Object.values(m))if(t?.isTexture)t.anisotropy=8;}
    });
    this.coolingDetail=new FrontCoolingDetail(this.root,coolingMaterialFactory);
    this.lights={headlights:true,brake:false};this.lightMaterials=new Set();this.headlightBeams=[];
    this.root.traverse(o=>{if(o.isMesh)for(const m of(Array.isArray(o.material)?o.material:[o.material]))if(['Headlight LED','Headlight DRL','Headlight DRL diffuser','Headlight projector optic','Rear LED light bar'].includes(m.name))this.lightMaterials.add(m);});
    // Runtime beam lights use portable anchors from the GLB and follow the vehicle.
    for(const side of ['L','R']){
      const anchor=this.nodes.get('Headlight_Aim_'+side);if(!anchor)continue;
      const beam=new THREE.SpotLight(0xe7f2ff,1500,45,THREE.MathUtils.degToRad(13),.65,2);
      beam.name='Headlight_Beam_'+side;beam.position.set(0,0,0);beam.target.position.set(0,-.65,22);anchor.add(beam,beam.target);this.headlightBeams.push(beam);
    }
    this.tachometerDesign=this.nodes.get('Tachometer_Face')?.userData.gauge_scale_max_rpm===10000;
    this.instrumentDisplays=[];this.instrumentElapsed=0;this.instrumentTime=0;
    this.chrono={elapsedSeconds:0,running:false};
    this.createInstrumentDisplays();this.setInstruments();this.setChrono();this.refreshInstrumentDisplays();
    this.applyLights();
    this.suspensionRig=new SuspensionKinematics(this,THREE);
  }
  clamp(v,min,max){v=Number(v);return Number.isFinite(v)?THREE.MathUtils.clamp(v,min,max):0;}
  setSteering(frontRadians,rearRadians=0){this.target.steering=this.clamp(frontRadians,-.55,.55);this.target.rearSteering=this.clamp(rearRadians,-.08,.08);}
  setWheelAngles(values){for(const key of ['FL','FR','RL','RR'])if(values[key]!==undefined)this.wheelAngles[key]=Number.isFinite(Number(values[key]))?Number(values[key]):0;}
  setSuspension(values){for(const key of ['FL','FR','RL','RR'])if(values[key]!==undefined)this.travel[key]=this.clamp(values[key],-.05,.05);}
  setDoor(side,openness){this.target[side==='L'?'doorL':'doorR']=this.clamp(openness,0,1);}
  setWindow(side,openness){this.target[side==='L'?'windowL':'windowR']=this.clamp(openness,0,1);}
  setAero({drs=0,airbrake=0}={}){this.target.drs=this.clamp(drs,0,1);this.target.airbrake=this.clamp(airbrake,0,1);}
  setPedals({brake=0,throttle=0}={}){this.target.brake=this.clamp(brake,0,1);this.target.throttle=this.clamp(throttle,0,1);}
  setPaddles({left=0,right=0}={}){this.target.paddleL=this.clamp(left,0,1);this.target.paddleR=this.clamp(right,0,1);}
  setRearviewAdjustment({yaw=0,pitch=0}={}){this.target.mirrorYaw=this.clamp(yaw,-.18,.18);this.target.mirrorPitch=this.clamp(pitch,-.12,.12);}
  /** Portable stopwatch. Game simulation time drives update(dt); paused time is retained. */
  setChrono(patch={}){
    const values={...this.chrono,...patch};
    this.chrono={elapsedSeconds:this.clamp(values.elapsedSeconds,0,5999.9),running:Boolean(values.running)};
    if(this.chrono.elapsedSeconds>=5999.9)this.chrono.running=false;
    this.applyChronoHands();
  }
  applyChronoHands(){
    const seconds=this.chrono.elapsedSeconds,axis=new THREE.Vector3(0,0,1);
    this.rotate('Chrono_Seconds',axis,(seconds%60)/60*Math.PI*2);
    this.rotate('Chrono_Minutes',axis,(seconds%1800)/1800*Math.PI*2);
  }
  rotate(name,axis,angle){const o=this.nodes.get(name);if(!o)return;o.quaternion.copy(this.rest.get(o.uuid).quaternion).multiply(new THREE.Quaternion().setFromAxisAngle(axis,angle));}
  setInstruments(patch={}){
    const maxRpm=Number(this.nodes.get('Tachometer_Needle')?.userData.rpm_max)||9000;
    const values={rpm:0,speedMps:0,gear:'N',transmissionMode:'auto',clusterPage:this.tachometerDesign?'chassis':'telemetry',dampingCompression:[2,4],dampingRebound:[2,4],ambientTempC:24.5,oilTempC:95,coolantTempC:90,oilPressureBar:4,fuelFraction:.75,tyrePressuresBar:[2,2,2.2,2.2],...this.instruments,...patch};
    this.instruments={rpm:this.clamp(values.rpm,0,maxRpm),speedMps:this.clamp(values.speedMps,-130,130),gear:/^(N|R|P|D|[1-7])$/.test(String(values.gear).toUpperCase())?String(values.gear).toUpperCase():'N',transmissionMode:values.transmissionMode==='manual'?'manual':'auto',oilTempC:this.clamp(values.oilTempC,0,170),coolantTempC:this.clamp(values.coolantTempC,0,140),oilPressureBar:this.clamp(values.oilPressureBar,0,10),fuelFraction:this.clamp(values.fuelFraction,0,1),tyrePressuresBar:[0,1,2,3].map(i=>this.clamp(values.tyrePressuresBar?.[i],0,5))};
    Object.assign(this.instruments,{clusterPage:values.clusterPage==='telemetry'?'telemetry':'chassis',dampingCompression:[0,1].map(i=>Math.round(this.clamp(values.dampingCompression?.[i],-4,4))),dampingRebound:[0,1].map(i=>Math.round(this.clamp(values.dampingRebound?.[i],-4,4))),ambientTempC:this.clamp(values.ambientTempC,-50,70)});
    const sweep=Number(this.nodes.get('Tachometer_Needle')?.userData.rpm_sweep_radians)||Math.PI*1.5;
    this.rotate('Tachometer_Needle',new THREE.Vector3(0,0,1),this.instruments.rpm/maxRpm*sweep);
  }
  createInstrumentDisplays(){
    // The GLB includes static rest textures. Canvas updates are optional and
    // remain attached to the modeled screen surfaces in any vehicle pose.
    if(typeof document==='undefined')return;
    const sizes={'GT3 RS live instrument cluster':[2048,640],'GT3 RS live tachometer display':this.tachometerDesign?[768,320]:[512,300],'GT3 RS live PCM display':[1536,512],'GT3 RS live chrono display':[768,180]},seen=new Set();
    this.root.traverse(o=>{if(!o.isMesh)return;for(const material of(Array.isArray(o.material)?o.material:[o.material])){
      if(!sizes[material.name]||seen.has(material))continue;seen.add(material);
      const canvas=document.createElement('canvas');[canvas.width,canvas.height]=sizes[material.name];const context=canvas.getContext('2d');if(!context)continue;
      const texture=new THREE.CanvasTexture(canvas);texture.flipY=false;texture.colorSpace=THREE.SRGBColorSpace;texture.anisotropy=4;
      const oldTextures=new Set([material.map,material.emissiveMap].filter(Boolean));for(const t of oldTextures)t.dispose();
      material.map=texture;material.emissiveMap=texture;material.color.set(0xffffff);material.emissive.set(0xffffff);material.emissiveIntensity=.65;material.metalness=0;material.roughness=.34;material.needsUpdate=true;
      this.instrumentDisplays.push({name:material.name,canvas,context,texture});
    }});
  }
  refreshInstrumentDisplays(){
    const v=this.instruments;if(!v)return;
    const speed=Math.round(Math.abs(v.speedMps)*3.6),rpm=Math.round(v.rpm),oil=Math.round(v.oilTempC),water=Math.round(v.coolantTempC),fuel=Math.round(v.fuelFraction*100),press=v.oilPressureBar.toFixed(1),tyres=v.tyrePressuresBar.map(p=>p.toFixed(1));
    const shift=Math.max(0,Math.min(8,Math.ceil((rpm-4000)/625))),flash=rpm>=8600&&Math.floor(this.instrumentTime*6)%2===0;
    const chronoTenths=Math.floor((this.chrono?.elapsedSeconds||0)*10+1e-8),chronoText=String(Math.floor(chronoTenths/600)).padStart(2,'0')+':'+String(Math.floor(chronoTenths/10)%60).padStart(2,'0')+'.'+chronoTenths%10;
    const key=JSON.stringify([speed,rpm,v.gear,v.transmissionMode,v.clusterPage,v.dampingCompression,v.dampingRebound,v.ambientTempC,oil,water,fuel,press,tyres,shift,flash]);
    const instrumentsChanged=key!==this.instrumentDisplayKey,chronoChanged=chronoTenths!==this.chronoDisplayKey;if(!instrumentsChanged&&!chronoChanged)return;this.instrumentDisplayKey=key;this.chronoDisplayKey=chronoTenths;
    for(const display of this.instrumentDisplays){
      if(display.name==='GT3 RS live chrono display'?!chronoChanged:!instrumentsChanged)continue;
      const c=display.context,w=display.canvas.width,h=display.canvas.height;c.fillStyle='#050607';c.fillRect(0,0,w,h);
      const text=(x,y,value,size=36,color='#e9edf2',bold=false,align='left')=>{c.fillStyle=color;c.font=(bold?'700 ':'400 ')+size+'px Arial, sans-serif';c.textAlign=align;c.textBaseline='top';c.fillText(String(value),x,y);};
      if(display.name==='GT3 RS live instrument cluster'){
        if(v.clusterPage==='chassis'){
          text(110,35,'Compression',46,'#e9edf2',true);text(1400,35,'Rebound',46,'#e9edf2',true);
          const dial=(x,y,value,accent)=>{for(let i=0;i<9;i++){c.beginPath();c.ellipse(x,y,100,113,0,(135+i*30+3)*Math.PI/180,(135+i*30+27)*Math.PI/180);c.strokeStyle=i===value+4?'#e4edf4':'#88b6dd';c.lineWidth=11;c.stroke();}if(accent){c.beginPath();c.ellipse(x,y,109,123,0,135*Math.PI/180,405*Math.PI/180);c.strokeStyle=accent;c.lineWidth=4;c.stroke();}text(x,y-28,(value>=0?'+':'')+value,62,'#e9edf2',true,'center');text(x-45,y+73,'−',34,'#e9edf2',true,'center');text(x+45,y+73,'+',34,'#e9edf2',true,'center');};
          for(const [x,y,value,label,lx,accent] of [[400,220,v.dampingCompression[0],'Front',140,'#dc645a'],[460,455,v.dampingCompression[1],'Rear',200,null],[1580,220,v.dampingRebound[0],'Front',1750,null],[1640,455,v.dampingRebound[1],'Rear',1810,'#d9c464']]){dial(x,y,value,accent);text(lx,y-24,label,42,'#e9edf2',true);}
          text(110,577,'F  '+tyres[0]+' / '+tyres[1]+' bar',30);text(110,610,'R  '+tyres[2]+' / '+tyres[3]+' bar',30);text(1490,577,v.ambientTempC.toFixed(1)+' °C',32,'#e9edf2',true);text(1490,610,'TRACK',30,'#e9edf2',true);
          display.texture.needsUpdate=true;continue;
        }
        for(const x of [30,1280]){text(x,85,'TRACK',44,'#f0f1f3',true);c.fillStyle='#4c545e';c.fillRect(x,147,690,2);}
        for(const [x,y,label,value] of [[30,220,'WATER',water+' °C'],[30,350,'TYRES  F',tyres[0]+' / '+tyres[1]+' bar'],[30,475,'TYRES  R',tyres[2]+' / '+tyres[3]+' bar'],[1280,220,'OIL TEMP',oil+' °C'],[1280,350,'OIL PRESS',press+' bar'],[1280,475,'FUEL',fuel+' %']]){text(x,y,label,32,'#9099a4');text(x+690,y,value,42,'#e9edf2',true,'right');}
      }else if(display.name==='GT3 RS live tachometer display'){
        if(!this.tachometerDesign){text(196,18,speed,152,'#e9edf2',true,'center');text(196,152,'km/h',40,'#e9edf2',false,'center');text(480,130,v.gear,46,'#e9edf2',true,'center');display.texture.needsUpdate=true;continue;}
        text(300,20,speed,172,'#e9edf2',true,'center');text(300,215,'km/h',46,'#e9edf2',false,'center');
        if(/^[1-7]$/.test(v.gear)){text(690,45,v.gear,118,'#e9edf2',true,'center');text(690,200,v.transmissionMode==='manual'?'M':'D',45,'#929ba7',false,'center');}
        else for(const [i,gear] of ['P','R','N','D'].entries())text(690,28+65*i,gear,48,gear===v.gear?'#ef544b':'#7d8792',true,'center');
      }else if(display.name==='GT3 RS live chrono display'){
        text(w/2,24,chronoText,118,'#d4dfd8',false,'center');
      }else{
        text(50,30,'GT3 RS',58,'#e9edf2',true);text(1480,45,'TRACK',35,'#ff4449',false,'right');c.fillStyle='#6e242c';c.fillRect(50,112,1430,3);
        text(50,150,speed,135,'#e9edf2',true);text(55,320,'km/h',36,'#929ba7');text(530,170,v.gear,110,'#e9edf2',true);
        for(const [x,label,value] of [[810,'OIL',oil+' °C'],[1150,'WATER',water+' °C']]){text(x,175,label,32,'#929ba7');text(x,230,value,54,'#e9edf2',true);}
        text(50,415,rpm+' rpm',38);c.fillStyle='#262c33';c.fillRect(290,420,1190,25);c.fillStyle=flash?'#43aaff':'#e0303a';c.fillRect(290,420,1190*rpm/9000,25);
      }
      display.texture.needsUpdate=true;
    }
  }
  setLights(patch={}){
    for(const key of ['headlights','brake'])if(patch[key]!==undefined)this.lights[key]=Boolean(patch[key]);this.applyLights();
  }
  applyLights(){
    for(const m of this.lightMaterials){
      if(m.name==='Headlight LED')m.emissiveIntensity=this.lights.headlights?4:0;
      if(m.name==='Headlight DRL')m.emissiveIntensity=this.lights.headlights?2.2:0;
      if(m.name==='Headlight DRL diffuser')m.emissiveIntensity=this.lights.headlights?2.8:0;
      if(m.name==='Headlight projector optic')m.emissiveIntensity=this.lights.headlights?2:0;
      if(m.name==='Rear LED light bar')m.emissiveIntensity=this.lights.brake||this.state.brake>.03?5:1;
    }
    for(const beam of this.headlightBeams)beam.visible=this.lights.headlights;
  }
  updateWingLinkages(){
    this.root.updateWorldMatrix(true,true);const up=new THREE.Vector3(0,1,0);
    for(const side of ['L','R']){
      const base=this.nodes.get('WingActuator_Base_'+side),tip=this.nodes.get('WingActuator_Tip_'+side);if(!base||!tip)continue;
      const a=base.getWorldPosition(new THREE.Vector3()),b=tip.getWorldPosition(new THREE.Vector3());
      for(const [name,start,end] of [['Housing',a,b],['Rod',b,a]]){
        const node=this.nodes.get('WingActuator_'+name+'_'+side);if(!node)continue;
        const from=node.parent.worldToLocal(start.clone()),to=node.parent.worldToLocal(end.clone());node.position.copy(from);node.quaternion.setFromUnitVectors(up,to.sub(from).normalize());
      }
    }
  }
  update(dt){
    const d=this.clamp(dt,0,.1),f=1-Math.exp(-d*8),s=this.state;
    if(this.chrono.running){const elapsed=Number(dt);if(Number.isFinite(elapsed)&&elapsed>0)this.setChrono({elapsedSeconds:this.chrono.elapsedSeconds+elapsed});}
    this.instrumentTime+=d;this.instrumentElapsed+=d;if(this.instrumentElapsed>=.1){this.instrumentElapsed=0;this.refreshInstrumentDisplays();}
    for(const k in s)s[k]=THREE.MathUtils.lerp(s[k],this.target[k],f);
    const X=new THREE.Vector3(1,0,0),Y=new THREE.Vector3(0,1,0);
    this.rotate('Door_L',Y,-s.doorL*1.05);this.rotate('Door_R',Y,s.doorR*1.05);
    this.rotate('WingUpper_Hinge',X,THREE.MathUtils.degToRad(40*s.airbrake-12*s.drs*(1-s.airbrake)));
    this.rotate('FrontAero_L',X,THREE.MathUtils.degToRad(20*s.airbrake-8*s.drs));
    this.rotate('FrontAero_R',X,THREE.MathUtils.degToRad(20*s.airbrake-8*s.drs));
    // The fitted exported +Z axis points toward the driver. Positive road
    // steering turns toward vehicle +X (driver's left), as does the rim top.
    // Published 14.1:1 centre ratio; constant approximation of the variable rack.
    this.rotate('SteeringWheel_Pivot',new THREE.Vector3(0,0,1),s.steering*14.1);
    this.rotate('BrakePedal_Pivot',X,s.brake*.18*(this.nodes.get('BrakePedal_Pivot')?.userData.press_axis_sign??-1));this.rotate('ThrottlePedal_Pivot',X,s.throttle*.12*(this.nodes.get('ThrottlePedal_Pivot')?.userData.press_axis_sign??-1));
    this.rotate('ShiftPaddle_L',Y,s.paddleL*.12);this.rotate('ShiftPaddle_R',Y,-s.paddleR*.12);
    const mirror=this.nodes.get('Mirror_Adjust_Interior');if(mirror)mirror.quaternion.copy(this.rest.get(mirror.uuid).quaternion).multiply(new THREE.Quaternion().setFromAxisAngle(Y,s.mirrorYaw)).multiply(new THREE.Quaternion().setFromAxisAngle(X,s.mirrorPitch));
    for(const key of ['FL','FR','RL','RR']){
      if(!this.suspensionRig.has(key))this.rotate((key[0]==='F'?'Steer_':'RearSteer_')+key,Y,key[0]==='F'?s.steering:s.rearSteering);
      this.rotate('WheelSpin_'+key,X,this.wheelAngles[key]);
      const o=this.nodes.get('Suspension_'+key);if(o&&!this.suspensionRig.has(key))o.position.copy(this.rest.get(o.uuid).position).add(new THREE.Vector3(0,this.travel[key],0));
    }
    this.suspensionRig.update(this.travel,s.steering,s.rearSteering);
    for(const side of ['L','R']){const o=this.nodes.get('Window_'+side);if(o)o.position.copy(this.rest.get(o.uuid).position).add(new THREE.Vector3(0,-s['window'+side]*.29,0));}
    this.applyLights();
    this.updateWingLinkages();
  }
  updateViewDetail(camera,viewportHeight){this.coolingDetail.update(camera,viewportHeight);}
  getStats(){let triangles=0,meshes=0;const materials=new Set();this.root.traverse(o=>{if(!o.isMesh)return;meshes++;triangles+=(o.geometry.index?.count??o.geometry.attributes.position.count)/3;for(const m of(Array.isArray(o.material)?o.material:[o.material]))materials.add(m.uuid);});return{triangles,meshes,materials:materials.size};}
  getSuspensionState(){return Object.fromEntries(Object.entries(this.suspensionRig.diagnostics).map(([key,value])=>[key,{...value,angles:[...value.angles]}]));}
  dispose(){if(this.disposed)return;this.coolingDetail.dispose();this.disposed=true;const geometries=new Set(),materials=new Set(),textures=new Set();this.root.traverse(o=>{if(!o.isMesh)return;geometries.add(o.geometry);for(const m of(Array.isArray(o.material)?o.material:[o.material])){materials.add(m);for(const v of Object.values(m))if(v?.isTexture)textures.add(v);}});for(const x of [...geometries,...textures,...materials])x.dispose();}
}


/** Keep individual wires only while their pitch is resolved by the framebuffer.
 * Farther away the retained radiator plane uses unlit, mip-filtered material maps.
 */
class FrontCoolingDetail {
  constructor(root,factory){
    this.root=root;this.factory=factory;this.wires=[];this.backing=[];this.filtered=null;this.detailed=true;
    root.traverse(o=>{if(!o.isMesh||Array.isArray(o.material))return;
      if(o.material.name==='Cooling | satin protective grille wire')this.wires.push(o);
      if(o.material.name==='Cooling | recessed aluminium radiator fins')this.backing.push({mesh:o,original:o.material});
    });
    this.position=new THREE.Vector3();this.cameraPosition=new THREE.Vector3();this.scale=new THREE.Vector3();
  }
  makeMaterial(){
    if(this.factory)return this.factory();
    const loader=new THREE.TextureLoader(),textures={};
    for(const suffix of ['Color','Normal','ORM']){
      const t=loader.load(new URL('../textures/lod/FrontGrille_'+suffix+'.png',import.meta.url).href);
      t.wrapS=t.wrapT=THREE.RepeatWrapping;t.channel=1;t.repeat.set(2,4);t.anisotropy=8;
      if(suffix==='Color')t.colorSpace=THREE.SRGBColorSpace;textures[suffix]=t;
    }
    return new THREE.MeshStandardMaterial({name:'Cooling | filtered Hero grille',map:textures.Color,normalMap:textures.Normal,normalScale:new THREE.Vector2(.55,.55),roughnessMap:textures.ORM,metalnessMap:textures.ORM,roughness:1,metalness:1,side:THREE.DoubleSide});
  }
  update(camera,height){
    if(!this.wires.length||!this.backing.length||!Number.isFinite(height)||height<=0)return;
    const wire=this.wires[0];let ancestor=wire;while(ancestor&&ancestor!==this.root)ancestor=ancestor.parent;if(!ancestor)return;
    wire.updateWorldMatrix(true,false);camera.updateWorldMatrix(true,false);
    if(!wire.geometry.boundingSphere)wire.geometry.computeBoundingSphere();
    this.position.copy(wire.geometry.boundingSphere.center).applyMatrix4(wire.matrixWorld);wire.getWorldScale(this.scale);camera.getWorldPosition(this.cameraPosition);
    const scale=Math.max(Math.abs(this.scale.x),Math.abs(this.scale.y),Math.abs(this.scale.z));
    const pixelsPerMetre=camera.isOrthographicCamera?height*camera.zoom/(camera.top-camera.bottom):height/(2*Math.max(.01,this.position.distanceTo(this.cameraPosition))*Math.tan(camera.getEffectiveFOV()*Math.PI/360));
    this.pitchPixels=.004*scale*pixelsPerMetre;
    const detailed=this.pitchPixels>(this.detailed?2.5:3.5);
    if(!detailed&&!this.filtered)this.filtered=this.makeMaterial();
    this.detailed=detailed;
    for(const wire of this.wires)wire.visible=detailed;
    for(const {mesh,original} of this.backing)mesh.material=detailed?original:this.filtered;
  }
  dispose(){
    for(const {mesh,original} of this.backing)mesh.material=original;
    for(const wire of this.wires)wire.visible=true;
    if(this.filtered){for(const t of new Set([this.filtered.map,this.filtered.normalMap,this.filtered.roughnessMap,this.filtered.metalnessMap]))t?.dispose();this.filtered.dispose();this.filtered=null;}
  }
}
