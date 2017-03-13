# Getting started

We put some effort in getting you started as smooth as possible.

For first tests and development you can use the Docker platform to bootstrap your environment.
Docker is a software to run containers (a.k.a images in Docker's terminology) on a Linux machine.

We prepared a image which you can download and run out of the box:

1. Download the Docker toolbox: https://www.docker.com/docker-toolbox .
   This toolbox includes a virtual machine as well all the Docker tools you need to the Docker container which is provided by us.
   More instructions to get with can be found here: http://docs.docker.com/windows/started/

2. After installing the toolbox, open the "Docker Quickstart Terminal"

3. Start the latest VILLASnode container by running:

    $ git clone --recursive git@git.rwth-aachen.de:VILLASframework/VILLASnode.git
    $ cd VILLASnode
    $ make docker

### To be added

VILLASnode ...

- is written in object-oriented C11
- is compiled with Clang / LLVM or GCC
- is fully based on open source software
- is extensible with new node types & hooks
- is heavily multi-threaded
- follows the Unix philosophy
- is separated into a library (libvillas) and a few binaries (villas-server, villas-pipe, villas-test-*, villas-signal, villas-hook) which link against the lib.