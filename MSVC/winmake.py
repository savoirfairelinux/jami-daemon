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
import struct
import importlib
import logging
import traceback
import re
import fileinput

root_logger = logging.getLogger(__name__)
log = None

# project paths
daemon_msvc_dir = os.path.dirname(os.path.realpath(__file__))
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
win_sdk_default = '10.0.16299.0'
win_toolset_default = 'v141'

vs_where_path = os.path.join(
    os.environ['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe'
)

host_is_64bit = (False, True)[platform.machine().endswith('64')]
python_is_64bit = (False, True)[8 * struct.calcsize("P") == 64]


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
    for root, _, files in os.walk(findVSLatestDir() + r'\\MSBuild'):
        if filename in files:
            return os.path.join(root, filename)


def getVSEnv(arch='x64', platform='', version=''):
    env_cmd = 'set path=%path:"=% && ' + \
        getVSEnvCmd(arch, platform, version) + ' && set'
    p = subprocess.Popen(env_cmd,
                         shell=True,
                         stdout=subprocess.PIPE)
    stdout, _ = p.communicate()
    out = stdout.decode('utf-8').split("\r\n")[5:-1]
    return dict(s.split('=', 1) for s in out)


def getCMakeGenerator(vs_version):
    if vs_version == '15':
        return '\"Visual Studio 15 2017 Win64\"'
    else:
        return '\"Visual Studio ' + vs_version + ' 2019\"'


def getVSEnvCmd(arch='x64', platform='', version=''):
    vcEnvInit = [findVSLatestDir() + r'\VC\Auxiliary\Build\"vcvarsall.bat']
    if platform != '':
        args = [arch, platform, version]
    else:
        args = [arch, version]
    if args:
        vcEnvInit.extend(args)
    vcEnvInit = 'call \"' + ' '.join(vcEnvInit)
    return vcEnvInit


def make_daemon(pkg_info, force, sdk_version, toolset):
    for dep in pkg_info.get('deps', []):
        resolve(dep, False, sdk_version, toolset)
    root_logger.warning(
        "Building daemon with preferred sdk version %s and toolset %s", sdk_version, toolset)
    env_set = 'false' if pkg_info.get('with_env', '') == '' else 'true'
    sdk_to_use = sdk_version if env_set == 'false' else pkg_info.get('with_env', '')
    build('daemon', daemon_msvc_dir,
          pkg_info.get('project_paths', []),
          pkg_info.get('custom_scripts', {}),
          env_set,
          sdk_to_use,
          toolset,
          conf=pkg_info.get('configuration', 'Release'))


def make(pkg_info, force, sdk_version, toolset):
    pkg_name = pkg_info.get('name')
    if pkg_name == 'daemon':
        return make_daemon(pkg_info, force, sdk_version, toolset)
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
        dep_build_dep = resolve(dep, False, sdk_version, toolset)
        if dep_build_dep:
            pkg_build_uptodate = False
    pkg_up_to_date = pkg_build_uptodate & pkg_ver_uptodate
    if not pkg_up_to_date or current_version is None or force:
        if not current_version is '':
            log.debug(pkg_name + ' currently @: ' + current_version)
        if force:
            log.debug('Forcing fetch/patch/build for ' + pkg_name)
        should_fetch = not pkg_up_to_date
        pkg_build_path = contrib_build_dir + '\\' + pkg_name
        should_fetch &= not os.path.exists(pkg_build_path)
        if not pkg_up_to_date or force:
            if not force and not current_version is None:
                log.warning(pkg_name + ' is not up to date')
            if (should_fetch or force) and fetch_pkg(pkg_name, version, pkg_info['url'], force):
                apply(pkg_name, pkg_info.get('patches', []),
                      pkg_info.get('win_patches', []))
        env_set = 'false' if pkg_info.get('with_env', '') != '' else 'true'
        sdk_to_use = sdk_version if env_set == 'false' else pkg_info.get('with_env', '')
        if build(pkg_name,
                 contrib_build_dir + '\\' + pkg_name,
                 pkg_info.get('project_paths', []),
                 pkg_info.get('custom_scripts', {}),
                 env_set,
                 sdk_to_use,
                 toolset):
            track_build(pkg_name, version)
        else:
            log.error("Couldn't build contrib " + pkg_name)
            exit(1)
        log.info(pkg_name + ' up to date')
        return True
    # did not need build
    log.info(pkg_name + ' already up to date')
    return False


def fetch_pkg(pkg_name, version, url, force):
    version_replace = re.compile(re.escape('__VERSION__'))
    full_url = version_replace.sub(version, url)
    if not full_url:
        log.error(pkg_name + ' missing url in package configuration')
        return False
    archive_name = full_url[full_url.rfind("/") + 1:]
    archive_path = contrib_tmp_dir + '\\' + archive_name
    if not os.path.exists(archive_path):
        log.debug('Fetching ' + pkg_name + ' from: ' + full_url)
        args = [full_url, '-P', contrib_tmp_dir]
        args.extend(wget_args)
        dl_result = getSHrunner().exec_batch('wget', args)
        if dl_result[0] is not 0:
            log.warning(
                'wget failure. Using powershell Invoke-WebRequest instead')
            args = ['-Uri', full_url, '-OutFile', archive_path]
            dl_result = getSHrunner().exec_ps1('Invoke-WebRequest', args)
        return extract_archive(pkg_name, archive_name, archive_path)
    else:
        log.warning(archive_name +
                    ' already exists in the tarball/archive directory')
        decomp_result = extract_archive(pkg_name, archive_name, archive_path)
        if not decomp_result and force:
            log.debug('Removing old tarball for ' + archive_name)
            getSHrunner().exec_batch('del', ['/s', '/q', archive_name])
            return fetch_pkg(pkg_name, version, url, False)
        else:
            return True
    return False


def remove_archive_if_needed(pkg_build_path, dirty_path):
    if os.path.exists(pkg_build_path):
        log.debug('Removing old package ' + pkg_build_path)
        getSHrunner().exec_batch('rmdir', ['/s', '/q', pkg_build_path])
    elif os.path.exists(dirty_path):
        log.debug('Removing partial decompression ' + dirty_path)
        getSHrunner().exec_batch('rmdir', ['/s', '/q', dirty_path])


def extract_tar(pkg_build_path, name, path):
    with tarfile.open(path, 'r') as tarball:
        tar_common_prefix = os.path.commonprefix(tarball.getnames())
        dirty_path = contrib_build_dir + '\\' + tar_common_prefix
        remove_archive_if_needed(pkg_build_path, dirty_path)
        log.debug('Decompressing ' + name + ' to ' + pkg_build_path)
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
        log.debug('Decompressing ' + name + ' to ' + pkg_build_path)
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
    log.debug('applying linux patch ' + patch_path)
    args = []
    args.extend(patch_args)
    args.append(patch_path)
    return getSHrunner().exec_sh('patch', args)


def apply_windows(patch_path):
    log.debug('applying windows patch ' + patch_path)
    args = []
    args.extend(git_apply_args)
    args.append(patch_path)
    return getSHrunner().exec_batch('git', args)


def apply(pkg_name, patches, win_patches):
    log.debug('patching ' + pkg_name + '...')
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
            log.error('Couldn\'t apply patch: ' + patch_path)
            exit(1)

    # 2. windows git patches (CR/LF)
    for wp in win_patches:
        patch_path = contrib_src_dir + '\\' + pkg_name + '\\' + wp
        result = apply_windows(patch_path)
        if result[0]:
            log.error('Couldn\'t apply patch: ' + patch_path)
            exit(1)

    os.chdir(tmp_dir)


def get_pkg_file(pkg_name):
    if pkg_name == 'daemon':
        pkg_location = daemon_msvc_dir
    else:
        pkg_location = daemon_dir + r'\contrib\src\\' + pkg_name
    pkg_json_file = pkg_location + r"\\package.json"
    if not os.path.exists(pkg_json_file):
        log.error("No package info for " + pkg_name)
        sys.exit(1)
    return pkg_json_file


def resolve(pkg_name, force=False, sdk_version='', toolset=''):
    pkg_json_file = get_pkg_file(pkg_name)
    with open(pkg_json_file) as json_file:
        log.info('Resolving: ' + pkg_name)
        pkg_info = json.load(json_file)
        try:
            return make(pkg_info, force, sdk_version, toolset)
        except Exception as e:
            print(e)
            log.error('Make ' + pkg_name + ' failed!')
            sys.exit(1)


def track_build(pkg_name, version):
    build_file = contrib_build_dir + '\\.' + pkg_name
    f = open(build_file, "w+")
    f.write(version)
    f.close()


def build(pkg_name, pkg_dir, project_paths, custom_scripts, with_env, sdk,
          toolset, arch='x64', conf='Release'):
    getMSbuilder().set_msbuild_configuration(with_env, arch, conf, toolset)
    getMSbuilder().setup_vs_env(sdk)

    success = True
    build_operations = 0
    tmp_dir = os.getcwd()
    os.chdir(pkg_dir)

    # pre_build custom step (CMake...)
    pre_build_scripts = custom_scripts.get("pre_build", [])
    if pre_build_scripts:
        log.debug('Pre_build phase')
    for script in pre_build_scripts:
        result = getSHrunner().exec_batch(script)
        success &= not result[0]
        build_operations += 1

    # build custom step (nmake...)
    build_scripts = custom_scripts.get("build", [])
    if build_scripts:
        log.debug('Custom Build phase')
    for script in build_scripts:
        result = getSHrunner().exec_batch(script)
        success &= not result[0]
        build_operations += 1

    # vcxproj files
    if project_paths:
        log.debug('Msbuild phase')
    for pp in project_paths:
        project_full_path = pkg_dir + '\\' + pp
        log.debug('Building: ' + pkg_name + " with sdk version " +
                  sdk + " and toolset " + toolset)
        getMSbuilder().build(pkg_name, project_full_path, sdk, toolset)
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
        sys_path = (r'\Sysnative', r'\system32')[python_is_64bit]
        full_sys_path = os.path.expandvars('%systemroot%') + sys_path

        # powershell
        self.ps_path = full_sys_path + r'\WindowsPowerShell\v1.0\powershell.exe'
        if not os.path.exists(self.ps_path):
            log.error('Powershell not found at %s.' % self.ps_path)
            sys.exit(1)

        # bash
        if not os.environ.get('JENKINS_URL'):
            self.sh_path = full_sys_path + r'\bash.exe'
        else:
            self.sh_path = '\"C:\Program Files\Git\git-bash.exe\"'

        if not os.path.exists(self.sh_path):
            log.warning('Bash not found at ' + self.sh_path)
            self.sh_path = shutil.which('bash.exe')
            if not os.path.exists(self.sh_path):
                log.error('No bash found')
                sys.exit(1)
            else:
                self.sh_path = shellquote(self.sh_path, windows=True)
                log.debug('Using alternate bash found at ' + self.sh_path)

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

    def set_msbuild_configuration(self, with_env='false', arch='x64',
                                  configuration='Release',
                                  toolset=win_toolset_default):
        self.extra_msbuild_args = [
            '/p:Platform=' + arch,
            '/p:Configuration=' + configuration,
            '/p:PlatformToolset=' + toolset,
            '/p:useenv=' + with_env
        ]

    @staticmethod
    def replace_vs_prop(filename, prop, val):
        p = re.compile(r'(?s)<' + prop + r'\s?.*?>(.*?)<\/' + prop + r'>')
        val = r'<' + prop + r'>' + val + r'</' + prop + r'>'
        with fileinput.FileInput(filename, inplace=True) as file:
            for line in file:
                print(re.sub(p, val, line), end='')

    def build(self, pkg_name, proj_path, sdk_version, toolset):
        if not os.path.isfile(self.msbuild):
            raise IOError('msbuild.exe not found. path=' + self.msbuild)
        # force chosen sdk
        self.__class__.replace_vs_prop(proj_path,
                                       'WindowsTargetPlatformVersion',
                                       sdk_version)
        # force chosen toolset
        self.__class__.replace_vs_prop(proj_path,
                                       'PlatformToolset',
                                        toolset)
        args = []
        args.extend(self.default_msbuild_args)
        args.extend(self.extra_msbuild_args)
        args.append(proj_path)
        result = getSHrunner().exec_batch(self.msbuild, args)
        if result[0] == 1:
            log.error("Build failed when building " + pkg_name)
            sys.exit(1)

    def setup_vs_env(self, env_target):
        if self.vsenv_done:
            log.debug('vs environment already initialized')
            return
        log.debug('Setting up vs environment')
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
        '-d', '--debug', default='DEBUG',
        help='Sets the logging level')
    ap.add_argument(
        '-i', '--indent', action='store_true',
        help='Sets whether the logs are indented to stack frames')
    ap.add_argument(
        '-v', '--verbose', action='store_true',
        help='Sets whether the logs are verbose or not')
    ap.add_argument(
        '-p', '--purge', action='store_true',
        help='Cleans out contrib tarball directory')
    ap.add_argument(
        '-s', '--sdk', default=win_sdk_default, type=str,
        help='Use specified windows sdk version')
    ap.add_argument(
        '-t', '--toolset', default=win_toolset_default, type=str,
        help='Use specified platform toolset version')

    parsed_args = ap.parse_args()

    return parsed_args


