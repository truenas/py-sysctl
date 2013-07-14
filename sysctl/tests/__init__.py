import subprocess


class SysctlTestBase(object):

    def command(self, cmd):
        proc = subprocess.Popen(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        stdout = proc.communicate()[0]
        if proc.returncode != 0:
            raise AssertionError
        return stdout.decode('utf8')
