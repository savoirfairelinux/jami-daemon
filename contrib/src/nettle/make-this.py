import subprocess, os, sys
from pathlib import Path
sys.path.append(str(Path(os.path.dirname(__file__)).parents[2]))
import winmake

# dependencies
# deps = [
#     'gmp'
# ]
# for d in deps: 
#     print('resolving dependency: ' + d) 
#     winmake.resolve(d)


# patch
patches = []


# build
print('building nettle...')