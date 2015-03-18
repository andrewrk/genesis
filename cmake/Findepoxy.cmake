# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# EPOXY_FOUND
# EPOXY_INCLUDE_DIR
# EPOXY_LIBRARY

find_path(EPOXY_INCLUDE_DIR NAMES epoxy/gl.h epoxy/glx.h)

find_library(EPOXY_LIBRARY NAMES epoxy)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EPOXY DEFAULT_MSG EPOXY_LIBRARY EPOXY_INCLUDE_DIR)

mark_as_advanced(EPOXY_INCLUDE_DIR EPOXY_LIBRARY)
