zephyr_library_amend()
zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../include)

zephyr_library_sources_ifdef(CONFIG_INPUT_IQS9151 iqs9151.c)
