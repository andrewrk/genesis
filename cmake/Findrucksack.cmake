# Copyright (c) 2015 Andrew Kelley
# This file is MIT licensed.
# See http://opensource.org/licenses/MIT

# RUCKSACK_FOUND
# RUCKSACK_INCLUDE_DIR
# RUCKSACK_LIBRARY
# RUCKSACKSPRITESHEET_LIBRARY
# RUCKSACK_BINARY

find_path(RUCKSACK_INCLUDE_DIR NAMES rucksack/rucksack.h)

find_library(RUCKSACK_LIBRARY NAMES rucksack)
find_library(RUCKSACK_LIBRARY NAMES rucksackspritesheet)

find_program(RUCKSACK_BINARY rucksack)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RUCKSACK DEFAULT_MSG RUCKSACK_LIBRARY RUCKSACK_INCLUDE_DIR)

mark_as_advanced(RUCKSACK_INCLUDE_DIR RUCKSACK_LIBRARY RUCKSACKSPRITESHEET_LIBRARY RUCKSACK_BINARY)
