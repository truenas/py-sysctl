import os

try:
    import pygit2
    PYGIT2 = True
except ImportError:
    PYGIT2 = False

try:
    from ._sysctl import *
except ImportError:
    pass

VERSION = (0, 3, 3, 'final', 0)


def get_version(version=None):
    """Derives a PEP386-compliant version number from VERSION."""
    if version is None:
        version = VERSION
    assert len(version) == 5
    assert version[3] in ('alpha', 'beta', 'rc', 'final')

    # Now build the two parts of the version number:
    # main = X.Y[.Z]
    # sub = .devN - for pre-alpha releases
    #     | {a|b|c}N - for alpha, beta and rc releases

    parts = 2 if version[2] == 0 else 3
    main = '.'.join(str(x) for x in version[:parts])

    sub = ''
    if version[3] == 'alpha' and version[4] == 0:
        if PYGIT2:
            repo = os.path.realpath(
                os.path.join(os.path.dirname(__file__), "..")
            )
            try:
                revision = pygit2.repository.Repository(repo).head.hex[:7]
            except KeyError:
                revision = 'UNKNOWN'
            sub = '.dev%s' % revision
        else:
            sub = '.devUNKNOWN'

    elif version[3] != 'final':
        mapping = {'alpha': 'a', 'beta': 'b', 'rc': 'c'}
        sub = mapping[version[3]] + str(version[4])

    return main + sub
