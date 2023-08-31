# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

from setuptools import setup, find_namespace_packages
from glob import glob
import sys

with open('README.md') as f:
    long_description = f.read()

setup(
    name='villas-node',
    version='0.10.3',
    author='Steffen Vogel',
    author_email='acs-software@eonerc.rwth-aachen.de',
    description='Python-support for VILLASnode simulation-data gateway',
    license='Apache-2.0',
    keywords='simulation power system real-time villas',
    url='https://git.rwth-aachen.de/acs/public/villas/VILLASnode',
    packages=find_namespace_packages(include=['villas.*']),
    long_description=long_description,
    long_description_content_type='text/markdown',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Topic :: Scientific/Engineering',
        'License :: OSI Approved :: '
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3'
    ],
    install_requires=[
        'requests'
    ] + [
        'linuxfd'
    ] if sys.platform == 'linux' else [],
    setup_requires=[
        'm2r'
    ],
    scripts=glob('bin/*')
)
