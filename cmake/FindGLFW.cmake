# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# GLFW_FOUND
# GLFW_INCLUDE_DIR
# GLFW_LIBRARY

find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)

find_library(GLFW_LIBRARY NAMES glfw glfw3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW DEFAULT_MSG GLFW_LIBRARY GLFW_INCLUDE_DIR)

mark_as_advanced(GLFW_INCLUDE_DIR GLFW_LIBRARY)
