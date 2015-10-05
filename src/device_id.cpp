#include "device_id.hpp"
#include "util.hpp"
#include <string.h>

static const DeviceId all_device_ids[] = {
    DeviceIdMainOut,
    DeviceIdMainIn,
};

const char *device_id_str(DeviceId device_id) {
    switch (device_id) {
        case DeviceIdInvalid:
            panic("invalid device id");
        case DeviceIdMainOut:
            return "Main Out";
        case DeviceIdMainIn:
            return "Main In";
    }
    panic("invalid device id");
}

int device_id_count(void) {
    return array_length(all_device_ids);
}

DeviceId device_id_at(int index) {
    assert(index < array_length(all_device_ids));
    return all_device_ids[index];
}

DeviceId device_id_from_str(const char *str) {
    for (int i = 0; i < array_length(all_device_ids); i += 1) {
        const char *this_str = device_id_str(all_device_ids[i]);
        if (strcmp(this_str, str) == 0) {
            return all_device_ids[i];
        }
    }
    return DeviceIdInvalid;
}
