#!/bin/bash

rpm -Uvh http://ccrma.stanford.edu/planetccrma/mirror/fedora/linux/planetccrma/20/i386/planetccrma-repo-1.1-3.fc20.ccrma.noarch.rpm 

yum update

yum install planetccrma-core

source update_boot.sh
