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

daemon_dir = os.getcwd()
contrib_src_dir = daemon_dir + r'\contrib\src'
contrib_build_dir = daemon_dir + r'\contrib\build'
contrib_tmp_dir = daemon_dir + r'\contrib\tarballs'


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
    vsenv_done = True


def make(pkg_name, version, url, patches, deps, force):
    pkg_up_to_date = False
    # attempt to get the current built version
    current_version = ''
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
        if not current_version is None:
            print(pkg_name + ' currently @: ' + current_version)
        if force:
            print('Forcing fetch/patch/build for ' + pkg_name)
        if not pkg_up_to_date or force:
            if not force and not current_version is None:
                print(pkg_name + ' is not up to date')
            fetch_success = fetch_pkg(pkg_name, version, url)
            apply_success = apply(pkg_name, patches)
            if not fetch_success or not apply_success:
                raise Exception("Can't resolve contrib " + pkg_name)
        print('Building ' + pkg_name + ' @ ' + version)
        build(pkg_name)
        track_build(pkg_name, version)
        return True
    # did not build
    return False


def apply(pkg_name, patches):
    for p in patches:
        print('applying ' + p)
    return True


def resolve(pkg_name, force=False):
    pkg_json = daemon_dir + r'\contrib\src\\' + pkg_name + r"\\package.json"
    with open(pkg_json) as json_file:
        data = json.load(json_file)
        try:
            return make(pkg_name, data['version'], data['url'], data['patches'], data['deps'], force)
        except Exception:
            print("Make " + pkg_name + ' failed!')
            sys.exit(1)


def track_build(pkg_name, version):
    build_file = contrib_build_dir + '\\.' + pkg_name
    f = open(build_file, "w+")
    f.write(version)
    f.close()


def fetch_pkg(pkg_name, version, url):
    version_replace = re.compile(re.escape('__VERSION__'))
    full_url = version_replace.sub(version, url)
    if not full_url:
        print(pkg_name + ' missing url in package configuration')
        return False
    tarball_name = full_url[full_url.rfind("/") + 1:]
    tarball_path = contrib_tmp_dir + '\\' + tarball_name
    if not os.path.exists(tarball_path):
        print('Fetching ' + pkg_name + ' from: ' + full_url)
        args = [full_url, '-P', contrib_tmp_dir]
        args.extend(wget_args)
        dl_result = getSHrunner().exec_batch('wget', args)
        if dl_result[0] is not 0:
            print('Wget failure. Using powershell Invoke-WebRequest instead')
            args = ['-Uri', full_url, '-OutFile', tarball_path]
            dl_result = getSHrunner().exec_ps1('Invoke-WebRequest', args)
    else:
        print(tarball_name + ' already exists in the tarball directory')
    return decompress_tarball(pkg_name, tarball_name, tarball_path)


def decompress_tarball(pkg_name, name, path):
    with tarfile.open(path, 'r') as tarball:
        pkg_build_path = contrib_build_dir + '\\' + pkg_name
        tar_common_prefix = os.path.commonprefix(tarball.getnames())
        dirty_path = contrib_build_dir + '\\' + tar_common_prefix
        if os.path.exists(pkg_build_path):
            print('Removing old package ' + pkg_build_path)
            getSHrunner().exec_batch('rmdir', ['/s', '/q', pkg_build_path])
        elif os.path.exists(dirty_path):
            print('Removing partial decompression ' + dirty_path)
            getSHrunner().exec_batch('rmdir', ['/s', '/q', dirty_path])
        print('Decompressing ' + name + ' to ' + pkg_build_path)
        tarball.extractall(contrib_build_dir)
        os.rename(contrib_build_dir + '\\' + tar_common_prefix,
                  pkg_build_path)
        return True
    return False


def track_decompression(members):
    for member in members:
        yield member


def build(pkg_name):
    getMSbuilder().setup_vs_env()
    print('Building ...')


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
        self.vsenv_done = False
        if msbuild == None:
            self.msbuild = r'C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe'
        else:
            self.msbuild = msbuild

    def build(self, projPath):
        if not os.path.isfile(self.msbuild):
            raise IOError('msbuild.exe not found. path=' + self.msbuild)

        arg1 = '/t:Rebuild'
        arg2 = '/p:Configuration=Release'
        p = subprocess.call([self.msbuild, projPath, arg1, arg2])
        if p == 1:
            return False

        return True

    def setup_vs_env(self):
        if self.vsenv_done:
            return
        print('Settin up vs environment')
        initVSEnv()
        self.vsenv_done = True


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
    ap.add_argument(
        '-c', '--clean', action='store_true',
        help='Cleans out contrib build directory')
    ap.add_argument(
        '-p', '--purge', action='store_true',
        help='Cleans out contrib tarball directory')

    parsed_args = ap.parse_args()

    return parsed_args


def main():
    parsed_args = parse_args()

    if parsed_args.purge:
        if os.path.exists(contrib_tmp_dir):
            print('Removing contrib tarballs ' + contrib_tmp_dir)
            getSHrunner().exec_batch('del', ['/s', '/f','/q', contrib_tmp_dir + '\\*.tar.*'])
            getSHrunner().exec_batch('del', ['/s', '/f','/q', contrib_tmp_dir + '\\*.tgz.*'])
            getSHrunner().exec_batch('del', ['/s', '/f','/q', contrib_tmp_dir + '\\*.zip.*'])
        else:
            print('No tarballs to remove')
    if parsed_args.clean:
        if os.path.exists(contrib_build_dir):
            print('Removing contrib builds ' + contrib_build_dir)
            getSHrunner().exec_batch('rmdir', ['/s', '/q', contrib_build_dir])
        else:
            print('No builds to remove')
    if parsed_args.build:
        if not os.path.exists(contrib_build_dir):
            os.makedirs(contrib_build_dir)
        print('Resolving ' + parsed_args.build)
        resolve(parsed_args.build, parsed_args.force)
        print(parsed_args.build + ' up to date')


if __name__ == '__main__':
    main()
