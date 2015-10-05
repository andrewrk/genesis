#ifndef GENESIS_DEVICE_ID_HPP
#define GENESIS_DEVICE_ID_HPP

// modifying this structure affects project file backward compatibility
// modifying this structure affects settings file backward compatibility
// If you add to this structure, add corresponding entry in all_device_ids
enum DeviceId {
    DeviceIdInvalid,

    DeviceIdMainOut,
    DeviceIdMainIn,
};

const char *device_id_str(DeviceId device_id);

DeviceId device_id_from_str(const char *str);

int device_id_count(void);
enum DeviceId device_id_at(int index);

#endif
