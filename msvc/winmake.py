import sys
import os
import subprocess
import platform
import argparse
import json
import re
import zipfile
import tarfile
import multiprocessing
import shutil
import shlex
import glob
import time
from datetime import timedelta
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# project paths
daemon_msvc_dir = os.getcwd()
daemon_dir = os.path.dirname(daemon_msvc_dir)
contrib_src_dir = daemon_dir + r'\contrib\src'
contrib_build_dir = daemon_dir + r'\contrib\build'
contrib_tmp_dir = daemon_dir + r'\contrib\tarballs'

# SCM
wget_args = [
    '--no-check-certificate', '--retry-connrefused',
    '--waitretry=1', '--read-timeout=20',
    '--timeout=15', '--tries=4']
git_apply_args = ['apply', '--reject',
                  '--ignore-whitespace', '--whitespace=fix']
patch_args = ['-flp1', '-i']

# vs help
vs_where_path = os.path.join(
    os.environ['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe'
)

host_is_64bit = (False, True)[platform.machine().endswith('64')]


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_cmd(str):
    p = subprocess.Popen('echo ' + str,
                         shell=True)
    p.communicate()

def print_fail(str):
    print_cmd(bcolors.FAIL + str + bcolors.ENDC)


def print_warn(str):
    print_cmd(bcolors.WARNING + str + bcolors.ENDC)


def print_success(str, bold=False):
    start = bcolors.BOLD + bcolors.OKGREEN if bold else bcolors.OKBLUE
    print_cmd(start + str + bcolors.ENDC)


def shellquote(s, windows=False):
    if not windows:
        return "'" + s.replace("'", "'\''") + "'"
    else:
        return '\"' + s + '\"'


def getLatestVSVersion():
    args = [
        '-latest',
        '-products *',
        '-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property installationVersion'
    ]
    cmd = [vs_where_path] + args
    output = subprocess.check_output(' '.join(cmd)).decode('utf-8')
    if output:
        return output.splitlines()[0].split('.')[0]
    else:
        return


def findVSLatestDir():
    args = [
        '-latest',
        '-products *',
        '-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property installationPath'
    ]
    cmd = [vs_where_path] + args
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


def getVSEnv(arch='x64', platform='', version='10.0.16299.0'):
    env_cmd = getVSEnvCmd(arch, platform, version) + " && set"
    p = subprocess.Popen(env_cmd,
                         shell=True,
                         stdout=subprocess.PIPE)
    stdout, _ = p.communicate()
    out = stdout.decode('utf-8').split("\r\n")[5:-1]
    return dict(s.split('=') for s in out)


def getCMakeGenerator(vs_version):
    if vs_version == '15':
        return '\"Visual Studio 15 2017 Win64\"'
    else:
        return '\"Visual Studio ' + vs_version + ' 2019\"'


def getVSEnvCmd(arch='x64', platform='', version='10.0.16299.0'):
    vcEnvInit = [findVSLatestDir() + r'\VC\Auxiliary\Build\"vcvarsall.bat']
    if platform != '':
        args = [arch, platform, version]
    else:
        args = [arch, version]
    if args:
        vcEnvInit.extend(args)
    vcEnvInit = 'call \"' + ' '.join(vcEnvInit)
    return vcEnvInit


def make_daemon(pkg_info, force):
    for dep in pkg_info.get('deps', []):
        resolve(dep)
    build('daemon', daemon_msvc_dir,
          pkg_info.get('project-paths', []),
          pkg_info.get('custom-scripts', {}),
          pkg_info.get('with_env', ''),
          conf=pkg_info.get('configuration', 'Release'))


def make(pkg_info, force):
    pkg_name = pkg_info.get('name')
    if pkg_name == 'daemon':
        return make_daemon(pkg_info, force)
    version = pkg_info.get('version')
    pkg_build_uptodate = False
    pkg_ver_uptodate = False
    # attempt to get the current built version
    current_version = ''
    # check build file for current version
    build_file = contrib_build_dir + r'\\.' + pkg_name
    if os.path.exists(build_file):
        if force:
            os.remove(build_file)
        else:
            pkg_build_uptodate = is_build_uptodate(pkg_name, build_file)
            with open(build_file, 'r+') as f:
                current_version = f.read()
                if current_version == version:
                    pkg_ver_uptodate = True
    for dep in pkg_info.get('deps', []):
        dep_build_dep = resolve(dep)
        if dep_build_dep:
            pkg_build_uptodate = False
    pkg_up_to_date = pkg_build_uptodate & pkg_ver_uptodate
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
            if (should_fetch or force) and fetch_pkg(pkg_name, version, pkg_info['url'], force):
                apply(pkg_name, pkg_info.get('patches', []),
                      pkg_info.get('win-patches', []))
        if build(pkg_name, contrib_build_dir + '\\' + pkg_name,
                 pkg_info.get('project-paths', []),
                 pkg_info.get('custom-scripts', {}),
                 pkg_info.get('with_env', '')):
            track_build(pkg_name, version)
        else:
            print_fail("Couldn't build contrib " + pkg_name)
            exit(1)
        print_success(pkg_name + ' up to date')
        return True
    # did not need build
    print_success(pkg_name + ' already up to date')
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
            print('[FETCH] Wget failure. Using powershell Invoke-WebRequest instead')
            args = ['-Uri', full_url, '-OutFile', archive_path]
            dl_result = getSHrunner().exec_ps1('Invoke-WebRequest', args)
        return extract_archive(pkg_name, archive_name, archive_path)
    else:
        print('[FETCH] ' + archive_name +
              ' already exists in the tarball/archive directory')
        decomp_result = extract_archive(pkg_name, archive_name, archive_path)
        if not decomp_result and force:
            print('[FETCH] Removing old tarball for ' + archive_name)
            getSHrunner().exec_batch('del', ['/s', '/q', archive_name])
            return fetch_pkg(pkg_name, version, url, False)
        else:
            return True
    return False


def remove_archive_if_needed(pkg_build_path, dirty_path):
    if os.path.exists(pkg_build_path):
        print('[FETCH] Removing old package ' + pkg_build_path)
        getSHrunner().exec_batch('rmdir', ['/s', '/q', pkg_build_path])
    elif os.path.exists(dirty_path):
        print('[FETCH] Removing partial decompression ' + dirty_path)
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
    print('[PATCH] applying linux patch ' + patch_path)
    args = []
    args.extend(patch_args)
    args.append(patch_path)
    return getSHrunner().exec_sh('patch', args)


def apply_windows(patch_path):
    print('[PATCH] applying windows patch ' + patch_path)
    args = []
    args.extend(git_apply_args)
    args.append(patch_path)
    return getSHrunner().exec_batch('git', args)


def apply(pkg_name, patches, win_patches):
    print('[PATCH] patching ' + pkg_name + '...')
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


def get_pkg_file(pkg_name):
    if pkg_name == 'daemon':
        pkg_location = daemon_msvc_dir
    else:
        pkg_location = daemon_dir + r'\contrib\src\\' + pkg_name
    pkg_json_file = pkg_location + r"\\package.json"
    if not os.path.exists(pkg_json_file):
        print_fail("No package info for " + pkg_name)
        sys.exit(1)
    return pkg_json_file


def resolve(pkg_name, force=False):
    pkg_json_file = get_pkg_file(pkg_name)
    with open(pkg_json_file) as json_file:
        print('Resolving: ' + pkg_name)
        pkg_info = json.load(json_file)
        try:
            return make(pkg_info, force)
        except Exception as e:
            # logger.error('Make ' + pkg_name + ' failed!', exc_info=True)
            print(e)
            print_fail('Make ' + pkg_name + ' failed!')
            sys.exit(1)


def track_build(pkg_name, version):
    build_file = contrib_build_dir + '\\.' + pkg_name
    f = open(build_file, "w+")
    f.write(version)
    f.close()


def build(pkg_name, pkg_dir, project_paths, custom_scripts, with_env, arch='x64', conf='Release'):
    getMSbuilder().set_msbuild_configuration(with_env, arch, conf)
    if with_env is not '':
        getMSbuilder().setup_vs_env(with_env)

    success = True
    build_operations = 0
    tmp_dir = os.getcwd()
    os.chdir(pkg_dir)

    # pre-build custom step (CMake...)
    pre_build_scripts = custom_scripts.get("pre-build", [])
    if pre_build_scripts:
        print('Pre-build phase')
    for script in pre_build_scripts:
        result = getSHrunner().exec_batch(script)
        success &= not result[0]
        build_operations += 1

    # build custom step (nmake...)
    build_scripts = custom_scripts.get("build", [])
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
        project_full_path = pkg_dir + '\\' + pp
        print('[MSBUILD] Building ' + pkg_name)
        getMSbuilder().build(pkg_name, project_full_path)
        build_operations += 1

    os.chdir(tmp_dir)

    # should cover header only, no cmake, etc
    ops = len(build_scripts) + len(project_paths) + len(pre_build_scripts)
    return success and build_operations == ops


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
        sys_path = (r'\Sysnative', r'\system32')[host_is_64bit]
        full_sys_path = os.path.expandvars('%systemroot%') + sys_path

        # powershell
        self.ps_path = full_sys_path + r'\WindowsPowerShell\v1.0\powershell.exe'
        if not os.path.exists(self.ps_path):
            raise IOError('Powershell not found at %s.' % self.ps_path)

        # bash
        self.sh_path = full_sys_path + r'\bash.exe'
        if not os.path.exists(self.sh_path):
            print('Bash not found at ' + self.sh_path)
            self.sh_path = shutil.which('bash.exe')
            if not os.path.exists(self.sh_path):
                raise IOError('No bash found')
            else:
                self.sh_path = shellquote(self.sh_path, windows=True)
                print('Using alternate bash found at ' + self.sh_path)

        self.project_env_vars = {
            'DAEMON_DIR': daemon_dir,
            'CONTRIB_SRC_DIR': contrib_src_dir,
            'CONTRIB_BUILD_DIR': contrib_build_dir,
            'VCVARSALL_CMD': getVSEnvCmd(),
            'CMAKE_GENERATOR': getCMakeGenerator(getLatestVSVersion())
        }
        self.base_env_vars = self.project_env_vars.copy()
        self.base_env_vars.update(os.environ.copy())
        self.vs_env_vars = {}

    def set_vs_env_vars(self, env_target):
        self.vs_env_vars = {}
        self.vs_env_vars = self.project_env_vars.copy()
        self.vs_env_vars.update(getVSEnv(version=env_target))

    def exec_script(self, script_type=ScriptType.cmd, script=None, args=[]):
        if script_type is ScriptType.cmd:
            cmd = [script]
            if not args:
                cmd = shlex.split(script)
        elif script_type is ScriptType.ps1:
            cmd = [self.ps_path, '-ExecutionPolicy', 'ByPass', script]
        elif script_type is ScriptType.sh:
            cmd = [self.sh_path, '-c ', '\"' + script]
        else:
            print('not implemented')
            return 1
        if args:
            cmd.extend(args)
        if script_type is ScriptType.sh:
            cmd[-1] = cmd[-1] + '\"'
            cmd = " ".join(cmd)
        p = subprocess.Popen(cmd,
                             shell=True,
                             stderr=sys.stderr,
                             stdout=sys.stdout,
                             env=self.vs_env_vars if self.vs_env_vars else self.base_env_vars)
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

    def force_vs_project_value(self, proj_path, prop_name, value):
        path = get_sh_path(proj_path.replace('\\', '/'))
        args = ["-i",
                "'/<" + prop_name + "/c\\\\<" + prop_name +
                ">" + value + "</" + prop_name + ">'",
                shellquote(path)]
        getSHrunner().exec_sh("sed", args)

    def build(self, pkg_name, proj_path):
        # force default/latest toolset/sdk
        self.force_vs_project_value(proj_path,
                                    'WindowsTargetPlatformVersion',
                                    '\\$(LatestTargetPlatformVersion)')
        self.force_vs_project_value(proj_path,
                                    'PlatformToolset',
                                    '\\$(DefaultPlatformToolset)')
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

    def setup_vs_env(self, env_target):
        if self.vsenv_done:
            print('vs environment already initialized')
            return
        print('Setting up vs environment')
        getSHrunner().set_vs_env_vars(env_target)
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
        '-c', '--clean',
        help='Cleans out build directory for a contrib')
    ap.add_argument(
        '-p', '--purge', action='store_true',
        help='Cleans out contrib tarball directory')

    parsed_args = ap.parse_args()

    return parsed_args


