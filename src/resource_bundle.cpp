#include "resource_bundle.hpp"

ResourceBundle::ResourceBundle(const char *filename) {
    int err = rucksack_bundle_open_read(filename, &_bundle);
    if (err)
        panic("Unable to open resource bundle: %s", rucksack_err_str(err));
}

ResourceBundle::~ResourceBundle() {
    rucksack_bundle_close(_bundle);
}

void ResourceBundle::get_file_buffer(const char *key, ByteBuffer &buffer) {
    RuckSackFileEntry *entry = rucksack_bundle_find_file(_bundle, key, -1);

    if (!entry)
        panic("could not find %s in resource bundle", key);

    long size = rucksack_file_size(entry);

    buffer.resize(size);

    int err = rucksack_file_read(entry, (unsigned char*)buffer.raw());

    if (err)
        panic("error reading '%s' resource: %s", key, rucksack_err_str(err));
}
