import json
import subprocess
import sys
import os
import shutil
import argparse
from pathlib import Path
import json
import re
import tarfile
import zipfile
import shutil


cwd = os.path.dirname(__file__)
contrib_src_dir = cwd + r'\contrib\src'
contrib_build_dir = cwd + r'\contrib\build'
contrib_tmp_dir = cwd + r'\contrib\tarballs'


wget_args = [
    '--no-check-certificate', '--retry-connrefused',
    '--waitretry=1', '--read-timeout=20',
    '--timeout=15', '--tries=4']
git_apply_args = ['--reject', '--ignore-whitespace', '--whitespace=fix']
patch_args = ['-flp1', '-i']


def findVSLatestDir():
    vsWherePath = os.path.join(
        os.environ['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    args = ['-latest',
            '-products *',
            '-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-property installationPath']
    cmd = [vsWherePath] + args
    output = subprocess.check_output(' '.join(cmd)).decode('utf-8')
    if output:
        return output.splitlines()[0]
    else:
        return


def initVSEnv(arch='x64', platform='', version='10.0.15063.0'):
    vcEnvInit = [findVSLatestDir() + r'\VC\Auxiliary\Build\"vcvarsall.bat']
    if platform != '':
        args = [arch, platform, version]
    else:
        args = [arch, version]
    if args:
        vcEnvInit.extend(args)
    vcEnvInit = 'call \"' + ' '.join(vcEnvInit)
    msbuildEnv = subprocess.Popen(vcEnvInit,
                                  shell=True,
                                  stderr=sys.stderr,
                                  stdout=sys.stdout
                                  )
    msbuildEnv.communicate()


def make(pkg_name, version, url, patches, deps, force):
    pkg_up_to_date = False
    # attempt to get the current built version
    current_version = 'None'
    # check build file for current version
    build_file = contrib_build_dir + r'\\.' + pkg_name
    if os.path.exists(build_file):
        with open(build_file, 'r+') as f:
            current_version = f.read()
            if current_version == version:
                pkg_up_to_date = True
    for dep in deps:
        dep_build_dep = resolve(dep)
        if dep_build_dep:
            pkg_up_to_date = False
    if not pkg_up_to_date or current_version is None or force:
        if current_version is not None:
            print(pkg_name + ' currently @: ' + current_version)
        if force:
            print('forcing fetch/patch/build for ' + pkg_name)
        if not pkg_up_to_date or force:
            if not force:
                print(pkg_name + ' is not up to date')
            fetch_pkg(pkg_name, version, url)
            apply(pkg_name, patches)
        print('building ' + pkg_name + ' @ ' + version)
        build(pkg_name)
        track_build(pkg_name, version)
        return True
    # did not build
    return False


def apply(pkg_name, patches):
    for p in patches:
        print('applying ' + p)


def resolve(pkg_name, force=False):
    if pkg_name == None:
        raise IOError("Can't just do nothing!")
    pkg_json = cwd + r'\contrib\src\\' + pkg_name + r"\\package.json"
    with open(pkg_json) as json_file:
        data = json.load(json_file)
        return make(pkg_name, data['version'], data['url'], data['patches'], data['deps'], force)


def track_build(pkg_name, version):
    if pkg_name == None or version == None:
        raise IOError("Can't just do nothing!")
    if not os.path.exists(contrib_build_dir):
        os.makedirs(contrib_build_dir)
    build_file = contrib_build_dir + '\\.' + pkg_name
    f = open(build_file, "w+")
    f.write(version)
    f.close()


def fetch_pkg(pkg_name, version, url):
    version_replace = re.compile(re.escape('__VERSION__'))
    full_url = version_replace.sub(version, url)
    tarball_name = full_url[full_url.rfind("/") + 1:]
    tarball_path = contrib_tmp_dir + '\\' + tarball_name
    if not os.path.exists(tarball_path):
        print('fetching ' + pkg_name + ' from: ' + full_url)
        args = [full_url, '-P', contrib_tmp_dir]
        args.extend(wget_args)
        getSHrunner().exec_batch('wget', args)
    else:
        print(tarball_name + ' already exists in the tarball directory')
    decompress_tarball(pkg_name, tarball_name, tarball_path)
    #file = wget.download(full_url)
    # r = requests.get(full_url)
    # print(len(r.content))


def decompress_tarball(pkg_name, name, path):
    pkg_build_path = contrib_build_dir + '\\' + pkg_name
    if os.path.exists(pkg_build_path):
        getSHrunner().exec_batch('rmdir', ['/s', '/q', pkg_build_path])
    print('decompressing ' + name + ' to ' + pkg_build_path)
    zf = zipfile.ZipFile(path, 'r')
    print(zf.namelist())
    # with tarfile.open(path, 'r') as tarball:
    #     for member in tarball.getmembers():
    #         print("Extracting %s" % member.name)
    #         tarball.extract(member, pkg_build_path)
        #tarball.extractall(pkg_build_path)
    #shutil.move(pkg_build_path + '\\' + name, pkg_build_path)


def track_decompression(members):
    for member in members:
        yield member


def build(pkg_name):
    print('building ...')


class Singleton:
    def __init__(self, decorated):
        self._decorated = decorated

    def instance(self):
        try:
            return self._instance
        except AttributeError:
            self._instance = self._decorated()
            return self._instance

    def __call__(self):
        raise TypeError('Singletons must be accessed through `instance()`.')

    def __instancecheck__(self, inst):
        return isinstance(inst, self._decorated)


class ScriptType:
    ps1 = 1
    cmd = 2
    sh = 3


@Singleton
class SHrunner():
    def __init__(self):
        self.ps_path = r'C:\Windows\system32\WindowsPowerShell\v1.0\powershell.exe'
        self.sh_path = r'C:\Windows\system32\bash.exe'

    def exec_script(self, script_type=ScriptType.cmd, script=None, args=[]):
        if script_type is ScriptType.cmd:
            cmd = [script]
        elif script_type is ScriptType.ps1:
            if not os.path.exists(self.ps_path):
                raise IOError('Powershell not found at %s.' % self.ps_path)
            cmd = [self.ps_path, '-ExecutionPolicy', 'ByPass', script]
        elif script_type is ScriptType.sh:
            if not os.path.exists(self.sh_path):
                raise IOError('Bash not found at %s.' % self.ps_path)
            cmd = [self.sh_path, '-c', script]
        else:
            print('not implemented')
            return 1
        if args:
            cmd.extend(args)
        p = subprocess.Popen(cmd,
                             shell=True,
                             stderr=sys.stderr,
                             stdout=sys.stdout
                             )
        rtrn, perr = p.communicate()
        rcode = p.returncode
        data = None
        if perr:
            data = json.dumps(perr.decode('utf-8', 'ignore'))
        else:
            data = rtrn
        return rcode, data

    def exec_batch(self, script=None, args=[]):
        return self.exec_script(ScriptType.cmd, script, args)

    def exec_ps1(self, script=None, args=[]):
        return self.exec_script(ScriptType.ps1, script, args)

    def exec_sh(self, script=None, args=[]):
        return self.exec_script(ScriptType.sh, script, args)


def getSHrunner():
    return SHrunner.instance()


@Singleton
class MSbuilder:
    def __init__(self, msbuild=None):
        if msbuild == None:
            self.msbuild = r'C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe'
        else:
            self.msbuild = msbuild

    def build(self, projPath):
        if not os.path.isfile(self.msbuild):
            raise Exception('msbuild.exe not found. path=' + self.msbuild)

        arg1 = '/t:Rebuild'
        arg2 = '/p:Configuration=Release'
        p = subprocess.call([self.msbuild, projPath, arg1, arg2])
        if p == 1:
            return False

        return True


def getMSbuilder():
    return MSbuilder.instance()


def parse_args():
    ap = argparse.ArgumentParser(description="Windows Jami build tool")
    ap.add_argument(
        '-b', '--build',
        help='Build latest contrib')
    ap.add_argument(
        '-f', '--force', action='store_true',
        help='Force action')

    parsed_args = ap.parse_args()

    return parsed_args


def main():
    parsed_args = parse_args()

    if parsed_args.build:
        resolve(parsed_args.build, parsed_args.force)
        print(parsed_args.build + ' up to date')


if __name__ == '__main__':
    # initVSEnv()
    main()
