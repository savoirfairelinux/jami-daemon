import json
import subprocess
import sys
import os
import shutil
import argparse
from pathlib import Path
import json
import re
import zipfile
import tarfile
import multiprocessing
import shlex

daemon_dir = os.getcwd()
contrib_src_dir = daemon_dir + r'\contrib\src'
contrib_build_dir = daemon_dir + r'\contrib\build'
contrib_tmp_dir = daemon_dir + r'\contrib\tarballs'

extra_env_vars = {
    'DAEMON_DIR': daemon_dir,
    'CONTRIB_SRC_DIR': contrib_src_dir,
    'CONTRIB_BUILD_DIR': contrib_build_dir,
    }

wget_args = [
    '--no-check-certificate', '--retry-connrefused',
    '--waitretry=1', '--read-timeout=20',
    '--timeout=15', '--tries=4']
git_apply_args = ['apply', '--reject',
                  '--ignore-whitespace', '--whitespace=fix']
patch_args = ['-flp1', '-i']


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_fail(str):
    print(bcolors.FAIL + str + bcolors.ENDC)

def print_warn(str):
    print(bcolors.WARNING + str + bcolors.ENDC)

def print_success(str):
    print(bcolors.OKGREEN + str + bcolors.ENDC)

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


def findMSBuild():
    filename = 'MSBuild.exe'
    for root, _, files in os.walk(findVSLatestDir() + r'\MSBuild'):
        if filename in files:
            return os.path.join(root, filename)


def getVSEnv(arch='x64', platform='', version='10.0.15063.0'):
    env_cmd = getVSEnvCmd(arch, platform, version) + " && set"
    p = subprocess.Popen(env_cmd,
                         shell=True,
                         stdout=subprocess.PIPE)
    stdout, _ = p.communicate()
    out = stdout.decode('utf-8').split("\r\n")[5:-1]
    # override
    return dict(s.split('=') for s in out)

def getVSEnvCmd(arch='x64', platform='', version='10.0.15063.0'):
    vcEnvInit = [findVSLatestDir() + r'\VC\Auxiliary\Build\"vcvarsall.bat']
    if platform != '':
        args = [arch, platform, version]
    else:
        args = [arch, version]
    if args:
        vcEnvInit.extend(args)
    vcEnvInit = 'call \"' + ' '.join(vcEnvInit)
    return vcEnvInit

def make(pkg_info, force):
    pkg_name = pkg_info['name']
    version = pkg_info['version']
    pkg_up_to_date = False
    # attempt to get the current built version
    current_version = ''
    # check build file for current version
    build_file = contrib_build_dir + r'\\.' + pkg_name
    if os.path.exists(build_file):
        if force:
            os.remove(build_file)
        else:
            with open(build_file, 'r+') as f:
                current_version = f.read()
                if current_version == pkg_info['version']:
                    pkg_up_to_date = True
    for dep in pkg_info['deps']:
        dep_build_dep = resolve(dep)
        if dep_build_dep:
            pkg_up_to_date = False
        else:
            print_success(dep + ' up to date')
    if not pkg_up_to_date or current_version is None or force:
        if not current_version is '':
            print(pkg_name + ' currently @: ' + current_version)
        if force:
            print('Forcing fetch/patch/build for ' + pkg_name)
        should_fetch = not pkg_up_to_date
        pkg_build_path = contrib_build_dir + '\\' + pkg_name
        should_fetch &= not os.path.exists(pkg_build_path)
        if not pkg_up_to_date or force:
            if not force and not current_version is None:
                print_warn(pkg_name + ' is not up to date')
            if should_fetch or force and fetch_pkg(pkg_name, version, pkg_info['url'], force):
                apply(pkg_name, pkg_info['patches'], pkg_info['win-patches'])
        if build(pkg_name, version,
                 pkg_info['project-paths'], pkg_info['custom-scripts'],
                 pkg_info['with_env']):
            track_build(pkg_name, version)
        else:
            print_fail("Couldn't build contrib " + pkg_name)
            exit(1)
        return True
    # did not need build
    return False


def fetch_pkg(pkg_name, version, url, force):
    version_replace = re.compile(re.escape('__VERSION__'))
    full_url = version_replace.sub(version, url)
    if not full_url:
        print(pkg_name + ' missing url in package configuration')
        return False
    archive_name = full_url[full_url.rfind("/") + 1:]
    archive_path = contrib_tmp_dir + '\\' + archive_name
    if not os.path.exists(archive_path):
        print('Fetching ' + pkg_name + ' from: ' + full_url)
        args = [full_url, '-P', contrib_tmp_dir]
        args.extend(wget_args)
        dl_result = getSHrunner().exec_batch('wget', args)
        if dl_result[0] is not 0:
            print('Wget failure. Using powershell Invoke-WebRequest instead')
            args = ['-Uri', full_url, '-OutFile', archive_path]
            dl_result = getSHrunner().exec_ps1('Invoke-WebRequest', args)
        return extract_archive(pkg_name, archive_name, archive_path)
    else:
        print(archive_name + ' already exists in the tarball/archive directory')
        decomp_result = extract_archive(pkg_name, archive_name, archive_path)
        if not decomp_result and force:
            print('Removing old tarball for ' + archive_name)
            getSHrunner().exec_batch('del', ['/s', '/q', archive_name])
            return fetch_pkg(pkg_name, version, url, False)
        else:
            return True
    return False
    
