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
  map_screen_tests.cpp "${ui}/text_editor.cpp" "${ui}/map_screen.cpp" "${ui}/setup_ui.cpp" "${ui}/settings_app.cpp"
  "${ui}/screen_controller.cpp" "${ui}/receiver_location.cpp"
  "${ui}/focus_nav.cpp" "${ui}/keyboard_input.cpp" "${ui}/clock_settings.cpp"
  map_pipeline_tests.cpp map_inventory_tests.cpp map_world_tests.cpp "${ui}/map_pipeline.cpp" "${ui}/map_inventory.cpp"
  "${engine}/src/core/geo.cpp" "${engine}/src/core/viewport.cpp"
  "${engine}/src/core/location_picker.cpp"
  "${engine}/src/core/pack.cpp" "${engine}/src/core/pack_json.cpp" "${engine}/src/core/pack_discovery.cpp"
  "${engine}/src/tiles/pmtiles_reader.cpp" "${engine}/src/tiles/compression.cpp"
  "${engine}/src/tiles/mvt_decoder.cpp" "${engine}/src/tiles/mvt_stream.cpp"
  "${engine}/src/tiles/mvt_translate.cpp" "${engine}/src/tiles/mvt_classify_experimental.cpp"
  "${engine}/src/render/style.cpp" "${engine}/src/render/renderer.cpp" "${engine}/src/render/clip.cpp"
  "${engine}/src/render/labels.cpp"
  "${engine}/adapters/host/file_byte_source.cpp" "${engine}/adapters/host/framebuffer_target.cpp"
  "${engine}/third_party/miniz/miniz.c" "${engine}/third_party/miniz/miniz_tinfl.c")
target_include_directories(orcsdr_maps_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ui_stubs" "${ui}" "${engine}/include"
  "${engine}/adapters/host" "${engine}/third_party/miniz")
target_compile_definitions(orcsdr_maps_tests PRIVATE MINIZ_NO_DEFLATE_APIS
  MINIZ_NO_ZLIB_APIS MINIZ_NO_ARCHIVE_APIS _CRT_SECURE_NO_WARNINGS)
if(MSVC)
  target_compile_options(orcsdr_maps_tests PRIVATE /UNDEBUG /EHsc)
else()
  target_compile_options(orcsdr_maps_tests PRIVATE -UNDEBUG)
endif()
enable_testing()
add_executable(orcsdr_map_preview_tests map_preview_tests.cpp
  "${engine}/src/core/geo.cpp" "${engine}/src/core/viewport.cpp")
target_include_directories(orcsdr_map_preview_tests PRIVATE "${ui}" "${engine}/include")
if(MSVC)
  target_compile_options(orcsdr_map_preview_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(orcsdr_map_preview_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME map_preview COMMAND orcsdr_map_preview_tests)
add_executable(orcsdr_map_timing_tests map_timing_tests.cpp)
target_include_directories(orcsdr_map_timing_tests PRIVATE "${ui}")
if(MSVC)
  target_compile_options(orcsdr_map_timing_tests PRIVATE /UNDEBUG)
else()
  target_compile_options(orcsdr_map_timing_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME map_timing COMMAND orcsdr_map_timing_tests)
add_executable(orcsdr_map_download_tests map_download_response_tests.cpp)
target_include_directories(orcsdr_map_download_tests PRIVATE "${ui}")
add_test(NAME map_download_response COMMAND orcsdr_map_download_tests)
add_executable(orcsdr_setup_ui_tests setup_ui_tests.cpp "${ui}/setup_ui.cpp"
  "${ui}/text_editor.cpp" "${ui}/keyboard_input.cpp" "${ui}/focus_nav.cpp")
target_include_directories(orcsdr_setup_ui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ui_stubs" "${ui}")
if(MSVC)
  target_compile_options(orcsdr_setup_ui_tests PRIVATE /UNDEBUG /EHsc)
else()
  target_compile_options(orcsdr_setup_ui_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME setup_ui COMMAND orcsdr_setup_ui_tests)
add_executable(orcsdr_sd_dma_tests ../sdmmc_dma_buffer_tests.cpp)
target_include_directories(orcsdr_sd_dma_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ui_stubs" "${ui}")
if(MSVC)
  target_compile_options(orcsdr_sd_dma_tests PRIVATE /UNDEBUG /EHsc)
else()
  target_compile_options(orcsdr_sd_dma_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME sd_dma_buffer COMMAND orcsdr_sd_dma_tests)
add_executable(orcsdr_setup_tests setup_flow_tests.cpp)
target_include_directories(orcsdr_setup_tests PRIVATE "${ui}")
if(MSVC)
  target_compile_options(orcsdr_setup_tests PRIVATE /UNDEBUG /EHsc)
else()
  target_compile_options(orcsdr_setup_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME setup_flow COMMAND orcsdr_setup_tests)
add_executable(orcsdr_location_store_tests location_store_tests.cpp)
target_include_directories(orcsdr_location_store_tests PRIVATE "${ui}")
if(MSVC)
  target_compile_options(orcsdr_location_store_tests PRIVATE /UNDEBUG /EHsc)
else()
  target_compile_options(orcsdr_location_store_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME location_store COMMAND orcsdr_location_store_tests)
add_test(NAME map_pipeline COMMAND orcsdr_maps_tests "${engine}/tests/fixtures/tiny-gzip.pmtiles"
  "${engine}/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json")
set(ORCSDR_WORLD_FIXTURE "" CACHE FILEPATH "Verified optional world archive")
if(ORCSDR_WORLD_FIXTURE)
  file(SHA256 "${ORCSDR_WORLD_FIXTURE}" world_sha)
  if(NOT world_sha STREQUAL "9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f")
    message(FATAL_ERROR "Unexpected world fixture hash")
  endif()
  add_test(NAME world_geometry COMMAND orcsdr_maps_tests "${engine}/tests/fixtures/tiny-gzip.pmtiles"
    "${engine}/examples/m5stack-tab5/test-pack/springfield-97477.manifest.json" "${ORCSDR_WORLD_FIXTURE}")
endif()
