add_library(mrg STATIC MRGArchive.cpp)
target_include_directories(mrg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mrg PROPERTIES CXX_STANDARD 17)
