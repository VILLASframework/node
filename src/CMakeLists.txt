link_libraries(villas)

add_executable(villas-node node.c)
add_executable(villas-test-rtt test-rtt.c)
add_executable(villas-test-shmem test-shmem.c)

if(WITH_IO)
    add_executable(villas-test-cmp test-cmp.c)
    add_executable(villas-convert convert.c)
    add_executable(villas-pipe pipe.c)
    add_executable(villas-signal signal.c)
endif()

if(WITH_IO AND WITH_HOOKS)
    add_executable(villas-hook hook.c)
endif()
