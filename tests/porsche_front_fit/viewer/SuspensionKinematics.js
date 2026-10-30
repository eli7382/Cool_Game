// Shared-rack front solver; source-fitted game datums, not OEM CAD.
/** Visual linkage solver fitted to the asset's retained meshes.
 * It supplies articulation, not tire forces or OEM vehicle dynamics.
 * Three is injected so this module works with the host renderer's instance.
 */
export class SuspensionKinematics {
  constructor(car,THREE){
    this.car=car;this.T=THREE;this.frame=car.nodes.get('CarRoot')||car.root;this.corners=new Map();this.diagnostics={};
    this.frame.updateWorldMatrix(true,true);const inverse=this.frame.matrixWorld.clone().invert();
    const point=name=>car.nodes.get(name)?.getWorldPosition(new THREE.Vector3()).applyMatrix4(inverse);
    for(const key of ['FL','FR','RL','RR']){
      const lower=car.nodes.get('LowerArm_'+key),upper=car.nodes.get('UpperArm_'+key);if(!lower||!upper)continue;
      const A=point('LowerArm_'+key),B=point('UpperArm_'+key),C=point('LowerBall_'+key),D=point('UpperBall_'+key),K=point('Suspension_'+key);
      const e={key,front:key[0]==='F',lower,upper,A,B,C,D,K,lowerAxis:point('LowerAxis_'+key).sub(A).normalize(),upperAxis:point('UpperAxis_'+key).sub(B).normalize(),lowerLength:C.distanceTo(A),uprightLength:D.distanceTo(C),upperLength:D.distanceTo(B),offset:K.clone().sub(C),upright:D.clone().sub(C),angles:key[0]==='F'?[0,0,0]:[0,0,0,0,0]};
      e.carrier=car.nodes.get('Suspension_'+key);e.steer=car.nodes.get((e.front?'Steer_':'RearSteer_')+key);
      e.restCarrier=this.restQuaternion(e.carrier).clone();
      const wheelFrame=inverse.clone().multiply(car.nodes.get('WheelSpin_'+key).matrixWorld);
      e.restWheelNormal=new THREE.Vector3(1,0,0).transformDirection(wheelFrame);e.restToe=Math.atan2(-e.restWheelNormal.z,e.restWheelNormal.x);
      if(!e.front){e.U=point('TrailingArm_'+key);e.E=point('TrailingOuter_'+key);e.trailingLength=e.E.distanceTo(e.U);e.trailing=car.nodes.get('TrailingArm_'+key);e.V=point('UpperTrailingArm_'+key);e.F=point('UpperTrailingOuter_'+key);e.fourthLength=e.F.distanceTo(e.V);e.fourth=car.nodes.get('UpperTrailingArm_'+key);}
      e.tieA=point('TieInner_'+key);e.tieB=point('TieOuter_'+key);e.tieLength=e.tieA.distanceTo(e.tieB);e.tieDirection=e.tieB.clone().sub(e.tieA).normalize();
      e.driveA=point('DriveInner_'+key);e.driveB=point('DriveOuter_'+key);if(e.driveA){e.driveLength=e.driveA.distanceTo(e.driveB);e.driveDirection=e.driveB.clone().sub(e.driveA).normalize();}
      e.damperTop=point('DamperTop_'+key);e.damperBottom=point('DamperBottom_'+key);e.damperDirection=e.damperTop.clone().sub(e.damperBottom).normalize();
      e.arbA=point('ARB_LinkBottom_'+key);e.arbB=point('ARB_End_'+key);e.arbPivot=point('ARB_Pivot_'+key);e.arbAngle=0;
      if(e.arbA){e.arbLength=e.arbA.distanceTo(e.arbB);e.arbDirection=e.arbB.clone().sub(e.arbA).normalize();}
      this.corners.set(key,e);
    }
  }
  has(key){return this.corners.has(key);}
  quaternion(axis,angle){return new this.T.Quaternion().setFromAxisAngle(axis,angle);}
  restQuaternion(node){return this.car.rest.get(node.uuid).quaternion;}
  rackForSteering(steering){
    const offsets=[];
    for(const e of this.corners.values())if(e.front){
      const axis=e.upright.clone().normalize();
      const error=angle=>{const n=e.restWheelNormal.clone().applyQuaternion(this.quaternion(axis,angle)),yaw=Math.atan2(-n.z,n.x)-e.restToe-steering;return Math.atan2(Math.sin(yaw),Math.cos(yaw));};
      let angle=steering;
      for(let i=0;i<12&&Math.abs(error(angle))>1e-11;++i){const r=error(angle),derivative=(error(angle+1e-5)-r)/1e-5;if(Math.abs(derivative)<1e-9)throw Error('Singular rack steering calibration');angle-=Math.max(-.2,Math.min(.2,r/derivative));}
      if(Math.abs(angle)>.7||Math.abs(error(angle))>1e-8)throw Error('Requested wheel yaw exceeds rack calibration');
      const q=this.quaternion(axis,angle);
      const outer=e.tieB.clone().sub(e.C).applyQuaternion(q).add(e.C),dy=outer.y-e.tieA.y,dz=outer.z-e.tieA.z;
      const square=e.tieLength*e.tieLength-dy*dy-dz*dz;
      if(square<0)throw Error('Requested steering exceeds rack closure');
      offsets.push(outer.x-e.tieA.x-Math.sign(e.tieB.x-e.tieA.x)*Math.sqrt(square));
    }
    return offsets.reduce((a,b)=>a+b,0)/offsets.length;
  }
  frontPose(e,x,travel,steering){
    const lowerQ=this.quaternion(e.lowerAxis,x[0]),upperQ=this.quaternion(e.upperAxis,x[1]);
    const C=e.C.clone().sub(e.A).applyQuaternion(lowerQ).add(e.A),D=e.D.clone().sub(e.B).applyQuaternion(upperQ).add(e.B);
    const camber=new this.T.Quaternion().setFromUnitVectors(e.upright.clone().normalize(),D.clone().sub(C).normalize());
    const steer=this.quaternion(e.upright.clone().normalize(),x[2]),total=camber.clone().multiply(steer),hub=e.offset.clone().applyQuaternion(total).add(C);
    const outer=e.tieB.clone().sub(e.C).applyQuaternion(total).add(C),inner=e.tieA.clone().add(new this.T.Vector3(this.rackOffset,0,0));
    return {C,D,lowerQ,upperQ,camber,steer,total,hub,residual:[D.distanceTo(C)-e.uprightLength,hub.y-e.K.y-travel,outer.distanceTo(inner)-e.tieLength]};
  }
  rearPose(e,x,travel,steering){
    const hub=e.K.clone().add(new this.T.Vector3(x[0],travel,x[1])),total=new this.T.Quaternion().setFromEuler(new this.T.Euler(x[2],x[3],x[4],'YXZ'));
    const attach=rest=>rest.clone().sub(e.K).applyQuaternion(total).add(hub),C=attach(e.C),D=attach(e.D),E=attach(e.E),F=attach(e.F);
    const normal=e.restWheelNormal.clone().applyQuaternion(total),toe=Math.atan2(-normal.z,normal.x),toeError=Math.atan2(Math.sin(toe-e.restToe-steering),Math.cos(toe-e.restToe-steering));
    const steer=this.quaternion(e.upright.clone().normalize(),steering),camber=total.clone().multiply(steer.clone().invert());
    const upperQ=new this.T.Quaternion().setFromUnitVectors(e.D.clone().sub(e.B).normalize(),D.clone().sub(e.B).normalize());
    const lowerQ=new this.T.Quaternion().setFromUnitVectors(e.C.clone().sub(e.A).normalize(),C.clone().sub(e.A).normalize());
    const trailingQ=new this.T.Quaternion().setFromUnitVectors(e.E.clone().sub(e.U).normalize(),E.clone().sub(e.U).normalize());
    const fourthQ=new this.T.Quaternion().setFromUnitVectors(e.F.clone().sub(e.V).normalize(),F.clone().sub(e.V).normalize());
    return {C,D,E,F,lowerQ,upperQ,trailingQ,fourthQ,camber,steer,total,hub,residual:[C.distanceTo(e.A)-e.lowerLength,D.distanceTo(e.B)-e.upperLength,E.distanceTo(e.U)-e.trailingLength,F.distanceTo(e.V)-e.fourthLength,toeError*.3]};
  }
  linearSolve(matrix,rhs){
    const n=rhs.length,a=matrix.map((row,i)=>[...row,rhs[i]]);
    for(let j=0;j<n;j++){
      let pivot=j;for(let i=j+1;i<n;i++)if(Math.abs(a[i][j])>Math.abs(a[pivot][j]))pivot=i;
      if(Math.abs(a[pivot][j])<1e-12)return null;[a[j],a[pivot]]=[a[pivot],a[j]];
      const factor=a[j][j];for(let k=j;k<=n;k++)a[j][k]/=factor;
      for(let i=0;i<n;i++)if(i!==j){const multiple=a[i][j];for(let k=j;k<=n;k++)a[i][k]-=multiple*a[j][k];}
    }
    return a.map(row=>row[n]);
  }
  solve(initial,evaluate){
    let x=[...initial],pose=evaluate(x);const error=p=>Math.max(...p.residual.map(Math.abs));
    for(let iteration=0;iteration<20&&error(pose)>1e-8;iteration++){
      const n=x.length,columns=[];
      for(let j=0;j<n;j++){const probe=[...x];probe[j]+=1e-5;const r=evaluate(probe).residual;columns.push(r.map((value,i)=>(value-pose.residual[i])/1e-5));}
      const delta=this.linearSolve(Array.from({length:n},(_,i)=>columns.map(c=>c[i])),pose.residual.map(v=>-v));if(!delta)break;
      let improved=false;
      for(let scale=1;scale>=1/128;scale/=2){const trial=x.map((value,i)=>Math.max(-.7,Math.min(.7,value+Math.max(-.18,Math.min(.18,delta[i]))*scale))),next=evaluate(trial);if(error(next)<error(pose)){x=trial;pose=next;improved=true;break;}}
      if(!improved)break;
    }
    return {x,pose,error:error(pose)};
  }
  setRotation(node,rotation){node.quaternion.copy(rotation).multiply(this.restQuaternion(node));}
  localPoint(name){this.frame.updateWorldMatrix(true,false);return this.frame.worldToLocal(this.car.nodes.get(name).getWorldPosition(new this.T.Vector3()));}
  align(name,start,end,restDirection){
    const node=this.car.nodes.get(name);if(!node)return;
    node.position.copy(start);const rotation=new this.T.Quaternion().setFromUnitVectors(restDirection,end.clone().sub(start).normalize());this.setRotation(node,rotation);
  }
  updateLinks(e){
    const side=e.key[1]==='L'?1:-1,B=this.localPoint('TieOuter_'+e.key),A=e.tieA.clone(),dy=B.y-A.y,dz=B.z-A.z;
    // The rack's inner joint slides laterally; the rod itself keeps its length.
    const transverse=e.tieLength*e.tieLength-dy*dy-dz*dz;A.x=e.front?e.tieA.x+this.rackOffset:B.x-side*Math.sqrt(Math.max(1e-8,transverse));
    this.align('TieRod_'+e.key,A,B,e.tieDirection);
    const inner=this.car.nodes.get('TieInner_'+e.key);inner.position.copy(A);
    if(e.driveA){
      const end=this.localPoint('DriveOuter_'+e.key);this.align('DriveShaft_'+e.key,e.driveA,end,e.driveDirection);
      // Sub-percent plunge of the retained source axle is represented along its
      // longitudinal axis; the exported rig identifies this visual approximation.
      this.car.nodes.get('DriveShaft_'+e.key).scale.y=end.distanceTo(e.driveA)/e.driveLength;
    }
    const top=this.localPoint('DamperTop_'+e.key),bottom=this.localPoint('DamperBottom_'+e.key),direction=top.clone().sub(bottom).normalize();
    this.align('DamperBody_'+e.key,bottom,top,e.damperDirection);this.align('DamperRod_'+e.key,top,bottom,e.damperDirection.clone().negate());
    const spring=this.car.nodes.get('Spring_'+e.key),offset=Number(spring.userData.lower_seat_offset)||.075,upper=Number(spring.userData.upper_seat_offset)||.045;
    this.align('Spring_'+e.key,bottom.clone().addScaledVector(direction,offset),top,e.damperDirection);
    const span=top.distanceTo(bottom)-offset-upper,rest=Number(spring.userData.spring_rest_span)||e.damperTop.distanceTo(e.damperBottom)-offset-upper,range=Number(spring.userData.spring_morph_range)||.075;
    const compressed=Math.max(0,Math.min(1,(rest-span)/range)),extended=Math.max(0,Math.min(1,(span-rest)/range));
    spring.traverse(o=>{if(!o.morphTargetDictionary)return;for(const [name,value] of [['Compressed',compressed],['Extended',extended]]){const index=o.morphTargetDictionary[name];if(index!==undefined)o.morphTargetInfluences[index]=value;}});
    this.diagnostics[e.key].springSpan=span;this.diagnostics[e.key].springEnvelopeExceeded=Math.abs(span-rest)>range+1e-6;this.diagnostics[e.key].tieLengthError=Math.abs(A.distanceTo(B)-e.tieLength);
    if(e.arbA){
      const lower=this.localPoint('ARB_LinkBottom_'+e.key),axis=new this.T.Vector3(1,0,0),arm=e.arbB.clone().sub(e.arbPivot);
      const endpoint=angle=>arm.clone().applyQuaternion(this.quaternion(axis,angle)).add(e.arbPivot),residual=angle=>endpoint(angle).distanceTo(lower)-e.arbLength;
      let angle=e.arbAngle;
      for(let i=0;i<20&&Math.abs(residual(angle))>1e-8;i++){const r=residual(angle),derivative=(residual(angle+1e-5)-r)/1e-5;if(Math.abs(derivative)<1e-9)break;angle=Math.max(-1.2,Math.min(1.2,angle-Math.max(-.18,Math.min(.18,r/derivative))));}
      e.arbAngle=angle;this.setRotation(this.car.nodes.get('ARB_Pivot_'+e.key),this.quaternion(axis,angle));this.align('ARB_Link_'+e.key,lower,endpoint(angle),e.arbDirection);
      this.diagnostics[e.key].antirollLinkError=Math.abs(residual(angle));
    }
  }
  update(travel,frontSteering,rearSteering){
    const inputs=[frontSteering,rearSteering,...['FL','FR','RL','RR'].map(key=>travel[key]||0)];if(this.previousInputs&&inputs.every((v,i)=>v===this.previousInputs[i]))return;this.previousInputs=inputs;this.rackOffset=this.rackForSteering(frontSteering);
    for(const e of this.corners.values()){
      const height=travel[e.key]||0,steering=e.front?frontSteering:rearSteering,evaluate=x=>e.front?this.frontPose(e,x,height,steering):this.rearPose(e,x,height,steering);
      let solution=this.solve(e.angles,evaluate);
      if(solution.error>1e-6){const restart=this.solve(e.front?[0,0,steering]:[0,0,0,steering,0],evaluate);if(restart.error<solution.error)solution=restart;}
      const p=solution.pose;e.angles=solution.x;
      this.setRotation(e.lower,p.lowerQ);this.setRotation(e.upper,p.upperQ);if(e.trailing)this.setRotation(e.trailing,p.trailingQ);
      if(e.fourth)this.setRotation(e.fourth,p.fourthQ);
      e.carrier.position.copy(e.offset).applyQuaternion(p.camber).add(p.C);this.setRotation(e.carrier,p.camber);
      // Solver rotations are in CarRoot space. Steering is parented below the
      // carrier's authored rest frame, and its origin can be rebased independently.
      const inverseRest=e.restCarrier.clone().invert(),localSteer=inverseRest.clone().multiply(p.steer).multiply(e.restCarrier),restPosition=this.car.rest.get(e.steer.uuid).position;
      const pivot=e.C.clone().sub(e.K).applyQuaternion(inverseRest).sub(restPosition);
      e.steer.position.copy(restPosition).add(pivot).sub(pivot.clone().applyQuaternion(localSteer));this.setRotation(e.steer,localSteer);
      this.diagnostics[e.key]={converged:solution.error<1e-6,residual:solution.error,requestedTravel:height,achievedTravel:p.hub.y-e.K.y,angles:[...e.angles],actualSteeringAngle:e.front?solution.x[2]:steering,rackOffset:e.front?this.rackOffset:null};
    }
    this.frame.updateWorldMatrix(true,true);for(const e of this.corners.values())this.updateLinks(e);
  }
}
