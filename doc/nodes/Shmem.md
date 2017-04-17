# Shared memory {#shmem}

The `shmem` node-type can be used to quickly exchange samples with a process on the same host using a POSIX shared memory object.

## Configuration

The only required configuration option is the `name` option; all others are optional with reasonable defaults.

#### `name` (string)

Name of the POSIX shared memory object. Must start with a forward slash (`/`).
The same name should be passed to the external program somehow in its
configuration or command-line arguments.

#### `queuelen` (int)

Length of the input and output queues in elements. Defaults to `DEFAULT_SHMEM_QUEUELEN`,
a compile-time constant.

#### `samplelen` (int)

Maximum number of data elements in a single `struct sample` for the samples handled
by this node. Defaults to `DEFAULT_SHMEM_SAMPLELEN`, a compile-time constant.

#### `polling` (boolean)

If set to false, POSIX condition variables are used to signal writes between processes.
If set to true, no CV's are used, meaning that blocking writes have to be
implemented using polling, leading to performance improvements at a cost of
unnecessary CPU usage. Defaults to false.

#### `exec` (array of strings)

Optional name and command-line arguments (as passed to `execve`) of a command
to be executed during node startup. This can be used to start the external
program directly from VILLASNode. If unset, no command is executed.

## API for external programs

The actual sharing of data is implemented by putting two shared `struct queue`s
(one per direction) and an associated `struct pool` in the shared memory region.
Samples can be exchanged by simply writing to and reading from these queues.

External programs that want to use this interface can link against
`libvillas-ext.so`. This library provides a subset of the functions from
`libvillas.so` that can be used to access and modify the shared data structures.

The interface for external programs is very simple: after opening the shared
memory object with `shmem_shared_open` (passing the object name from the
configuration file), samples can be read from and written to VILLASNode using
`shmem_shared_read` and `shmem_shared_write`, respectively. Samples written to
the node must be allocated by `sample_alloc` from the shared pool; samples read
from the node should be freed with `sample_put` after they have been processed.
See the [example client](test-shmem_8c_source.html) and the [API
documentation](group__shmem.html) for more details.