def main():
    start_time = time.time()

    parsed_args = parse_args()

    setup_logging(lvl=parsed_args.debug,
                  verbose=parsed_args.verbose,
                  do_indent=parsed_args.indent)

    if not host_is_64bit:
        log.error('These scripts will only run on a 64-bit Windows system for now!')
        sys.exit(1)

    if int(getLatestVSVersion()) < 15:
        log.error('These scripts require at least Visual Studio v15 2017!')
        sys.exit(1)

    if parsed_args.purge:
        if os.path.exists(contrib_tmp_dir):
            log.warning('Removing contrib tarballs ' + contrib_tmp_dir)
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.tar.*'])
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.tgz.*'])
            getSHrunner().exec_batch(
                'del', ['/s', '/f', '/q', contrib_tmp_dir + '\\*.zip.*'])
        else:
            log.warning('No tarballs to remove')

    if parsed_args.clean:
        if os.path.exists(contrib_build_dir) and parsed_args.clean == 'all':
            log.warning('Removing contrib builds ' + contrib_build_dir)
            getSHrunner().exec_batch('rmdir', ['/s', '/q', contrib_build_dir])
        else:
            pkg_json_file = get_pkg_file(parsed_args.clean)
            with open(pkg_json_file) as json_file:
                pkg_info = json.load(json_file)
                dir_to_clean = contrib_build_dir + '\\' + pkg_info['name']
                file_to_clean = contrib_build_dir + '\\.' + pkg_info['name']
                if os.path.exists(dir_to_clean) or os.path.exists(file_to_clean):
                    log.warning('Removing contrib build ' + dir_to_clean)
                    getSHrunner().exec_batch(
                        'rmdir', ['/s', '/q', dir_to_clean])
                    getSHrunner().exec_batch(
                        'del', ['/s', '/f', '/q', file_to_clean])
                else:
                    log.warning('No builds to remove')

    if parsed_args.build:
        if not os.path.exists(contrib_build_dir):
            os.makedirs(contrib_build_dir)
        log.info('Making: ' + parsed_args.build)
        resolve(parsed_args.build, parsed_args.force,
                parsed_args.sdk, parsed_args.toolset)
        log.info('Make done for: ' + parsed_args.build)

    log.debug("--- %s ---" % secondsToStr(time.time() - start_time))


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


