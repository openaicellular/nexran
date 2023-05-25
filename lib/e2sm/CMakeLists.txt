
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


if("${RIC_GENERATED_E2SM_KPM_BINDING_DIR}" STREQUAL "")
  set(E2SM_KPM_ASN_FILE ${ASN_DIR}/e2sm-kpm-v01.00.asn1)
  set(E2SM_KPM_C_DIR ${CMAKE_CURRENT_BINARY_DIR}/messages/E2SM-KPM/)
  execute_process(
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_KPM_C_DIR}" "${E2SM_KPM_ASN_FILE}" "E2SM_KPM_" -fno-include-deps -fincludes-quoted
    RESULT_VARIABLE ret)
  if (NOT ${ret} STREQUAL 0)
    message(FATAL_ERROR "${ret}: error")
  endif (NOT ${ret} STREQUAL 0)
else()
  set(E2SM_KPM_C_DIR "${RIC_GENERATED_E2SM_KPM_BINDING_DIR}")
endif()
file(GLOB E2SM_KPM_source ${E2SM_KPM_C_DIR}/*.c)
if("${RIC_GENERATED_E2SM_KPM_BINDING_DIR}" STREQUAL "")
  add_custom_command (
    OUTPUT ${E2SM_KPM_source}
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_KPM_C_DIR}" "${E2SM_KPM_ASN_FILE}" "E2SM_KPM_" -fno-include-deps -fincludes-quoted
    DEPENDS ${E2SM_KPM_ASN_FILE})
endif()

if("${RIC_GENERATED_E2SM_NEXRAN_BINDING_DIR}" STREQUAL "")
  set(E2SM_NEXRAN_ASN_FILE ${ASN_DIR}/e2sm-nexran-v01.00.asn1)
  set(E2SM_NEXRAN_C_DIR ${CMAKE_CURRENT_BINARY_DIR}/messages/E2SM-NEXRAN/)
  execute_process(
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_NEXRAN_C_DIR}" "${E2SM_NEXRAN_ASN_FILE}" "E2SM_NEXRAN_" -fno-include-deps -fincludes-quoted
    RESULT_VARIABLE ret)
  if (NOT ${ret} STREQUAL 0)
    message(FATAL_ERROR "${ret}: error")
  endif (NOT ${ret} STREQUAL 0)
else()
  set(E2SM_NEXRAN_C_DIR "${RIC_GENERATED_E2SM_NEXRAN_BINDING_DIR}")
endif()
file(GLOB E2SM_NEXRAN_source ${E2SM_NEXRAN_C_DIR}/*.c)
if("${RIC_GENERATED_E2SM_NEXRAN_BINDING_DIR}" STREQUAL "")
  add_custom_command (
    OUTPUT ${E2SM_NEXRAN_source}
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_NEXRAN_C_DIR}" "${E2SM_NEXRAN_ASN_FILE}" "E2SM_NEXRAN_" -fno-include-deps -fincludes-quoted
    DEPENDS ${E2SM_NEXRAN_ASN_FILE})
endif()

if("${RIC_GENERATED_E2SM_ZYLINIUM_BINDING_DIR}" STREQUAL "")
  set(E2SM_ZYLINIUM_ASN_FILE ${ASN_DIR}/e2sm-zylinium-v01.00.asn1)
  set(E2SM_ZYLINIUM_C_DIR ${CMAKE_CURRENT_BINARY_DIR}/messages/E2SM-ZYLINIUM/)
  execute_process(
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_ZYLINIUM_C_DIR}" "${E2SM_ZYLINIUM_ASN_FILE}" "E2SM_ZYLINIUM_" -fno-include-deps -fincludes-quoted
    RESULT_VARIABLE ret)
  if (NOT ${ret} STREQUAL 0)
    message(FATAL_ERROR "${ret}: error")
  endif (NOT ${ret} STREQUAL 0)
else()
  set(E2SM_ZYLINIUM_C_DIR "${RIC_GENERATED_E2SM_ZYLINIUM_BINDING_DIR}")
endif()
file(GLOB E2SM_ZYLINIUM_source ${E2SM_ZYLINIUM_C_DIR}/*.c)
if("${RIC_GENERATED_E2SM_ZYLINIUM_BINDING_DIR}" STREQUAL "")
  add_custom_command (
    OUTPUT ${E2SM_ZYLINIUM_source}
    COMMAND ${MAKE_ASN1_INCLUDES_SH} "${E2SM_ZYLINIUM_C_DIR}" "${E2SM_ZYLINIUM_ASN_FILE}" "E2SM_ZYLINIUM_" -fno-include-deps -fincludes-quoted
    DEPENDS ${E2SM_ZYLINIUM_ASN_FILE})
endif()


add_library(
  e2sm
  ${E2SM_KPM_source}
  ${E2SM_NEXRAN_source}
  ${E2SM_ZYLINIUM_source}
  src/e2sm.cc
  src/e2sm_nexran.cc
  src/e2sm_zylinium.cc
  src/e2sm_kpm.cc
  )
include_directories(${E2SM_KPM_C_DIR})
include_directories(${E2SM_NEXRAN_C_DIR})
include_directories(${E2SM_ZYLINIUM_C_DIR})

#target_include_directories(e2sm BEFORE PUBLIC ${E2SM_KPM_C_DIR})
#target_include_directories(e2sm BEFORE PUBLIC ${E2SM_NEXRAN_C_DIR})
