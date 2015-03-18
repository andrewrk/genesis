# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# FREETYPE_FOUND
# FREETYPE_INCLUDE_DIR
# FREETYPE_LIBRARY

find_path(FREETYPE_INCLUDE_DIR
    NAMES ft2build.h
    PATH_SUFFIXES freetype2)

find_library(FREETYPE_LIBRARY NAMES freetype)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FREETYPE DEFAULT_MSG FREETYPE_LIBRARY FREETYPE_INCLUDE_DIR)

mark_as_advanced(FREETYPE_INCLUDE_DIR FREETYPE_LIBRARY)
