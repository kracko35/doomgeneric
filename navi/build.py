"""Build big-endian SH-4A Doom; needs the existing private SH cross compiler."""
from pathlib import Path
import concurrent.futures, hashlib, json, os, re, subprocess
HERE=Path(__file__).resolve().parent
SOURCE=HERE.parent/'doomgeneric';BUILD=HERE/'build';BUILD.mkdir(exist_ok=True)
TOOLROOT=Path(os.environ.get('NAVI_SH_TOOLROOT',str(HERE.parent.parent/'hello-navi-20261103/motion-toolchain/root')))
BIN=TOOLROOT/'usr/bin'
gcc=BIN/'sh4-linux-gnu-gcc-13'
env=os.environ.copy();env['PATH']=str(BIN)+os.pathsep+env['PATH']
libs=list(TOOLROOT.rglob('libbfd-*-sh4.so'))
if libs:env['LD_LIBRARY_PATH']=str(libs[0].parent)
flags=['-mb','-m4a','-O2','-g','-ffreestanding','-fno-builtin','-fno-pic','-fno-pie','-fno-stack-protector','-fno-strict-aliasing','-ffunction-sections','-fdata-sections','-nostdinc','-isystem',str(HERE/'include'),'-isystem',subprocess.check_output([str(gcc),'-print-file-name=include'],text=True,env=env).strip(),'-I',str(SOURCE),'-DDOOMGENERIC_RESX=320','-DDOOMGENERIC_RESY=200','-DNAVI_SH4','-include',str(SOURCE/'i_swap.h')]
make=(SOURCE/'Makefile').read_text();objects=re.search(r'SRC_DOOM = (.*)',make)[1].split()
sources=[SOURCE/(x[:-2]+'.c') for x in objects if x!='doomgeneric_xlib.o']+[HERE/'libc.c',HERE/'platform.c',HERE/'start.S',HERE/'division.S']
def compile(src):
    obj=BUILD/(src.stem+'.o')
    active_flags=flags[:-2] if src.suffix=='.S' else flags
    cmd=[str(gcc),*active_flags,'-c',str(src),'-o',str(obj)]
    p=subprocess.run(cmd,env=env,text=True,capture_output=True)
    if p.returncode:raise RuntimeError(src.name+'\n'+p.stderr)
    return str(obj)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objs=list(pool.map(compile,sources))
# The installed libgcc is little endian. division.S/libc.c supply the helpers
# with the correct SH internal ABI instead of linking an incompatible archive.
elf=BUILD/'doom.elf'
subprocess.run([str(BIN/'sh4-linux-gnu-ld'),'-EB','--gc-sections','-T',str(HERE/'link.ld'),'-Map',str(BUILD/'doom.map'),'-o',str(elf),*objs],check=True,env=env)
subprocess.run([str(BIN/'sh4-linux-gnu-objcopy'),'-O','binary',str(elf),str(BUILD/'doom.bin')],check=True,env=env)
nm=subprocess.check_output([str(BIN/'sh4-linux-gnu-nm'),'-n',str(elf)],text=True,env=env)
symbols={p[2]:int(p[0],16) for line in nm.splitlines() if len(p:=line.split())==3}
data=(BUILD/'doom.bin').read_bytes()
report={'base':0x8c000000,'binary':'doom.bin','sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'symbols':symbols,'cpu':'SH-4A big endian','sound':False,'source_commit':'e257a09527062bfb986b6e5e0780839b6cbd5b71',
        'compiler':subprocess.check_output([str(gcc),'--version'],text=True,env=env).splitlines()[0],
        'compile_flags':flags,'sources_sha256':{str(p.relative_to(HERE.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
        'bss_end':symbols['__bss_end'],'heap_start':0x8d000000,'heap_end':0x8f000000}
(BUILD/'build.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='symbols'},indent=2))
