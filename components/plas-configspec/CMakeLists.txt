# plas-configspec component — JSON Schema validation for device configs
# Target: plas::configspec

# --- Collect builtin specs and generate C++ source ---

file(GLOB PLAS_SCHEMA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.schema.yaml"
                            "${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.schema.json")

set(PLAS_SCHEMA_ENTRIES "")
foreach(schema_file ${PLAS_SCHEMA_FILES})
    get_filename_component(schema_filename "${schema_file}" NAME)
    # Extract driver name: "aardvark.schema.yaml" -> "aardvark"
    string(REGEX REPLACE "\\.schema\\.(yaml|yml|json)$" "" schema_name "${schema_filename}")
    file(READ "${schema_file}" schema_content)
    string(APPEND PLAS_SCHEMA_ENTRIES "    {\"${schema_name}\", R\"plas_spec(${schema_content})plas_spec\"},\n")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/configspec/builtin_schemas.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/builtin_specs.cpp"
    @ONLY
)

# --- Library target ---

add_library(plas_configspec
    src/configspec/spec_registry.cpp
    src/configspec/validator.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/builtin_specs.cpp"
)
add_library(plas::configspec ALIAS plas_configspec)

target_include_directories(plas_configspec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/configspec
)

target_link_libraries(plas_configspec
    PUBLIC plas::config
    PRIVATE nlohmann_json::nlohmann_json nlohmann_json_schema_validator yaml-cpp::yaml-cpp
)
