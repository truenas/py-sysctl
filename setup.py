#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys

from setuptools import setup, Extension

from sysctl import get_version

modules = [
    Extension(
        'sysctl/_sysctl',
        sources=['sysctl/_sysctl.c'],
        extra_compile_args=['-Wall', '-Wextra', '-pedantic', '-Werror'],
    )
]

setup(
    name='sysctl',
    version=get_version(),
    url='https://github.com/williambr/py-sysctl',
    license='BSD',
    author='William Grzybowski',
    author_email='wg@FreeBSD.org',
    description='Wrapper for the sysctl* system functions',
    long_description=open(
        os.path.join(
            os.path.dirname(__file__),
            'README.rst'
        )
    ).read(),
    keywords='sysctl',
    packages=('sysctl', 'sysctl/tests'),
    platforms='any',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.2',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: Implementation :: CPython',
    ],
    #cmdclass={'build_py': build_py},
    ext_modules=modules,
    test_suite='nose.collector',
)