def remove_archive_if_needed(pkg_build_path, dirty_path):
    if os.path.exists(pkg_build_path):
        print('Removing old package ' + pkg_build_path)
        getSHrunner().exec_batch('rmdir', ['/s', '/q', pkg_build_path])
    elif os.path.exists(dirty_path):
        print('Removing partial decompression ' + dirty_path)
        getSHrunner().exec_batch('rmdir', ['/s', '/q', dirty_path])

def extract_tar(pkg_build_path, name, path):
    with tarfile.open(path, 'r') as tarball:
        tar_common_prefix = os.path.commonprefix(tarball.getnames())
        dirty_path = contrib_build_dir + '\\' + tar_common_prefix
        remove_archive_if_needed(pkg_build_path, dirty_path)
        print('[TAR] Decompressing ' + name + ' to ' + pkg_build_path)
        tarball.extractall(contrib_build_dir)
        os.rename(contrib_build_dir + '\\' + tar_common_prefix,
                  pkg_build_path)
        return True
    return False

def extract_zip(pkg_build_path, name, path):
    with zipfile.ZipFile(path, 'r') as ziparchive:
        zip_common_prefix = os.path.commonprefix(ziparchive.namelist())
        dirty_path = contrib_build_dir + '\\' + zip_common_prefix
        remove_archive_if_needed(pkg_build_path, dirty_path)
        print('[ZIP] Decompressing ' + name + ' to ' + pkg_build_path)
        ziparchive.extractall(contrib_build_dir)
        os.rename(contrib_build_dir + '\\' + zip_common_prefix,
                  pkg_build_path)
        return True
    return False

def extract_archive(pkg_name, name, path):
    pkg_build_path = contrib_build_dir + '\\' + pkg_name
    if tarfile.is_tarfile(path):
        return extract_tar(pkg_build_path, name, path)
    elif zipfile.is_zipfile(path):
        return extract_zip(pkg_build_path, name, path)

def apply_linux(patch_path):
    print('applying linux patch ' + patch_path)
    args = []
    args.extend(patch_args)
    args.append(patch_path)
    return getSHrunner().exec_sh('patch', args)

def apply_windows(patch_path):
    print('applying windows patch ' + patch_path)
    args = []
    args.extend(git_apply_args)
    args.append(patch_path)
    return getSHrunner().exec_batch('git', args)

def apply(pkg_name, patches, win_patches):
    print('patching ' + pkg_name + '...')
    tmp_dir = os.getcwd()
    pkg_build_path = contrib_build_dir + '\\' + pkg_name
    if not os.path.exists(pkg_build_path):
        os.makedirs(pkg_build_path)
    os.chdir(pkg_build_path)
    base_sh_src_path = get_sh_path(contrib_src_dir)

    # 1. git patches (LF)
    for p in patches:
        patch_path = base_sh_src_path + '/' + pkg_name + '/' + p
        result = apply_linux(patch_path)
        if result[0]:
            print_fail('Couldn\'t apply patch: ' + patch_path)
            exit(1)

    # 2. windows git patches (CR/LF)
    for wp in win_patches:
        patch_path = contrib_src_dir + '\\' + pkg_name + '\\' + wp
        result = apply_windows(patch_path)
        if result[0]:
            print_fail('Couldn\'t apply patch: ' + patch_path)
            exit(1)

    os.chdir(tmp_dir)


def resolve(pkg_name, force=False):
    print('Resolving: ' + pkg_name)
    pkg_json_file = daemon_dir + r'\contrib\src\\' + pkg_name + r"\\package.json"
    if not os.path.exists(pkg_json_file):
        print_warn("No package info for " + pkg_name)
        raise Exception()
    with open(pkg_json_file) as json_file:
        pkg_info = json.load(json_file)
        try:
            return make(pkg_info, force)
        except Exception:
            print_fail("Make " + pkg_name + ' failed!')
            sys.exit(1)


def track_build(pkg_name, version):
    build_file = contrib_build_dir + '\\.' + pkg_name
    f = open(build_file, "w+")
    f.write(version)
    f.close()


