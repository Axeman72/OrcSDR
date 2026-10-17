cmake_minimum_required(VERSION 3.16)
project(orcsdr_maps_tests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(ORCMAPS_ROOT "" CACHE PATH "Reviewed OrcMaps checkout")
if(NOT EXISTS "${ORCMAPS_ROOT}/include/orcmap/pmtiles.hpp")
  message(FATAL_ERROR "Set ORCMAPS_ROOT to the reviewed OrcMaps checkout")
endif()
set(ui "${CMAKE_CURRENT_SOURCE_DIR}/../../ui")
set(engine "${ORCMAPS_ROOT}")
add_executable(orcsdr_maps_tests
  map_pipeline_tests.cpp map_inventory_tests.cpp "${ui}/map_pipeline.cpp" "${ui}/map_inventory.cpp"
  "${engine}/src/core/geo.cpp" "${engine}/src/core/viewport.cpp"
  "${engine}/src/core/pack.cpp" "${engine}/src/core/pack_json.cpp" "${engine}/src/core/pack_discovery.cpp"
  "${engine}/src/tiles/pmtiles_reader.cpp" "${engine}/src/tiles/compression.cpp"
  "${engine}/src/tiles/mvt_decoder.cpp" "${engine}/src/tiles/mvt_stream.cpp"
  "${engine}/src/tiles/mvt_translate.cpp" "${engine}/src/tiles/mvt_classify_experimental.cpp"
  "${engine}/src/render/style.cpp" "${engine}/src/render/renderer.cpp" "${engine}/src/render/clip.cpp"
  "${engine}/adapters/host/file_byte_source.cpp" "${engine}/adapters/host/framebuffer_target.cpp"
  "${engine}/third_party/miniz/miniz.c" "${engine}/third_party/miniz/miniz_tinfl.c")
target_include_directories(orcsdr_maps_tests PRIVATE "${ui}" "${engine}/include"
  "${engine}/adapters/host" "${engine}/third_party/miniz")
target_compile_definitions(orcsdr_maps_tests PRIVATE MINIZ_NO_DEFLATE_APIS
  MINIZ_NO_ZLIB_APIS MINIZ_NO_ARCHIVE_APIS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(orcsdr_maps_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(orcsdr_maps_tests PRIVATE -UNDEBUG)
endif()
enable_testing()
add_test(NAME map_pipeline COMMAND orcsdr_maps_tests "${engine}/tests/fixtures/tiny-gzip.pmtiles"
  "${engine}/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json")
