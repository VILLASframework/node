set(LIB libnodes-go.a)
file(GLOB_RECURSE SRCS *.go)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${LIB}
  DEPENDS ${SRCS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND env CGO_ENABLED=1 GOPATH=${GOPATH} go build -buildmode=c-archive -o "${CMAKE_CURRENT_BINARY_DIR}/${LIB}" ${CMAKE_GO_FLAGS} .
  COMMENT "Building Go library ${LIB}")

add_custom_target(libvillas-go DEPENDS ${LIB} ${HEADER})

add_library(nodes-go STATIC IMPORTED GLOBAL)
add_dependencies(nodes-go libvillas-go)
set_target_properties(nodes-go
  PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${LIB}"
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}
)
