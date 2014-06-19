============
py-sysctl
============

py-sysctl is a wrapper for the sysctl* system functions.
The core is written in C.


Examples
--------

py-sysctl provides a very simple interface to access sysctls through the filter method.


Sysctls for CPU #0
++++++++++++++++++

    >>> import sysctl
    >>> sysctl.filter('dev.cpu.0')
    [<Sysctl: dev.cpu.0.%desc>, <Sysctl: dev.cpu.0.%driver>, <Sysctl: dev.cpu.0.%location>, <Sysctl: dev.cpu.0.%pnpinfo>, <Sysctl: dev.cpu.0.%parent>, <Sysctl: dev.cpu.0.freq>, <Sysctl: dev.cpu.0.freq_levels>, <Sysctl: dev.cpu.0.cx_supported>, <Sysctl: dev.cpu.0.cx_lowest>, <Sysctl: dev.cpu.0.cx_usage>]


Inspect a sysctl
++++++++++++++++

    >>> import sysctl
    >>> shmall = sysctl.filter('kern.ipc.shmall')[0]
    >>> shmall.value
    131072L
    >>> shmall.tuneable
    False
    >>> shmall.writable
    True


License
-------

This python module is licensed under BSD 2 Clause, see LICENSE.
