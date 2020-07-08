function(add_ccgen_target TARGET_NAME)
  message(STATUS "setting up code generation for target \"${TARGET_NAME}\"")

  set(CCGEN_TARGET_NAME ${TARGET_NAME}.ccgen)
  set(CCGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/ccgen/${TARGET_NAME})

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  find_file(CCGEN_SCRIPT_PATH NAMES ccgen.py HINTS ${CMAKE_MODULE_PATH} REQUIRED NO_DEFAULT_PATH)
  get_filename_component(CCGEN_SCRIPT_DIR ${CCGEN_SCRIPT_PATH} DIRECTORY)
  file(GLOB CCGEN_INPUT_TEMPLATES CONFIGURE_DEPENDS ${CCGEN_SCRIPT_DIR}/templates/*.*)
  get_target_property(CCGEN_INPUT_SOURCES ${TARGET_NAME} SOURCES)

  set(CCGEN_OUTPUT_SOURCES "")
  foreach(TEMPLATE_FILENAME IN LISTS CCGEN_INPUT_TEMPLATES)
    get_filename_component(OUT_FILENAME ${TEMPLATE_FILENAME} NAME)
    list(APPEND CCGEN_OUTPUT_SOURCES "${CCGEN_OUTPUT_DIR}/${OUT_FILENAME}")
  endforeach()

  # add generated files to the target and generated group
  if(CCGEN_OUTPUT_SOURCES)

    # create files
    foreach(OUT_SOURCE IN LISTS CCGEN_OUTPUT_SOURCES)
      file(WRITE ${OUT_SOURCE} "static_assert(false, \"this file is not generated properly \");")
    endforeach()

    target_sources(${TARGET_NAME} PRIVATE ${CCGEN_OUTPUT_SOURCES})
    source_group(ccgen FILES ${CCGEN_OUTPUT_SOURCES})
    
    set(CCGEN_TARGET_EDITOR_SOURCES ${CCGEN_INPUT_TEMPLATES})
    set(CCGEN_TARGET_EDITOR_SOURCES ${CCGEN_TARGET_EDITOR_SOURCES} ${CCGEN_SCRIPT_PATH})

    add_custom_target(
      ${CCGEN_TARGET_NAME}
      ${Python3_EXECUTABLE} ${CCGEN_SCRIPT_PATH} ${CCGEN_OUTPUT_DIR} ${CCGEN_INPUT_SOURCES}
      SOURCES ${CCGEN_TARGET_EDITOR_SOURCES}
      COMMENT "ccgen: ${TARGET_NAME}"
    )

    set_target_properties(${CCGEN_TARGET_NAME} PROPERTIES FOLDER ccgen)
    add_dependencies(${TARGET_NAME} ${CCGEN_TARGET_NAME})
    target_include_directories(${TARGET_NAME} PRIVATE ${CCGEN_OUTPUT_DIR})

  endif()

endfunction()
