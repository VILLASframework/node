from setuptools import setup, find_namespace_packages
from glob import glob
import sys

with open('README.md') as f:
    long_description = f.read()

setup(
    name='villas-node',
    version='0.10.2',
    author='Steffen Vogel',
    author_email='acs-software@eonerc.rwth-aachen.de',
    description='Python-support for VILLASnode simulation-data gateway',
    license='GPL-3.0',
    keywords='simulation power system real-time villas',
    url='https://git.rwth-aachen.de/acs/public/villas/VILLASnode',
    packages=find_namespace_packages(include=['villas.*']),
    long_description=long_description,
    long_description_content_type='text/markdown',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Topic :: Scientific/Engineering',
        'License :: OSI Approved :: '
        'GNU General Public License v3 or later (GPLv3+)',
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
