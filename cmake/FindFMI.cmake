

find_path(FMI_INCLUDE_DIR
    NAMES fmilib.h
)

find_library(FMI_LIBRARY fmilib
  PATHS /usr/local/lib /usr/local/lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FMI DEFAULT_MSG FMI_LIBRARY FMI_INCLUDE_DIR)

mark_as_advanced(FMI_INCLUDE_DIR FMI_LIBRARY)

if(FMI_FOUND)
    add_library(FMI SHARED IMPORTED)
    set_target_properties(FMI PROPERTIES
        IMPORTED_LOCATION "${FMI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${FMI_INCLUDE_DIR}"
    )
endif()
