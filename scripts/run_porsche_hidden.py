"""Run a native Porsche QA mode with a hidden window and a retained log."""
import subprocess,sys,json,time,ctypes
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mode=sys.argv[1] if len(sys.argv)>1 else 'qa'
assert mode in ['qa','culling','performance']
log=root/f'.tools/porsche-marker-{mode}.log';log.parent.mkdir(exist_ok=True)
with log.open('w',encoding='utf-8') as stream:
 command=[str(root/'build-native/ninja/bin/game.exe'),'--qa-porsche'+('' if mode=='qa' else '-'+mode)]
 process=subprocess.Popen(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT)
 print(json.dumps({'mode':mode,'pid':process.pid,'started':time.time(),'log':str(log)}),flush=True)
 class Memory(ctypes.Structure):
  _fields_=[('length',ctypes.c_ulong),('load',ctypes.c_ulong)]+[(k,ctypes.c_ulonglong) for k in ['physical','physical_free','commit','commit_free','virtual','virtual_free','extended']]
 class ProcessMemory(ctypes.Structure):
  _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(k,ctypes.c_size_t) for k in ['peak_working','working','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
 kernel=ctypes.windll.kernel32;kernel.OpenProcess.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 ctypes.windll.psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
 handle=kernel.OpenProcess(0x410,False,process.pid);minimum_commit=2**64;peak_private=peak_working=0
 while process.poll() is None:
  memory=Memory();memory.length=ctypes.sizeof(memory);kernel.GlobalMemoryStatusEx(ctypes.byref(memory));minimum_commit=min(minimum_commit,memory.commit_free)
  pm=ProcessMemory();pm.cb=ctypes.sizeof(pm)
  if handle and ctypes.windll.psapi.GetProcessMemoryInfo(handle,ctypes.byref(pm),pm.cb):peak_private=max(peak_private,pm.private);peak_working=max(peak_working,pm.working)
  time.sleep(.1)
 if handle:kernel.CloseHandle(handle)
 result=process.returncode
 measurement={'mode':mode,'exit_code':result,'minimum_system_commit_free_bytes':minimum_commit,'peak_process_private_bytes':peak_private,'peak_process_working_bytes':peak_working,'executable':str(command[0])}
 (root/f'.tools/porsche-marker-{mode}-resources.json').write_text(json.dumps(measurement,indent=2));print(json.dumps(measurement),flush=True)
print(json.dumps({'mode':mode,'exit_code':result}),flush=True)
if result:print(log.read_text(errors='replace')[-3000:])
sys.exit(result)
