cmake_minimum_required(VERSION 3.20)
project(orcsdr_map_catalog_security LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
foreach(required ORCMAPS_ROOT MBEDTLS_ROOT MBEDCRYPTO_LIBRARY CJSON_ROOT)
  if(NOT ${required})
    message(FATAL_ERROR "Set ${required} to the resolved dependency")
  endif()
endforeach()
set(ui "${CMAKE_CURRENT_SOURCE_DIR}/../../../ui")
add_executable(map_catalog_security_test catalog_security_test.cpp
  "${ui}/map_catalog.cpp" "${CJSON_ROOT}/cJSON.c"
  "${ORCMAPS_ROOT}/src/core/pack.cpp" "${ORCMAPS_ROOT}/src/core/pack_json.cpp"
  "${ORCMAPS_ROOT}/src/core/geo.cpp")
target_include_directories(map_catalog_security_test PRIVATE "${ui}" "${CJSON_ROOT}"
  "${MBEDTLS_ROOT}/include" "${ORCMAPS_ROOT}/include")
target_link_libraries(map_catalog_security_test PRIVATE "${MBEDCRYPTO_LIBRARY}")
if(WIN32)
  target_link_libraries(map_catalog_security_test PRIVATE bcrypt)
endif()
if(MSVC)
  target_compile_options(map_catalog_security_test PRIVATE /EHsc)
endif()
