# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# PNG_FOUND
# PNG_INCLUDE_DIR
# PNG_LIBRARY

find_path(PNG_INCLUDE_DIR NAMES png.h)

find_library(PNG_LIBRARY NAMES png)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PNG DEFAULT_MSG PNG_LIBRARY PNG_INCLUDE_DIR)

mark_as_advanced(PNG_INCLUDE_DIR PNG_LIBRARY)
