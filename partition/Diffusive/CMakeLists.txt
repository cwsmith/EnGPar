set(ENGPAR_SOURCES
  engpar.cpp
  engpar_diffusive_input.cpp
  src/engpar_balancer.cpp
  src/engpar_vtxBalancer.cpp
  src/engpar_queue.cpp
  src/engpar_selector.cpp
  src/engpar_make.cpp
  src/engpar_sd.cpp
  )

set(ENGPAR_HEADERS
  engpar.h
  )

if(OpenCL_FOUND)
  include_directories(${CMAKE_HOME_DIRECTORY}/opencl)
  include_directories(${OPENCLHPPDIR})
  message(STATUS "CMAKE_BINARY_DIR is ${CMAKE_BINARY_DIR}")
  include_directories(${CMAKE_BINARY_DIR}/opencl) # opencl config header
  set(ENGPAR_SOURCES
    ${ENGPAR_SOURCES}
    src/engpar_opencl_bfs.cpp)
  file(COPY "src/bfskernel.cl" DESTINATION "${CMAKE_BINARY_DIR}/test")
  set(kernel bfskernel.cl)
  set(kernelSrc src/${kernel})
  set(kernelDest test/${kernel})
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${kernelDest}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_SOURCE_DIR}/${kernelSrc}
      ${CMAKE_BINARY_DIR}/${kernelDest}
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${kernelSrc})
endif()

add_library(diffusive ${ENGPAR_SOURCES})

target_include_directories(diffusive PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  )

target_link_libraries(diffusive agi)

if(OpenCL_FOUND)
  target_link_libraries(diffusive OpenCL::OpenCL)
endif()

engpar_export_lib(diffusive "${ENGPAR_HEADERS}")
