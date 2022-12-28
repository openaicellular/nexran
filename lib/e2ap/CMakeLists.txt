
# NB: asn1c generates code with warnings.  Hack around this for now.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
endif()

set(MAKE_ASN1_INCLUDES_SH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/tools/make_asn1c_includes.sh)

# Set global flags for build of generated asn.1 bindings.
#
# For debugging: add ASN_EMIT_DEBUG to see per-type debug asn.1
# encode/decode info (-DASN_EMIT_DEBUG=1).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DASN_DISABLE_OER_SUPPORT=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASN_DISABLE_OER_SUPPORT=1")

# The source dir containing our asn.1 specification source files, if any.
set(ASN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/messages)

#
# Build the E2AP bindings.
#
if("${RIC_GENERATED_E2AP_BINDING_DIR}" STREQUAL "")
  set(E2AP_ASN_FILE ${ASN_DIR}/e2ap-v01.00.asn1)
  set(E2AP_C_DIR ${CMAKE_CURRENT_BINARY_DIR}/E2AP/)

  # NB: we need to run an execute_process during makefile parsing, to
  # force population of the generated files, so that we can glob them
  # for dependencies.  When you don't know the file listing being
  # generated a priori, you must use this two-phase approach if you want
  # the bindings to be regenerated again if the specification source is
  # newer.

  execute_process(
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2AP_C_DIR}" "${E2AP_ASN_FILE}" "E2AP_" -fno-include-deps -fincludes-quoted
    RESULT_VARIABLE ret)
  if (NOT ${ret} STREQUAL 0)
    message(FATAL_ERROR "${ret}: error")
  endif (NOT ${ret} STREQUAL 0)
else()
  set(E2AP_C_DIR "${RIC_GENERATED_E2AP_BINDING_DIR}")
endif()
file(GLOB E2AP_source ${E2AP_C_DIR}/*.c)

# NB: definition of this custom_command has to wait until we have the
# $E2AP_source glob from above, *after* we have run the execute process
# to make the bindings so that we have the dependent files to be used in
# $OUTPUT.
if("${RIC_GENERATED_E2AP_BINDING_DIR}" STREQUAL "")
  add_custom_command (
    OUTPUT ${E2AP_source}
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2AP_C_DIR}" "${E2AP_ASN_FILE}" "E2AP_" -fno-include-deps -fincludes-quoted
    DEPENDS ${E2AP_ASN_FILE})
endif()

add_library(
  e2ap
  ${E2AP_source}
  src/e2ap.cc)
include_directories(${E2AP_C_DIR})

#target_include_directories(e2ap BEFORE PUBLIC ${E2AP_C_DIR})