def main():
    start_time = time.time()

    if not host_is_64bit:
        print('These scripts will only run on a 64-bit Windows system for now!')
        sys.exit(1)

    if int(getLatestVSVersion()) < 15:
        print('These scripts require at least Visual Studio v15 2017!')
        sys.exit(1)

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
        if os.path.exists(contrib_build_dir) and parsed_args.clean == 'all':
            print('Removing contrib builds ' + contrib_build_dir)
            getSHrunner().exec_batch('rmdir', ['/s', '/q', contrib_build_dir])
        else:
            pkg_json_file = get_pkg_file(parsed_args.clean)
            with open(pkg_json_file) as json_file:
                pkg_info = json.load(json_file)
                dir_to_clean = contrib_build_dir + '\\' + pkg_info['name']
                file_to_clean = contrib_build_dir + '\\.' + pkg_info['name']
                if os.path.exists(dir_to_clean) or os.path.exists(file_to_clean):
                    print('Removing contrib build ' + dir_to_clean)
                    getSHrunner().exec_batch('rmdir', ['/s', '/q', dir_to_clean])
                    getSHrunner().exec_batch(
                        'del', ['/s', '/f', '/q', file_to_clean])
                else:
                    print('No builds to remove')

    if parsed_args.build:
        if not os.path.exists(contrib_build_dir):
            os.makedirs(contrib_build_dir)
        if parsed_args.build == 'all':
            resolve('asio', parsed_args.force)
            resolve('ffmpeg', parsed_args.force)
            resolve('opendht', parsed_args.force)
            resolve('pjproject', parsed_args.force)
            resolve('portaudio', parsed_args.force)
            resolve('secp256k1', parsed_args.force)
            resolve('upnp', parsed_args.force)
            resolve('yaml-cpp', parsed_args.force)
        else:
            resolve(parsed_args.build, parsed_args.force)
        print_success('make ' + parsed_args.build + ' done', True)

    print("--- took %s ---" % secondsToStr(time.time() - start_time))


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


def newest_file(root):
    file_list = []
    for path, _, files in os.walk(root):
        for name in files:
            file_list.append(os.path.join(path, name))
    latest_file = max(file_list, key=os.path.getmtime)
    return latest_file


def is_build_uptodate(pkg_name, build_file):
    root = contrib_build_dir + '\\' + pkg_name
    file_list = []
    for path, _, files in os.walk(root):
        for name in files:
            file_list.append(os.path.join(path, name))
    if not file_list:
        return False
    latest_file = max(file_list, key=os.path.getmtime)
    t_mod = os.path.getmtime(latest_file)
    t_build = os.path.getmtime(build_file)
    return t_mod < t_build


def secondsToStr(elapsed=None):
    return str(timedelta(seconds=elapsed))


if __name__ == '__main__':
    main()
