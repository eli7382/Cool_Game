// Compare every converted HDR channel against the viewer's independent loader.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL,fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import assert from 'node:assert/strict';
const source=path.resolve(process.argv[2]),root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),vendor=pathToFileURL(path.join(source,'viewer/vendor/three.module.js')).href;
const THREE=await import(vendor),loaderSource=(await fs.readFile(path.join(source,'viewer/vendor/addons/loaders/HDRLoader.js'),'utf8')).replace("from 'three'",`from '${vendor}'`);
const {HDRLoader}=await import('data:text/javascript;base64,'+Buffer.from(loaderSource).toString('base64'));
const hdr=await fs.readFile(path.join(root,'assets/porsche/environments/daylight.hdr')),linear=await fs.readFile(path.join(root,'assets/porsche/environments/daylight.rgb32f'));
const parsed=new HDRLoader().setDataType(THREE.FloatType).parse(hdr.buffer.slice(hdr.byteOffset,hdr.byteOffset+hdr.byteLength));
assert.equal(linear.subarray(0,4).toString(),'PGHF');assert.equal(linear.readUInt32LE(4),parsed.width);assert.equal(linear.readUInt32LE(8),parsed.height);assert.equal(linear.length,16+parsed.width*parsed.height*12);
let maximum=0;
for(let i=0;i<parsed.width*parsed.height;i++)for(let c=0;c<3;c++){const expected=parsed.data[i*4+c],actual=linear.readFloatLE(16+(i*3+c)*4);assert.ok(Number.isFinite(actual));maximum=Math.max(maximum,Math.abs(actual-expected));assert.ok(Math.abs(actual-expected)<=Math.max(1e-7,Math.abs(expected)*2e-7));}
const sha=b=>createHash('sha256').update(b).digest('hex');
const report={passed:true,checker_sha256:sha(await fs.readFile(fileURLToPath(import.meta.url))),loader_sha256:sha(await fs.readFile(path.join(source,'viewer/vendor/addons/loaders/HDRLoader.js'))),source_sha256:sha(hdr),linear_sha256:sha(linear),channels_compared:parsed.width*parsed.height*3,maximum_absolute_error:maximum};
await fs.writeFile(path.join(root,'docs/porsche-environment-validation.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));
