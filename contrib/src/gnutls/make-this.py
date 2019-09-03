import os, sys
from pathlib import Path
this_file_path = os.path.dirname(__file__)
sys.path.append(str(Path(this_file_path).parents[2]))
import winmake

this_package_name = os.path.basename(this_file_path)
build_dir = str(Path(this_file_path).parents[1]) + r'\build'
GNUTLS_VERSION='3.6.7'

# dependencies
deps = [
    'nettle',
    'zlib',
    'gmp'
]
for d in deps: 
    winmake.resolve(d)

# # patch
# patches = []
# for p in patches: 

# build
print('building ' + this_package_name)

# done
if not os.path.exists(build_dir):
    os.makedirs(build_dir)
build_file = build_dir + '\\.' + this_package_name
Path(build_file).touch()