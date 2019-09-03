import subprocess, os, sys
from pathlib import Path
sys.path.append(str(Path(os.path.dirname(__file__)).parents[2]))
import winmake


# build
print('building zlib...')