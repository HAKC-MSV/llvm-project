# Build for all components of the HAKC support library.

# Now put it all together...
include_directories(..)
include_directories(../../include)

set(HAKC_CFLAGS
  ${COMPILER_RT_COMMON_CFLAGS}
  ${COMPILER_RT_CXX_CFLAGS})

set(HAKC_LINK_FLAGS ${COMPILER_RT_COMMON_LINK_FLAGS})
set(HAKC_LINK_LIBS
  ${COMPILER_RT_UNWINDER_LINK_LIBS}
  ${COMPILER_RT_CXX_LINK_LIBS})

add_compiler_rt_component(hakc)

set(HAKC_COMMON_RUNTIME_OBJECT_LIBS
    RTSanitizerCommon
    RTSanitizerCommonLibc)

# HAKC uses C++ standard library headers.
if (TARGET cxx-headers OR HAVE_LIBCXX)
  set(HAKC_DEPS cxx-headers)
endif()
