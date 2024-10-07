project(tosu_overlay)

#
# Source files.
#

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# tosu_overlay sources.
set(CEFSIMPLE_SRCS
  tosu_overlay_app.cc
  tosu_overlay_app.h
  tosu_overlay_handler.cc
  tosu_overlay_handler.h
  tosu_overlay_handler_win.cc
  tosu_overlay_win.cc
  tools.cc
  tools.h
  glad.cc
  canvas.cc
  resource.h
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
  if (DESKTOP)
    add_compile_definitions(DESKTOP=1)
    add_executable(${CEF_TARGET} WIN32 ${CEFSIMPLE_SRCS})
  else()
    add_compile_definitions(DESKTOP=0)
    add_library(${CEF_TARGET} SHARED ${CEFSIMPLE_SRCS})
  endif()

  target_link_options(${PROJECT_NAME} PUBLIC "/DELAYLOAD:libcef.dll")

  target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    lib/include
  )

  target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib)

  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
  target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper minhook opengl32 delayimp ${CEF_STANDARD_LIBS})

  if(USE_SANDBOX)
    # Logical target used to link the cef_sandbox library.
    ADD_LOGICAL_TARGET("cef_sandbox_lib" "${CEF_SANDBOX_LIB_DEBUG}" "${CEF_SANDBOX_LIB_RELEASE}")
    target_link_libraries(${CEF_TARGET} cef_sandbox_lib ${CEF_SANDBOX_STANDARD_LIBS})
  endif()

  # Add the custom manifest files to the executable.
  if (DESKTOP)
    ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${CEF_TARGET}" "exe")
  else()
    ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${CEF_TARGET}" "dll")
  endif()

  # Copy binary and resource files to the target output directory.
  COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
endif()
