get_filename_component(acomms_examples_dir ./ ABSOLUTE)
add_definitions(-DACOMMS_EXAMPLES_DIR="${acomms_examples_dir}")

add_subdirectory(chat)
add_subdirectory(amac)
add_subdirectory(dccl)
add_subdirectory(queue)
add_subdirectory(modemdriver)
