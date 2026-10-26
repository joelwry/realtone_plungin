add_executable(realtone_cli main.cpp)
set_target_properties(realtone_cli PROPERTIES OUTPUT_NAME realtone-cli)
target_link_libraries(realtone_cli PRIVATE realtone_core)
