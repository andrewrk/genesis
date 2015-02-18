#ifndef RESOURCE_BUNDLE_HPP
#define RESOURCE_BUNDLE_HPP

#include "byte_buffer.hpp"

#include <rucksack/rucksack.h>

class ResourceBundle {
public:
    ResourceBundle(const char *filename);
    ~ResourceBundle();

    void get_file_buffer(const char *key, ByteBuffer &buffer);

    RuckSackBundle *_bundle;
};

#endif
