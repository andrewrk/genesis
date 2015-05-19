# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# RHASH_FOUND
# RHASH_INCLUDE_DIR
# RHASH_LIBRARY

find_path(RHASH_INCLUDE_DIR NAMES rhash.h)

find_library(RHASH_LIBRARY NAMES rhash)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RHASH DEFAULT_MSG RHASH_LIBRARY RHASH_INCLUDE_DIR)

mark_as_advanced(RHASH_INCLUDE_DIR RHASH_LIBRARY)
