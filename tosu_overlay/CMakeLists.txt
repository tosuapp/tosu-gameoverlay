project(tosu_overlay)

#
# Source files.
#

# tosu_overlay sources.
set(CEFSIMPLE_SRCS
  simple_app.cc
  simple_app.h
  simple_handler.cc
  simple_handler.h
)

set(CEFSIMPLE_SRCS_WINDOWS
  tosu_overlay_win.cc
  resource.h
  simple_handler_win.cc
  )
APPEND_PLATFORM_SOURCES(CEFSIMPLE_SRCS)
source_group(${PROJECT_NAME} FILES ${CEFSIMPLE_SRCS})

set(CEFSIMPLE_WIN_SRCS_WINDOWS
  win/tosu_overlay.rc
)

APPEND_PLATFORM_SOURCES(CEFSIMPLE_WIN_SRCS)
source_group(${PROJECT_NAME}\\\\win FILES ${CEFSIMPLE_WIN_SRCS})

set(CEFSIMPLE_SRCS
  ${CEFSIMPLE_SRCS}
  ${CEFSIMPLE_WIN_SRCS}
  )

# tosu_overlay helper sources.
APPEND_PLATFORM_SOURCES(CEFSIMPLE_HELPER_SRCS)
source_group(${PROJECT_NAME} FILES ${CEFSIMPLE_HELPER_SRCS})

#
# Shared configuration.
#

# Target executable names.
set(CEF_TARGET "tosu_overlay")
# Logical target used to link the libcef library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

#
# Windows configuration.
#

if(OS_WINDOWS)
  # Executable target.
  add_executable(${CEF_TARGET} WIN32 ${CEFSIMPLE_SRCS})
  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
  target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})

  if(USE_SANDBOX)
    # Logical target used to link the cef_sandbox library.
    ADD_LOGICAL_TARGET("cef_sandbox_lib" "${CEF_SANDBOX_LIB_DEBUG}" "${CEF_SANDBOX_LIB_RELEASE}")
    target_link_libraries(${CEF_TARGET} cef_sandbox_lib ${CEF_SANDBOX_STANDARD_LIBS})
  endif()

  # Add the custom manifest files to the executable.
  ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${CEF_TARGET}" "exe")

  # Copy binary and resource files to the target output directory.
  COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
endif()
