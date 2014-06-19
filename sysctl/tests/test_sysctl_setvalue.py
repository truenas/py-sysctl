import os
import sysctl
from sysctl.tests import SysctlTestBase


class TestSysctlANum(SysctlTestBase):

    def test_sysctl_setvalue(self):
        dummy = sysctl.filter('kern.dummy')[0]
        try:
            self.command("/sbin/sysctl kern.dummy=0")
        except:
            if os.getuid() == 0:
                raise

        try:
            dummy.value = 1
        except TypeError:
            if os.getuid() == 0:
                raise

        if os.getuid() == 0:
            value = int(self.command("/sbin/sysctl -n kern.dummy"))
            if value != 1:
                raise ValueError("Failed to set kern.dummy")
