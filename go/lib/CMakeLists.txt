set(LIB libvillas-go.a)
set(HEADER libvillas-go.h)

file(GLOB_RECURSE SRCS *.go)
file(GLOB_RECURSE NODE_SRCS ../pkg/nodes/*)

list(FILTER SRCS EXCLUDE REGEX /_obj/)
list(FILTER NODE_SRCS EXCLUDE REGEX /_obj/)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${HEADER}
  DEPENDS ${SRCS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND env "GOPATH=${GOPATH}" "CGO_ENABLED=1" ${GO} tool cgo  -exportheader "${CMAKE_CURRENT_BINARY_DIR}/${HEADER}" -- -I "${CMAKE_CURRENT_SOURCE_DIR}/../../include" ${SRCS}
  COMMENT "Generating CGo header ${HEADER}"
  COMMAND_EXPAND_LISTS
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${LIB}
  DEPENDS ${SRCS} ${NODE_SRCS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND env "PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig" "GOPATH=${GOPATH}" "CGO_ENABLED=1" "CGO_CFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/../../include" ${GO} build -buildmode=c-archive -o "${CMAKE_CURRENT_BINARY_DIR}/${LIB}" ${CMAKE_GO_FLAGS} .
  COMMENT "Building CGo library ${LIB}"
  COMMAND_EXPAND_LISTS)

add_custom_target(villas-go-lib DEPENDS ${LIB})
add_custom_target(villas-go-header DEPENDS ${HEADER})

add_library(villas-go STATIC IMPORTED GLOBAL)
add_dependencies(villas-go villas-go-lib)
set_target_properties(villas-go 
  PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${LIB}"
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}
)
