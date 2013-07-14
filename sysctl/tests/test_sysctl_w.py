import sysctl
from sysctl.tests import SysctlTestBase


class TestSysctlWritable(SysctlTestBase):

    def test_sysctl_writable(self):
        libAll = sysctl.filter(writable=True)
        cmdAll = self.command("/sbin/sysctl -WNa")
        cmdNames = []
        for line in cmdAll.split('\n'):
            if not line:
                continue
            cmdNames.append(line)

        notIn = []
        for ctl in libAll:
            if ctl.name not in cmdNames:
                notIn.append(ctl.name)

        print(notIn)
        assert len(notIn) == 0

    def test_sysctl_writable_false(self):
        libAll = sysctl.filter(writable=False)

        cmdAll = self.command("/sbin/sysctl -Na")
        cmdNames = []
        for line in cmdAll.split('\n'):
            if not line:
                continue
            cmdNames.append(line)

        cmdWAll = self.command("/sbin/sysctl -WNa")
        for line in cmdWAll.split('\n'):
            if not line:
                continue
            if line in cmdNames:
                cmdNames.remove(line)

        notIn = []
        for ctl in libAll:
            if ctl.name not in cmdNames:
                notIn.append(ctl.name)

        print(notIn)
        assert len(notIn) == 0