class CustomAdapter(logging.LoggerAdapter):
    @staticmethod
    def indent():
        indentation_level = len(traceback.extract_stack())
        return indentation_level - 4 - 2  # Remove logging infrastructure frames

    def process(self, msg, kwargs):
        return '{i}{m}'.format(i=' '*(self.indent()), m=msg), kwargs


def setup_logging(lvl=logging.DEBUG, verbose=False, do_indent=False):
    format = ''
    if verbose:
        format = '[ %(levelname)-8s %(created).6f %(funcName)10s:%(lineno)4s ] '
    fmt = format + '%(message)s'
    try:
        import coloredlogs
        coloredlogs.install(
            level=lvl,
            logger=root_logger,
            fmt=fmt,
            level_styles={
                'debug': {'color': 'blue'},
                'info': {'color': 'green'},
                'warn': {'color': 'yellow'},
                'error': {'color': 'red'},
                'critical': {'color': 'red', 'bold': True}
            },
            field_styles={
                'asctime': {'color': 'magenta'},
                'created': {'color': 'magenta'},
                'levelname': {'color': 'cyan'},
                'funcName': {'color': 'black', 'bold': True},
                'lineno': {'color': 'black', 'bold': True}
            })
    except ImportError:
        root_logger.setLevel(logging.DEBUG)
        logging.basicConfig(level=lvl, format=fmt)
    if do_indent:
        global log
        log = CustomAdapter(logging.getLogger(__name__), {})
    else:
        log = logging.getLogger(__name__)


if __name__ == '__main__':
    main()
