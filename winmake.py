import json
import subprocess
import sys
import os
import shutil


cwd = os.path.dirname(__file__)


def shellquote(s):
    return "'" + s.replace("'", "'\\''") + "'"


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
    vcEnvInit = [findVSLatestDir() + r'\VC\Auxiliary\Build\vcvarsall.bat']
    if platform != '':
        args = [arch, platform, version]
    else:
        args = [arch, version]
    if args:
        vcEnvInit.extend(args)
    vcEnvInit = ' '.join(vcEnvInit)
    # print(vcEnvInit)
    test = 'call \"' + findVSLatestDir() + \
        r'\VC\Auxiliary\Build\\"vcvarsall.bat x86_amd64 10.0.15063.0'
    print(test)
    msbuildEnv = subprocess.Popen(test,
                                  shell=True,
                                  stderr=sys.stderr,
                                  stdout=sys.stdout
                                  )
    msbuildEnv.communicate()


def resolve(pkgName=None):
    if pkgName == None:
        raise IOError("Can't just do nothing!")
    print('resolving: ' + pkgName)
    pkg_script = cwd + '\\contrib\\src\\' + pkgName + "\\make-this.py"
    subprocess.call(["python.exe", pkg_script])


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


@Singleton
class ContribManager():
    def __init__(self):
        self.wget_cmd = 'wget --no-check-certificate --retry-connrefused --waitretry=1 --read-timeout=20 --timeout=15 --tries=4'
        self.apply_cmd = 'git apply --reject --ignore-whitespace --whitespace=fix'
        self.patch_cmd = 'patch -flp1 -i'

    def build(self, list=[]):
        return


def getContribManager():
    return ContribManager.instance()


@Singleton
class SHrunner():
    def __init__(self):
        self.ps_path = r'C:\Windows\system32\WindowsPowerShell\v1.0\powershell.exe'
        self.cmd_path = r'C:\Windows\System32\cmd.exe'
        self.sh_path = r'C:\Windows\system32\bash.exe'

    def exec_ps1(self, script=None, args=[]):
        if not os.path.exists(self.ps_path):
            raise IOError('Powershell not found at %s.' % self.ps_path)
        cmd = [self.ps_path, '-ExecutionPolicy', 'ByPass', script]
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
        cmd = [r'call ', script]
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

    def exec_sh(self, script=None, args=[]):
        return


def getSHrunner():
    return SHrunner.instance()


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


if __name__ == '__main__':
    resolve('gnutls')
