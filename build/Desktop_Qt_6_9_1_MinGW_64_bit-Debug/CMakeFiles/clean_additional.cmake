# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\PhotoManager_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PhotoManager_autogen.dir\\ParseCache.txt"
  "PhotoManager_autogen"
  )
endif()
