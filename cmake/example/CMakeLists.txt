
file(GLOB libarc_example_files ${LIBARC_ROOT_DIR}/examples/*.cc)

foreach(libarc_example_file ${libarc_example_files})
  get_filename_component(example_name ${libarc_example_file} NAME_WE)
  add_executable(arc_${example_name} ${libarc_example_file})
  target_link_libraries(arc_${example_name} arc)
endforeach(libarc_example_file ${libarc_example_files})
