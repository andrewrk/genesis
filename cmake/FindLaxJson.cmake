# Copyright (c) 2014 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# LAXJSON_FOUND
# LAXJSON_INCLUDE_DIR
# LAXJSON_LIBRARY

find_path(LAXJSON_INCLUDE_DIR laxjson.h)
find_library(LAXJSON_LIBRARY NAMES laxjson)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LAXJSON DEFAULT_MSG LAXJSON_LIBRARY LAXJSON_INCLUDE_DIR)

mark_as_advanced(LAXJSON_INCLUDE_DIR LAXJSON_LIBRARY)