def build(pkg_name, version, project_paths, custom_scripts, with_env):
    getMSbuilder().set_msbuild_configuration(with_env, 'x64', 'Release')
    if with_env == 'true':
        getMSbuilder().setup_vs_env()
    
    success = True
    build_operations = 0
    tmp_dir = os.getcwd()
    os.chdir(contrib_build_dir + '\\' + pkg_name)

    # pre-build custom step (CMake...)
    pre_build_scripts = custom_scripts["pre-build"]
    if pre_build_scripts:
        print('Pre-build phase')
    for script in pre_build_scripts:
        result = getSHrunner().exec_batch(script)
        success &= not result[0]
        build_operations += 1

    # build custom step (nmake...)
    build_scripts = custom_scripts["build"]
    if build_scripts:
        print('Custom Build phase')
    for script in build_scripts:
        result = getSHrunner().exec_batch(script)
        success &= not result[0]
        build_operations += 1

    # vcxproj files
    if project_paths:
        print('Msbuild phase')
    for pp in project_paths:
        project_full_path = contrib_build_dir + '\\' + pkg_name + '\\' + pp
        print('[MSBUILD] Building ' + pkg_name + ' @ ' + version + ' using '
              +  project_full_path)
        getMSbuilder().build(pkg_name, project_full_path)
        build_operations += 1

    os.chdir(tmp_dir)

    # should cover header only, no cmake, etc
    total_ops =  len(build_scripts) + len(project_paths) + len(pre_build_scripts)
    return success and build_operations == total_ops


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
        self.cmd_env_vars = os.environ.copy()
        self.cmd_env_vars.update(extra_env_vars)
        self.cmd_env_vars.update({'VCVARSALL_CMD': getVSEnvCmd()})

    def exec_script(self, script_type=ScriptType.cmd, script=None, args=[]):
        if script_type is ScriptType.cmd:
            cmd = [script]
            if not args:
                cmd = shlex.split(script)
        elif script_type is ScriptType.ps1:
            if not os.path.exists(self.ps_path):
                raise IOError('Powershell not found at %s.' % self.ps_path)
            cmd = [self.ps_path, '-ExecutionPolicy', 'ByPass', script]
        elif script_type is ScriptType.sh:
            if not os.path.exists(self.sh_path):
                raise IOError('Bash not found at %s.' % self.ps_path)
            cmd = [self.sh_path, '-c ', '\"' + script]
        else:
            print('not implemented')
            return 1
        if args:
            cmd.extend(args)
        if script_type is ScriptType.sh:
            cmd[-1] = cmd[-1] + '\"'
            cmd = " ".join(cmd)
        print(cmd)
        #print(self.cmd_env_vars)
        p = subprocess.Popen(cmd,
                             shell=True,
                             stderr=sys.stderr,
                             stdout=sys.stdout,
                             env=self.cmd_env_vars)
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
    def __init__(self):
        self.vsenv_done = False
        self.msbuild = findMSBuild()
        self.default_msbuild_args = [
            '/nologo',
            '/verbosity:normal',
            '/maxcpucount:' + str(multiprocessing.cpu_count())]
        self.set_msbuild_configuration()

    def set_msbuild_configuration(self, with_env='false', arch='x64', configuration='Release'):
        self.extra_msbuild_args = [
            '/p:Platform=' + arch,
            '/p:Configuration=' + configuration,
            '/p:useenv=' + with_env
        ]

    def build(self, pkg_name, proj_path):
        if not os.path.isfile(self.msbuild):
            raise IOError('msbuild.exe not found. path=' + self.msbuild)
        args = []
        args.extend(self.default_msbuild_args)
        args.extend(self.extra_msbuild_args)
        args.append(proj_path)
        result = getSHrunner().exec_batch(self.msbuild, args)
        if result[0] == 1:
            print_fail("Build failed when building " + pkg_name)
            sys.exit(1)

    def setup_vs_env(self):
        if self.vsenv_done:
            print('vs environment already initialized')
            return
        print('Setting up vs environment')
        getSHrunner().cmd_env_vars.update(getVSEnv())   
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
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.tar.*'])
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.tgz.*'])
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.zip.*'])
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
        resolve(parsed_args.build, parsed_args.force)
        print_success(parsed_args.build + ' up to date')


def get_sh_path(path):
    driveless_path = path.replace(os.path.sep, '/')[3:]
    drive_letter = os.path.splitdrive(daemon_dir)[0][0].lower()
    wsl_drive_path = '/mnt/' + drive_letter + '/' 
    no_echo = ' &> /dev/null'
    result = getSHrunner().exec_sh('pwd | grep ' + wsl_drive_path + no_echo)
    if result[0]:
        # using git bash
        return '/' + drive_letter + '/' + driveless_path
     # using wsl
    return wsl_drive_path + driveless_path

def validate_pair(ob):
    try:
        if not (len(ob) == 2):
            print("Unexpected result:", ob, file=sys.stderr)
            raise ValueError
    except:
        return False
    return True

import itertools

def consume(iter):
    try:
        while True: next(iter)
    except StopIteration:
        pass

if __name__ == '__main__':
    #print(getVSEnvCmd())
    main()
