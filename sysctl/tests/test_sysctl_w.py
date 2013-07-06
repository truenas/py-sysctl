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

        print notIn
        assert len(notIn) == 0
