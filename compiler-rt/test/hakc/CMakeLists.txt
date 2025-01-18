# message("HAKC2_0")
set(HAKC_LIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(HAKC_TESTSUITES)
set(HAKC_FDR_TESTSUITES)

set(HAKC_TEST_ARCH ${HAKC_SUPPORTED_ARCH})
if (COMPILER_RT_BUILD_HAKC)
foreach(arch ${HAKC_TEST_ARCH})
set(HAKC_TEST_TARGET_ARCH ${arch})
string(TOLOWER "-${arch}-${OS_NAME}" HAKC_TEST_CONFIG_SUFFIX)
get_test_cc_for_arch(${arch} HAKC_TEST_TARGET_CC HAKC_TEST_TARGET_CFLAGS)
string(TOUPPER ${arch} ARCH_UPPER_CASE)
set(CONFIG_NAME ${ARCH_UPPER_CASE}${OS_NAME}Config)

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME}/lit.site.cfg.py)
  list(APPEND HAKC_TESTSUITES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME})
  endforeach()
  endif()
  
add_lit_testsuite(check-hakc "Running the hakc tests"
${HAKC_TESTSUITES})
