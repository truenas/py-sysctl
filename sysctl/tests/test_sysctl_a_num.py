import sysctl
from sysctl.tests import SysctlTestBase


class TestSysctlANum(SysctlTestBase):

    def test_sysctl_a_num(self):
        numLib = len(sysctl.filter())
        numCmd = int(self.command("/sbin/sysctl -Na|wc -l").strip())

        assert numLib == numCmd
