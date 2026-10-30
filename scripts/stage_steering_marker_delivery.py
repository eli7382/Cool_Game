"""Stage only this task's reviewed delivery, excluding the user's session log."""
import subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mode=sys.argv[1] if len(sys.argv)>1 else 'stage';assert mode in ['stage','commit']
if mode=='stage':
 modified=subprocess.check_output(['git','ls-files','--modified','-z'],cwd=root).decode().split('\0')
 added=subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=root).decode().split('\0')
 files=sorted(set(p for p in modified+added if p and p!='logs/breakwater-session.log'))
 allowed=('assets/porsche/','docs/','screenshots/porsche-','scripts/','src/breakwater/porsche_','tests/')
 assert files and all(p=='README.md' or p=='dist/Breakwater_Windows_x64.exe' or p.startswith(allowed) for p in files),files
 assert all((root/p).stat().st_size<95*1024*1024 for p in files)
 subprocess.run(['git','add','--sparse','--',*files],cwd=root,check=True)
 subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
 staged=subprocess.check_output(['git','diff','--cached','--name-only','-z'],cwd=root).decode().split('\0')
 assert 'logs/breakwater-session.log' not in staged
 print(f'Staged {len([p for p in staged if p])} reviewed files; user session log excluded.',flush=True)
else:
 staged=subprocess.check_output(['git','diff','--cached','--name-only','-z'],cwd=root).decode().split('\0')
 assert 'logs/breakwater-session.log' not in staged and 'docs/porsche-steering-marker-review.json' in staged
 message='Add GT3 RS centering band and avoid duplicate texture uploads\n\nUse the existing rim crown faces for the yellow marker, preserve the fitted steering axis, and retain the full rim at every detail level. Keep complete material definitions in the native metadata/header while the stock GLB loader receives untextured placeholders, avoiding transient duplicate texture uploads. Ship the updated editable source and tested Windows executable.\n\nValidation: 155 native checks, 69 native steering poses, 138 indexed-marker motion poses, 36 main-view and 54 mirror-target culling pairs, 15 detail-switch cases, and source/native attribute correspondence pass. Current instrumented local mean frame times are 53-116 ms; final visual parity and rendering performance remain in progress.'
 subprocess.run(['git','-c','user.name=Codex','-c','user.email=codex@localhost','commit','-m',message],cwd=root,check=True)
