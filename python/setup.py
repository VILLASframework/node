import os, re
from setuptools import setup
from glob import glob

def cleanhtml(raw_html):
    cleanr = re.compile('<.*?>')
    cleantext = re.sub(cleanr, '', raw_html)
    return cleantext

def read(fname):
    dname = os.path.dirname(__file__)
    fname = os.path.join(dname, fname)

    with open(fname) as f:
        contents = f.read()
        sanatized = cleanhtml(contents)

        try:
            from m2r import M2R
            m2r = M2R()
            return m2r(sanatized)
        except:
            return sanatized

setup(
    name = 'villas-node',
    version = '0.6.4',
    author = 'Steffen Vogel',
    author_email = 'acs-software@eonerc.rwth-aachen.de',
    description = 'Python-support for VILLASnode simulation-data gateway',
    license = 'GPL-3.0',
    keywords = 'simulation power system real-time villas',
    url = 'https://git.rwth-aachen.de/acs/public/villas/dataprocessing',
    packages = [ 'villas.node' ],
    long_description = read('README.md'),
    classifiers = [
        'Development Status :: 4 - Beta',
        'Topic :: Scientific/Engineering',
        'License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3'
    ],
    install_requires = [
        'requests'
    ],
    setup_requires = [
        'm2r'
    ],
    scripts = glob('bin/*')
)

