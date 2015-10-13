#include "project.hpp"
#include "audio_graph.hpp"

#include <limits.h>

// modifying this structure affects project file backward compatibility
enum PropKey {
    PropKeyInvalid,
    PropKeyDelimiter,
    PropKeyProjectId,
    PropKeyTrack,
    PropKeyCommand,
    PropKeyUser,
    PropKeyUndoStack,
    PropKeyUndoStackIndex,
    PropKeyAudioAsset,
    PropKeyAudioClip,
    PropKeyAudioClipSegment,
    PropKeyMixerLine,
    PropKeyEffect,
    PropKeySampleRate,
    PropKeyTagArtist,
    PropKeyTagTitle,
    PropKeyTagAlbumArtist,
    PropKeyTagAlbum,
    PropKeyTagYear,
};

static const int PROP_KEY_SIZE = 4;
static const int UINT256_SIZE = 32;

// modifying this structure affects project file backward compatibility
enum SerializableFieldKey {
    SerializableFieldKeyInvalid,
    SerializableFieldKeyUserId,
    SerializableFieldKeyRevision,
    SerializableFieldKeyName,
    SerializableFieldKeySortKey,
    SerializableFieldKeyTrackId,
    SerializableFieldKeyPayload,
    SerializableFieldKeyOtherCommandId,
    SerializableFieldKeyCmdType,
    SerializableFieldKeyCommandChild,
    SerializableFieldKeyPath,
    SerializableFieldKeyAudioAssetId,
    SerializableFieldKeySha256Sum,
    SerializableFieldKeyAudioClipId,
    SerializableFieldKeyAudioClipSegmentId,
    SerializableFieldKeyStart,
    SerializableFieldKeyEnd,
    SerializableFieldKeyPos,
    SerializableFieldKeySolo,
    SerializableFieldKeyVolume,
    SerializableFieldKeyMixerLineId,
    SerializableFieldKeyEffectType,
    SerializableFieldKeyEffectChild,
    SerializableFieldKeyEffectSendType,
    SerializableFieldKeyEffectSendChild,
    SerializableFieldKeyGain,
    SerializableFieldKeyDeviceId,
};

// modifying this structure affects project file backward compatibility
enum SerializableFieldType {
    SerializableFieldTypeInvalid,
    SerializableFieldTypeUInt32,
    SerializableFieldTypeEffectChild,
    SerializableFieldTypeUInt32AsInt,
    SerializableFieldTypeString,
    SerializableFieldTypeByteBuffer,
    SerializableFieldTypeUInt256,
    SerializableFieldTypeSortKey,
    SerializableFieldTypeCmdType,
    SerializableFieldTypeCommandChild,
    SerializableFieldTypeUInt64AsLong,
    SerializableFieldTypeDouble,
    SerializableFieldTypeUInt8,
    SerializableFieldTypeFloat,
    SerializableFieldTypeEffectSendChild,
};

template <typename T>
struct SerializableField {
    SerializableFieldKey key;
    SerializableFieldType type;
    void *(*get_field_ptr)(T *);
    void (*set_default_value)(T *);
};

static int deserialize_from_enum(void *ptr, SerializableFieldType type, const ByteBuffer &buffer, int *offset);
static void serialize_effect(Effect *effect, ByteBuffer &buffer);
static void serialize_effect_send(EffectSend *effect_send, ByteBuffer &buffer);

static const SerializableField<Track> *get_serializable_fields(Track *) {
    static const SerializableField<Track> fields[] = {
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](Track *track) -> void * {
                return &track->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeSortKey,
            [](Track *track) -> void * {
                return &track->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<MixerLine> *get_serializable_fields(MixerLine *) {
    static const SerializableField<MixerLine> fields[] = {
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](MixerLine *self) -> void * {
                return &self->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeSortKey,
            [](MixerLine *self) -> void * {
                return &self->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeySolo,
            SerializableFieldTypeUInt8,
            [](MixerLine *self) -> void * {
                return &self->solo;
            },
            nullptr,
        },
        {
            SerializableFieldKeyVolume,
            SerializableFieldTypeFloat,
            [](MixerLine *self) -> void * {
                return &self->volume;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<User> *get_serializable_fields(User *) {
    static const SerializableField<User> fields[] = {
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](User *user) -> void * {
                return &user->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AudioAsset> *get_serializable_fields(AudioAsset *) {
    static const SerializableField<AudioAsset> fields[] = {
        {
            SerializableFieldKeyPath,
            SerializableFieldTypeByteBuffer,
            [](AudioAsset *audio_asset) -> void * {
                return &audio_asset->path;
            },
            nullptr,
        },
        {
            SerializableFieldKeySha256Sum,
            SerializableFieldTypeByteBuffer,
            [](AudioAsset *audio_asset) -> void * {
                return &audio_asset->sha256sum;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AudioClip> *get_serializable_fields(AudioClip *) {
    static const SerializableField<AudioClip> fields[] = {
        {
            SerializableFieldKeyAudioAssetId,
            SerializableFieldTypeUInt256,
            [](AudioClip *audio_clip) -> void * {
                return &audio_clip->audio_asset_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](AudioClip *audio_clip) -> void * {
                return &audio_clip->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AudioClipSegment> *get_serializable_fields(AudioClipSegment *) {
    static const SerializableField<AudioClipSegment> fields[] = {
        {
            SerializableFieldKeyAudioClipId,
            SerializableFieldTypeUInt256,
            [](AudioClipSegment *segment) -> void * {
                return &segment->audio_clip_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](AudioClipSegment *segment) -> void * {
                return &segment->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyStart,
            SerializableFieldTypeUInt64AsLong,
            [](AudioClipSegment *segment) -> void * {
                return &segment->start;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEnd,
            SerializableFieldTypeUInt64AsLong,
            [](AudioClipSegment *segment) -> void * {
                return &segment->end;
            },
            nullptr,
        },
        {
            SerializableFieldKeyPos,
            SerializableFieldTypeDouble,
            [](AudioClipSegment *segment) -> void * {
                return &segment->pos;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<Effect> *get_serializable_fields(Effect *) {
    static const SerializableField<Effect> fields[] = {
        {
            SerializableFieldKeyMixerLineId,
            SerializableFieldTypeUInt256,
            [](Effect *self) -> void * {
                return &self->mixer_line_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeSortKey,
            [](Effect *self) -> void * {
                return &self->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEffectType,
            SerializableFieldTypeUInt32AsInt,
            [](Effect *self) -> void * {
                return &self->effect_type;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEffectChild,
            SerializableFieldTypeEffectChild,
            [](Effect *self) -> void * {
                return self;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<EffectSend> *get_serializable_fields(EffectSend *) {
    static const SerializableField<EffectSend> fields[] = {
        {
            SerializableFieldKeyGain,
            SerializableFieldTypeFloat,
            [](EffectSend *self) -> void * {
                return &self->gain;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEffectSendType,
            SerializableFieldTypeUInt32AsInt,
            [](EffectSend *self) -> void * {
                return &self->send_type;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEffectSendChild,
            SerializableFieldTypeEffectSendChild,
            [](EffectSend *self) -> void * {
                return self;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<EffectSendDevice> *get_serializable_fields(EffectSendDevice *) {
    static const SerializableField<EffectSendDevice> fields[] = {
        {
            SerializableFieldKeyDeviceId,
            SerializableFieldTypeUInt32AsInt,
            [](EffectSendDevice *self) -> void * {
                return &self->device_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<Command> *get_serializable_fields(Command *) {
    static const SerializableField<Command> fields[] = {
        {
            SerializableFieldKeyCmdType,
            SerializableFieldTypeCmdType,
            [](Command *cmd) -> void * {
                return cmd;
            },
            nullptr,
        },
        {
            SerializableFieldKeyUserId,
            SerializableFieldTypeUInt256,
            [](Command *cmd) -> void * {
                return &cmd->user_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyRevision,
            SerializableFieldTypeUInt32AsInt,
            [](Command *cmd) -> void * {
                return &cmd->revision;
            },
            nullptr,
        },
        {
            SerializableFieldKeyCommandChild,
            SerializableFieldTypeCommandChild,
            [](Command *cmd) -> void * {
                return cmd;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AddTrackCommand> *get_serializable_fields(AddTrackCommand *) {
    static const SerializableField<AddTrackCommand> fields[] = {
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeySortKey,
            SerializableFieldTypeSortKey,
            [](AddTrackCommand *cmd) -> void * {
                return &cmd->sort_key;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<DeleteTrackCommand> *get_serializable_fields(DeleteTrackCommand *) {
    static const SerializableField<DeleteTrackCommand> fields[] = {
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](DeleteTrackCommand *cmd) -> void * {
                return &cmd->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyPayload,
            SerializableFieldTypeByteBuffer,
            [](DeleteTrackCommand *cmd) -> void * {
                return &cmd->payload;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AddAudioClipCommand> *get_serializable_fields(AddAudioClipCommand *) {
    static const SerializableField<AddAudioClipCommand> fields[] = {
        {
            SerializableFieldKeyAudioClipId,
            SerializableFieldTypeUInt256,
            [](AddAudioClipCommand *cmd) -> void * {
                return &cmd->audio_clip_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyAudioAssetId,
            SerializableFieldTypeUInt256,
            [](AddAudioClipCommand *cmd) -> void * {
                return &cmd->audio_asset_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyName,
            SerializableFieldTypeString,
            [](AddAudioClipCommand *cmd) -> void * {
                return &cmd->name;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<AddAudioClipSegmentCommand> *get_serializable_fields(AddAudioClipSegmentCommand *) {
    static const SerializableField<AddAudioClipSegmentCommand> fields[] = {
        {
            SerializableFieldKeyAudioClipSegmentId,
            SerializableFieldTypeUInt256,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->audio_clip_segment_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyAudioClipId,
            SerializableFieldTypeUInt256,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->audio_clip_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyTrackId,
            SerializableFieldTypeUInt256,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->track_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyStart,
            SerializableFieldTypeUInt64AsLong,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->start;
            },
            nullptr,
        },
        {
            SerializableFieldKeyEnd,
            SerializableFieldTypeUInt64AsLong,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->end;
            },
            nullptr,
        },
        {
            SerializableFieldKeyPos,
            SerializableFieldTypeDouble,
            [](AddAudioClipSegmentCommand *cmd) -> void * {
                return &cmd->pos;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<UndoCommand> *get_serializable_fields(UndoCommand *) {
    static const SerializableField<UndoCommand> fields[] = {
        {
            SerializableFieldKeyOtherCommandId,
            SerializableFieldTypeUInt256,
            [](UndoCommand *cmd) -> void * {
                return &cmd->other_command_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static const SerializableField<RedoCommand> *get_serializable_fields(RedoCommand *) {
    static const SerializableField<RedoCommand> fields[] = {
        {
            SerializableFieldKeyOtherCommandId,
            SerializableFieldTypeUInt256,
            [](RedoCommand *cmd) -> void * {
                return &cmd->other_command_id;
            },
            nullptr,
        },
        {
            SerializableFieldKeyInvalid,
            SerializableFieldTypeInvalid,
            nullptr,
            nullptr,
        },
    };
    return fields;
}

static void serialize_uint256(ByteBuffer &buf, const uint256 &val) {
    buf.resize(buf.length() + UINT256_SIZE);
    val.write_be(buf.raw() + buf.length() - UINT256_SIZE);
}

static void serialize_from_enum(void *ptr, SerializableFieldType type, ByteBuffer &buffer) {
    switch (type) {
    case SerializableFieldTypeInvalid:
        panic("invalid serialize field type");
    case SerializableFieldTypeUInt32:
        {
            uint32_t *value = static_cast<uint32_t *>(ptr);
            buffer.append_uint32be(*value);
            break;
        }
    case SerializableFieldTypeUInt8:
        {
            uint8_t *value = static_cast<uint8_t *>(ptr);
            buffer.append_uint8(*value);
            break;
        }
    case SerializableFieldTypeUInt32AsInt:
        {
            int *value = static_cast<int *>(ptr);
            buffer.append_uint32be(*value);
            break;
        }
    case SerializableFieldTypeUInt64AsLong:
        {
            long *value = static_cast<long *>(ptr);
            buffer.append_uint64be(*value);
            break;
        }
    case SerializableFieldTypeUInt256:
        {
            uint256 *value = static_cast<uint256 *>(ptr);
            serialize_uint256(buffer, *value);
            break;
        }
    case SerializableFieldTypeByteBuffer:
        {
            ByteBuffer *value = static_cast<ByteBuffer *>(ptr);
            buffer.append_uint32be(value->length());
            buffer.append(*value);
            break;
        }
    case SerializableFieldTypeString:
        {
            String *value = static_cast<String *>(ptr);
            ByteBuffer encoded = value->encode();
            buffer.append_uint32be(encoded.length());
            buffer.append(encoded);
            break;
        }
    case SerializableFieldTypeSortKey:
        {
            SortKey *value = static_cast<SortKey *>(ptr);
            value->serialize(buffer);
            break;
        }
    case SerializableFieldTypeCommandChild:
        {
            Command *value = static_cast<Command *>(ptr);
            value->serialize(buffer);
            break;
        }
    case SerializableFieldTypeEffectChild:
        {
            Effect *value = static_cast<Effect *>(ptr);
            serialize_effect(value, buffer);
            break;
        }
    case SerializableFieldTypeEffectSendChild:
        {
            EffectSend *value = static_cast<EffectSend *>(ptr);
            serialize_effect_send(value, buffer);
            break;
        }
    case SerializableFieldTypeCmdType:
        {
            Command *value = static_cast<Command *>(ptr);
            buffer.append_uint32be(value->command_type());
            break;
        }
    case SerializableFieldTypeDouble:
        {
            double *value = static_cast<double *>(ptr);
            buffer.append_double(*value);
            break;
        }
    case SerializableFieldTypeFloat:
        {
            float *value = static_cast<float *>(ptr);
            buffer.append_float(*value);
            break;
        }
    }
}

template<typename T>
static void serialize_object(T *obj, ByteBuffer &buffer) {
    const SerializableField<T> *serializable_fields = get_serializable_fields(obj);

    // iterate once to count the fields
    int field_count = 0;
    const SerializableField<T> *it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        assert(it->get_field_ptr);
        field_count += 1;
        it += 1;
    }

    buffer.append_uint32be(field_count);

    // iterate again and serialize everything
    it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        int field_length_offset = buffer.length();
        buffer.resize(buffer.length() + 4);

        buffer.append_uint32be(it->key);
        serialize_from_enum(it->get_field_ptr(obj), it->type, buffer);

        int field_size = buffer.length() - field_length_offset;
        write_uint32be(buffer.raw() + field_length_offset, field_size);

        it += 1;
    }
}

static void serialize_effect(Effect *effect, ByteBuffer &buffer) {
    switch ((EffectType)effect->effect_type) {
        case EffectTypeSend:
            serialize_object(&effect->effect.send, buffer);
            return;
    }
    panic("invalid effect type");
}

static void serialize_effect_send(EffectSend *effect_send, ByteBuffer &buffer) {
    switch ((EffectSendType)effect_send->send_type) {
        case EffectSendTypeDevice:
            serialize_object(&effect_send->send.device, buffer);
            return;
    }
    panic("invalid effect type");
}


static int deserialize_double(double *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 8)
        return GenesisErrorInvalidFormat;

    const double *buffer_ptr = reinterpret_cast<const double*>(buffer.raw() + *offset);
    *x = *buffer_ptr;
    *offset += 8;
    return 0;
}

static int deserialize_float(float *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    const float *buffer_ptr = reinterpret_cast<const float*>(buffer.raw() + *offset);
    *x = *buffer_ptr;
    *offset += 4;
    return 0;
}

static int deserialize_uint32be(uint32_t *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    *x = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    return 0;
}

static int deserialize_uint8(uint8_t *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 1)
        return GenesisErrorInvalidFormat;

    *x = *((uint8_t*)(buffer.raw() + *offset));
    *offset += 1;
    return 0;
}

static int deserialize_uint64be(uint64_t *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 8)
        return GenesisErrorInvalidFormat;

    *x = read_uint64be(buffer.raw() + *offset);
    *offset += 8;
    return 0;
}

static int deserialize_uint32be_as_int(int *x, const ByteBuffer &buffer, int *offset) {
    uint32_t unsigned_x;
    int err;
    if ((err = deserialize_uint32be(&unsigned_x, buffer, offset))) return err;
    if (unsigned_x > (uint32_t)INT_MAX) return GenesisErrorInvalidFormat;
    *x = unsigned_x;
    return 0;
}

static int deserialize_uint64be_as_long(long *x, const ByteBuffer &buffer, int *offset) {
    uint64_t unsigned_x;
    int err;
    if ((err = deserialize_uint64be(&unsigned_x, buffer, offset))) return err;
    if (unsigned_x > (uint64_t)LONG_MAX) return GenesisErrorInvalidFormat;
    *x = unsigned_x;
    return 0;
}

static int deserialize_byte_buffer(ByteBuffer &out, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < 4)
        return GenesisErrorInvalidFormat;

    int len = read_uint32be(buffer.raw() + *offset);
    *offset += 4;
    if (buffer.length() - *offset < len)
        return GenesisErrorInvalidFormat;

    out.clear();
    out.append(buffer.raw() + *offset, len);
    *offset += len;

    return 0;
}

static int deserialize_string(String &out, const ByteBuffer &buffer, int *offset) {
    ByteBuffer encoded;
    int err;
    if ((err = deserialize_byte_buffer(encoded, buffer, offset))) return err;

    bool ok;
    out = String(encoded, &ok);
    if (!ok)
        return GenesisErrorInvalidFormat;

    return 0;
}

static int deserialize_uint256(uint256 *x, const ByteBuffer &buffer, int *offset) {
    if (buffer.length() - *offset < UINT256_SIZE)
        return GenesisErrorInvalidFormat;

    *x = uint256::read_be(buffer.raw() + *offset);
    *offset += UINT256_SIZE;
    return 0;
}

template<typename T>
static int deserialize_object(T *obj, const ByteBuffer &buffer, int *offset) {
    const SerializableField<T> *serializable_fields = get_serializable_fields(obj);

    int err;
    struct Item {
        bool found;
        const SerializableField<T> *field;
    };
    List<Item> items;
    const SerializableField<T> *it = serializable_fields;
    while (it->key != SerializableFieldKeyInvalid) {
        if (items.append({false, it}))
            return GenesisErrorNoMem;
        it += 1;
    }

    int field_count;
    if ((err = deserialize_uint32be_as_int(&field_count, buffer, offset))) return err;
    for (int field_i = 0; field_i < field_count; field_i += 1) {
        int field_offset = *offset;

        int field_size;
        if ((err = deserialize_uint32be_as_int(&field_size, buffer, offset))) return err;

        int field_key;
        if ((err = deserialize_uint32be_as_int(&field_key, buffer, offset))) return err;

        bool found_this_field = false;
        for (int item_i = 0; item_i < items.length(); item_i += 1) {
            Item *item = &items.at(item_i);
            if (item->field->key == field_key) {
                found_this_field = true;
                item->found = true;
                if ((err = deserialize_from_enum(item->field->get_field_ptr(obj), item->field->type, buffer, offset)))
                    return err;
                break;
            }
        }
        if (!found_this_field) {
            // skip this field
            *offset = field_offset + field_size;
        }
        assert(*offset == field_offset + field_size);
    }

    // call default callbacks on unfound fields
    for (int item_i = 0; item_i < items.length(); item_i += 1) {
        Item *item = &items.at(item_i);
        if (!item->found) {
            if (!item->field->set_default_value)
                return GenesisErrorInvalidFormat;
            item->field->set_default_value(obj);
        }
    }

    return 0;
}

static int deserialize_from_enum(void *ptr, SerializableFieldType type, const ByteBuffer &buffer, int *offset) {
    switch (type) {
    case SerializableFieldTypeInvalid:
        panic("invalid serialize field type");
    case SerializableFieldTypeUInt32:
        {
            uint32_t *value = static_cast<uint32_t *>(ptr);
            return deserialize_uint32be(value, buffer, offset);
        }
    case SerializableFieldTypeUInt8:
        {
            uint8_t *value = static_cast<uint8_t *>(ptr);
            return deserialize_uint8(value, buffer, offset);
        }
    case SerializableFieldTypeUInt32AsInt:
        {
            int *value = static_cast<int *>(ptr);
            return deserialize_uint32be_as_int(value, buffer, offset);
        }
    case SerializableFieldTypeUInt64AsLong:
        {
            long *value = static_cast<long *>(ptr);
            return deserialize_uint64be_as_long(value, buffer, offset);
        }
    case SerializableFieldTypeUInt256:
        {
            uint256 *value = static_cast<uint256 *>(ptr);
            return deserialize_uint256(value, buffer, offset);
        }
    case SerializableFieldTypeByteBuffer:
        {
            ByteBuffer *value = static_cast<ByteBuffer *>(ptr);
            return deserialize_byte_buffer(*value, buffer, offset);
        }
    case SerializableFieldTypeString:
        {
            String *value = static_cast<String *>(ptr);
            return deserialize_string(*value, buffer, offset);
        }
    case SerializableFieldTypeSortKey:
        {
            SortKey *value = static_cast<SortKey *>(ptr);
            return value->deserialize(buffer, offset);
        }
    case SerializableFieldTypeEffectChild:
        {
            Effect *effect = static_cast<Effect *>(ptr);
            switch ((EffectType)effect->effect_type) {
                case EffectTypeSend:
                    return deserialize_object(&effect->effect.send, buffer, offset);
            }
            panic("unreachable");
        }
    case SerializableFieldTypeEffectSendChild:
        {
            EffectSend *effect_send = static_cast<EffectSend *>(ptr);
            switch ((EffectSendType)effect_send->send_type) {
                case EffectSendTypeDevice:
                    return deserialize_object(&effect_send->send.device, buffer, offset);
            }
            panic("unreachable");
        }
    case SerializableFieldTypeCommandChild:
        {
            Command *cmd = static_cast<Command *>(ptr);
            switch (cmd->command_type()) {
                case CommandTypeInvalid:
                    panic("invalid command type");
                case CommandTypeUndo:
                    return deserialize_object(reinterpret_cast<UndoCommand*>(cmd), buffer, offset);
                case CommandTypeRedo:
                    return deserialize_object(reinterpret_cast<RedoCommand*>(cmd), buffer, offset);
                case CommandTypeAddTrack:
                    return deserialize_object(reinterpret_cast<AddTrackCommand*>(cmd), buffer, offset);
                case CommandTypeDeleteTrack:
                    return deserialize_object(reinterpret_cast<DeleteTrackCommand*>(cmd), buffer, offset);
                case CommandTypeAddAudioClip:
                    return deserialize_object(reinterpret_cast<AddAudioClipCommand*>(cmd), buffer, offset);
                case CommandTypeAddAudioClipSegment:
                    return deserialize_object(reinterpret_cast<AddAudioClipSegmentCommand*>(cmd), buffer, offset);
            }
            panic("unreachable");
        }
    case SerializableFieldTypeCmdType:
        // ignore
        *offset += 4;
        return 0;
    case SerializableFieldTypeDouble:
        {
            double *value = static_cast<double *>(ptr);
            return deserialize_double(value, buffer, offset);
        }
    case SerializableFieldTypeFloat:
        {
            float *value = static_cast<float *>(ptr);
            return deserialize_float(value, buffer, offset);
        }
    }
    panic("unreachable");
}

static OrderedMapFileBuffer *omf_buf_uint256(const uint256 &value) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(UINT256_SIZE));
    value.write_be(buf->data);
    return buf;
}

static OrderedMapFileBuffer *omf_buf_uint32(uint32_t x) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(4));
    write_uint32be(buf->data, x);
    return buf;
}

static OrderedMapFileBuffer *omf_buf_byte_buffer(const ByteBuffer &byte_buffer) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(byte_buffer.length()));
    memcpy(buf->data, byte_buffer.raw(), byte_buffer.length());
    return buf;
}

static OrderedMapFileBuffer *omf_buf_string(const String &string) {
    ByteBuffer encoded = string.encode();
    return omf_buf_byte_buffer(encoded);
}

template<typename T>
static OrderedMapFileBuffer *omf_buf_obj(T *obj) {
    ByteBuffer buffer;
    serialize_object(obj, buffer);
    return omf_buf_byte_buffer(buffer);
}

static int compare_tracks(Track *a, Track *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? uint256::compare(a->id, b->id) : sort_key_cmp;
}

static int compare_users(User *a, User *b) {
    return uint256::compare(a->id, b->id);
}

static int compare_commands(Command *a, Command *b) {
    if (a->revision > b->revision)
        return 1;
    else if (a->revision < b->revision)
        return -1;
    else
        return uint256::compare(a->id, b->id);
}

static int compare_audio_assets(AudioAsset *a, AudioAsset *b) {
    return ByteBuffer::compare(a->path, b->path);
}

static int compare_audio_clips(AudioClip *a, AudioClip *b) {
    int cmp = String::compare(a->name, b->name);
    if (cmp != 0)
        return cmp;

    return uint256::compare(a->id, b->id);
}

static int compare_mixer_lines(MixerLine *a, MixerLine *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? uint256::compare(a->id, b->id) : sort_key_cmp;
}

static int compare_audio_clip_segments(AudioClipSegment *a, AudioClipSegment *b) {
    if (a->pos < b->pos)
        return -1;
    else if (a->pos > b->pos)
        return 1;
    else
        return uint256::compare(a->id, b->id);
}

static int compare_effects(Effect *a, Effect *b) {
    int sort_key_cmp = SortKey::compare(a->sort_key, b->sort_key);
    return (sort_key_cmp == 0) ? uint256::compare(a->id, b->id) : sort_key_cmp;
}

template<typename T, int (*compare)(T, T)>
static void project_sort_item(List<T> &list, IdMap<T> &id_map) {
    list.clear();
    auto it = id_map.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        ok_or_panic(list.append(entry->value));
    }
    quick_sort<T, compare>(list.raw(), list.length());
}

static void project_sort_tracks(Project *project) {
    project_sort_item<Track *, compare_tracks>(project->track_list, project->tracks);
}

static void project_sort_users(Project *project) {
    project_sort_item<User *, compare_users>(project->user_list, project->users);
}

static void project_sort_commands(Project *project) {
    project_sort_item<Command *, compare_commands>(project->command_list, project->commands);
}

static void project_sort_audio_assets(Project *project) {
    project_sort_item<AudioAsset *, compare_audio_assets>(project->audio_asset_list, project->audio_assets);
}

static void project_sort_audio_clips(Project *project) {
    project_sort_item<AudioClip *, compare_audio_clips>(project->audio_clip_list, project->audio_clips);
}

static void project_sort_mixer_lines(Project *project) {
    project_sort_item<MixerLine *, compare_mixer_lines>(project->mixer_line_list, project->mixer_lines);
}

static void project_sort_audio_clip_segments(Project *project) {
    for (int i = 0; i < project->track_list.length(); i += 1) {
        Track *track = project->track_list.at(i);
        track->audio_clip_segments.clear();
    }

    for (int i = 0; i < project->audio_clip_list.length(); i += 1) {
        AudioClip *audio_clip = project->audio_clip_list.at(i);
        audio_clip->events_write_ptr = audio_clip->events.write_begin();
        audio_clip->events_write_ptr->clear();
    }

    auto it = project->audio_clip_segments.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        AudioClipSegment *segment = entry->value;
        ok_or_panic(segment->track->audio_clip_segments.append(segment));

        AudioClip *audio_clip = segment->audio_clip;
        ok_or_panic(audio_clip->events_write_ptr->add_one());
        GenesisMidiEvent *event = &audio_clip->events_write_ptr->last();
        event->event_type = GenesisMidiEventTypeSegment;
        event->start = segment->pos;
        event->data.segment_data.start = segment->start;
        event->data.segment_data.end = segment->end;
    }

    for (int i = 0; i < project->audio_clip_list.length(); i += 1) {
        AudioClip *audio_clip = project->audio_clip_list.at(i);
        audio_clip->events.write_end();
        audio_clip->events_write_ptr = nullptr;
    }

    for (int i = 0; i < project->track_list.length(); i += 1) {
        Track *track = project->track_list.at(i);
        track->audio_clip_segments.sort<compare_audio_clip_segments>();
    }
}

static void project_sort_effects(Project *project) {
    for (int i = 0; i < project->mixer_line_list.length(); i += 1) {
        MixerLine *mixer_line = project->mixer_line_list.at(i);
        mixer_line->effects.clear();
    }

    auto it = project->effects.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;

        Effect *effect = entry->value;
        ok_or_panic(effect->mixer_line->effects.append(effect));
    }

    for (int i = 0; i < project->mixer_line_list.length(); i += 1) {
        MixerLine *mixer_line = project->mixer_line_list.at(i);
        mixer_line->effects.sort<compare_effects>();
    }
}

static void trigger_event(Project *project, Event event) {
    project->events.trigger(event);
}

static void project_compute_indexes(Project *project) {
    if (project->track_list_dirty) project_sort_tracks(project);
    if (project->user_list_dirty) project_sort_users(project);
    if (project->command_list_dirty) project_sort_commands(project);
    if (project->audio_asset_list_dirty) project_sort_audio_assets(project);
    if (project->audio_clip_list_dirty) project_sort_audio_clips(project);
    if (project->mixer_line_list_dirty) project_sort_mixer_lines(project);
    // depends on tracks being sorted
    if (project->audio_clip_segments_dirty) project_sort_audio_clip_segments(project);
    // depends on mixer lines being sorted
    if (project->effects_dirty) project_sort_effects(project);

    if (project->track_list_dirty) {
        project->track_list_dirty = false;
        trigger_event(project, EventProjectTracksChanged);
    }
    if (project->user_list_dirty) {
        project->user_list_dirty = false;
        trigger_event(project, EventProjectUsersChanged);
    }
    if (project->command_list_dirty) {
        project->command_list_dirty = false;
        trigger_event(project, EventProjectCommandsChanged);
    }
    if (project->audio_asset_list_dirty) {
        project->audio_asset_list_dirty = false;
        trigger_event(project, EventProjectAudioAssetsChanged);
    }
    if (project->audio_clip_list_dirty) {
        project->audio_clip_list_dirty = false;
        trigger_event(project, EventProjectAudioClipsChanged);
    }
    if (project->mixer_line_list_dirty) {
        project->mixer_line_list_dirty = false;
        trigger_event(project, EventProjectMixerLinesChanged);
    }
    if (project->audio_clip_segments_dirty) {
        project->audio_clip_segments_dirty = false;
        trigger_event(project, EventProjectAudioClipSegmentsChanged);
    }
    if (project->effects_dirty) {
        project->effects_dirty = false;
        trigger_event(project, EventProjectEffectsChanged);
    }
}

int project_get_next_revision(Project *project) {
    project_compute_indexes(project);

    if (project->command_list.length() == 0)
        return 0;

    Command *last_command = project->command_list.at(project->command_list.length() - 1);
    return last_command->revision + 1;
}

static OrderedMapFileBuffer *create_undo_stack_key(int index) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(PROP_KEY_SIZE + PROP_KEY_SIZE + 4));
    write_uint32be(&buf->data[0], PropKeyUndoStack);
    write_uint32be(&buf->data[4], PropKeyDelimiter);
    write_uint32be(&buf->data[8], index);
    return buf;
}

static OrderedMapFileBuffer *create_id_key(PropKey prop_key, const uint256 &id) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(PROP_KEY_SIZE + PROP_KEY_SIZE + UINT256_SIZE));
    write_uint32be(&buf->data[0], prop_key);
    write_uint32be(&buf->data[4], PropKeyDelimiter);
    id.write_be(&buf->data[8]);
    return buf;
}

static OrderedMapFileBuffer *create_effect_key(const uint256 &id) {
    return create_id_key(PropKeyEffect, id);
}

static OrderedMapFileBuffer *create_mixer_line_key(const uint256 &id) {
    return create_id_key(PropKeyMixerLine, id);
}

static OrderedMapFileBuffer *create_user_key(const uint256 &id) {
    return create_id_key(PropKeyUser, id);
}

static OrderedMapFileBuffer *create_track_key(const uint256 &id) {
    return create_id_key(PropKeyTrack, id);
}

static OrderedMapFileBuffer *create_command_key(const uint256 &id) {
    return create_id_key(PropKeyCommand, id);
}

static OrderedMapFileBuffer *create_basic_key(PropKey prop_key) {
    OrderedMapFileBuffer *buf = ok_mem(ordered_map_file_buffer_create(4));
    write_uint32be(buf->data, prop_key);
    return buf;
}

static int object_key_to_id(const ByteBuffer &key, uint256 *out_id) {
    if (key.length() != PROP_KEY_SIZE + PROP_KEY_SIZE + UINT256_SIZE)
        return GenesisErrorInvalidFormat;

    *out_id = uint256::read_be(key.raw() + 8);

    return 0;
}

static int list_key_to_index(const ByteBuffer &key, int *out_index) {
    if (key.length() != PROP_KEY_SIZE + PROP_KEY_SIZE + 4)
        return GenesisErrorInvalidFormat;

    uint32_t x = read_uint32be(key.raw() + 8);

    if (x > (uint32_t)INT_MAX)
        return GenesisErrorInvalidFormat;

    *out_index = x;

    return 0;
}

static int deserialize_track_decoded_key(Project *project, const uint256 &id, const ByteBuffer &value) {
    Track *track = create_zero<Track>();
    if (!track)
        return GenesisErrorNoMem;

    track->id = id;

    int err;
    int offset = 0;
    if ((err = deserialize_object(track, value, &offset))) {
        destroy(track, 1);
        return err;
    }

    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    return 0;

}

static int deserialize_track(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    uint256 track_id;
    int err = object_key_to_id(key, &track_id);
    if (err)
        return err;
    return deserialize_track_decoded_key(project, track_id, value);
}

static int deserialize_user(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    User *user = create_zero<User>();
    if (!user)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &user->id))) {
        destroy(user, 1);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(user, value, &offset))) {
        destroy(user, 1);
        return err;
    }

    project->users.put(user->id, user);
    project->user_list_dirty = true;

    return 0;
}

static void project_put_audio_asset(Project *project, AudioAsset *audio_asset) {
    project->audio_assets.put(audio_asset->id, audio_asset);
    project->audio_assets_by_digest.put(audio_asset->sha256sum, audio_asset);
    project->audio_asset_list_dirty = true;
}

static int deserialize_audio_asset(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    AudioAsset *audio_asset = create_zero<AudioAsset>();
    if (!audio_asset)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &audio_asset->id))) {
        destroy(audio_asset, 1);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(audio_asset, value, &offset))) {
        destroy(audio_asset, 1);
        return err;
    }

    project_put_audio_asset(project, audio_asset);

    return 0;
}

static void destroy_audio_clip(Project *project, AudioClip *audio_clip) {
    if (audio_clip) {
        if (audio_clip->node)
            project_remove_node_from_audio_clip(project, audio_clip);
        destroy(audio_clip, 1);
    }
}

static int deserialize_audio_clip(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    AudioClip *audio_clip = create_zero<AudioClip>();
    if (!audio_clip)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &audio_clip->id))) {
        destroy_audio_clip(project, audio_clip);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(audio_clip, value, &offset))) {
        destroy_audio_clip(project, audio_clip);
        return err;
    }

    auto audio_asset_entry = project->audio_assets.maybe_get(audio_clip->audio_asset_id);
    if (!audio_asset_entry) {
        destroy_audio_clip(project, audio_clip);
        return GenesisErrorInvalidFormat;
    }
    audio_clip->audio_asset = audio_asset_entry->value;
    audio_clip->project = project;

    project->audio_clips.put(audio_clip->id, audio_clip);
    project->audio_clip_list_dirty = true;

    project_add_node_to_audio_clip(project, audio_clip);

    return 0;
}

static int deserialize_audio_clip_segment(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    AudioClipSegment *segment = create_zero<AudioClipSegment>();
    if (!segment)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &segment->id))) {
        destroy(segment, 1);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(segment, value, &offset))) {
        destroy(segment, 1);
        return err;
    }

    auto audio_clip_entry = project->audio_clips.maybe_get(segment->audio_clip_id);
    if (!audio_clip_entry) {
        destroy(segment, 1);
        return GenesisErrorInvalidFormat;
    }
    segment->audio_clip = audio_clip_entry->value;

    auto track_entry = project->tracks.maybe_get(segment->track_id);
    if (!track_entry) {
        destroy(segment, 1);
        return GenesisErrorInvalidFormat;
    }
    segment->track = track_entry->value;

    project->audio_clip_segments.put(segment->id, segment);
    project->audio_clip_segments_dirty = true;

    return 0;
}

static int deserialize_mixer_line(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    MixerLine *mixer_line = create_zero<MixerLine>();
    if (!mixer_line)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &mixer_line->id))) {
        destroy(mixer_line, 1);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(mixer_line, value, &offset))) {
        destroy(mixer_line, 1);
        return err;
    }

    project->mixer_lines.put(mixer_line->id, mixer_line);
    project->mixer_line_list_dirty = true;

    return 0;
}

static int deserialize_effect(Project *project, const ByteBuffer &key, const ByteBuffer &value) {
    Effect *effect = create_zero<Effect>();
    if (!effect)
        return GenesisErrorNoMem;

    int err;
    if ((err = object_key_to_id(key, &effect->id))) {
        destroy(effect, 1);
        return err;
    }

    int offset = 0;
    if ((err = deserialize_object(effect, value, &offset))) {
        destroy(effect, 1);
        return err;
    }

    auto mixer_line_entry = project->mixer_lines.maybe_get(effect->mixer_line_id);
    if (!mixer_line_entry) {
        destroy(effect, 1);
        return GenesisErrorInvalidFormat;
    }
    effect->mixer_line = mixer_line_entry->value;

    project->effects.put(effect->id, effect);
    project->effects_dirty = true;

    return 0;
}

static int deserialize_command(Project *project, const ByteBuffer &key, const ByteBuffer &buffer) {
    int offset_data = 0;
    int *offset = &offset_data;
    int err;

    int fields_start_offset = *offset;

    // look for the command type field first
    int field_count;
    if ((err = deserialize_uint32be_as_int(&field_count, buffer, offset))) return err;

    int cmd_type = -1;
    for (int field_i = 0; field_i < field_count; field_i += 1) {
        int field_offset = *offset;

        int field_size;
        if ((err = deserialize_uint32be_as_int(&field_size, buffer, offset))) return err;

        int field_key;
        if ((err = deserialize_uint32be_as_int(&field_key, buffer, offset))) return err;

        if (field_key == SerializableFieldKeyCmdType) {
            if ((err = deserialize_uint32be_as_int(&cmd_type, buffer, offset))) return err;
            break;
        } else {
            *offset = field_offset + field_size;
        }
    }
    if (cmd_type == -1)
        return GenesisErrorInvalidFormat;

    *offset = fields_start_offset;

    Command *command = nullptr;
    switch ((CommandType)cmd_type) {
        case CommandTypeInvalid:
            return GenesisErrorInvalidFormat;
        case CommandTypeAddTrack:
            command = create_zero<AddTrackCommand>();
            break;
        case CommandTypeDeleteTrack:
            command = create_zero<DeleteTrackCommand>();
            break;
        case CommandTypeAddAudioClip:
            command = create_zero<AddAudioClipCommand>();
            break;
        case CommandTypeAddAudioClipSegment:
            command = create_zero<AddAudioClipSegmentCommand>();
            break;
        case CommandTypeUndo:
            command = create_zero<UndoCommand>();
            break;
        case CommandTypeRedo:
            command = create_zero<RedoCommand>();
            break;
    }
    if (!command)
        return GenesisErrorNoMem;

    command->project = project;

    if ((err = object_key_to_id(key, &command->id))) {
        destroy(command, 1);
        return err;
    }

    if ((err = deserialize_object(command, buffer, offset))) {
        destroy(command, 1);
        return err;
    }

    auto entry = project->users.maybe_get(command->user_id);
    if (!entry)
        return GenesisErrorInvalidFormat;
    command->user = entry->value;

    project->commands.put(command->id, command);
    project->command_list_dirty = true;

    return 0;
}

static int deserialize_undo_stack_item(Project *project, const ByteBuffer &key, const ByteBuffer &buffer) {
    int index;
    int err;
    if ((err = list_key_to_index(key, &index))) return err;

    int offset = 0;

    uint256 cmd_id;
    if ((err = deserialize_uint256(&cmd_id, buffer, &offset))) return err;

    auto entry = project->commands.maybe_get(cmd_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    Command *command = entry->value;

    if (project->undo_stack.resize(project->undo_stack.length() + 1))
        return GenesisErrorNoMem;

    project->undo_stack.at(project->undo_stack.length() - 1) = command;

    return 0;
}

static int read_scalar_byte_buffer(Project *project, PropKey prop_key, ByteBuffer &buffer) {
    buffer.resize(4);
    write_uint32be(buffer.raw(), prop_key);
    int key_index = ordered_map_file_find_key(project->omf, buffer);
    if (key_index == -1)
        return GenesisErrorKeyNotFound;
    return ordered_map_file_get(project->omf, key_index, nullptr, buffer);
}

static int read_scalar_string(Project *project, PropKey prop_key, String &string) {
    ByteBuffer buf;
    int err;
    if ((err = read_scalar_byte_buffer(project, prop_key, buf))) {
        return err;
    }
    bool ok;
    string = String::decode(buf, &ok);
    if (!ok)
        return GenesisErrorDecodingString;
    return 0;
}

static int read_scalar_uint256(Project *project, PropKey prop_key, uint256 *out_value) {
    ByteBuffer buf;
    int err = read_scalar_byte_buffer(project, prop_key, buf);
    if (err)
        return err;
    if (buf.length() != UINT256_SIZE)
        return GenesisErrorInvalidFormat;
    *out_value = uint256::read_be(buf.raw());
    return 0;
}

static int read_scalar_uint32be(Project *project, PropKey prop_key, uint32_t *out_value) {
    ByteBuffer buf;
    int err = read_scalar_byte_buffer(project, prop_key, buf);
    if (err)
        return err;
    if (buf.length() != 4)
        return GenesisErrorInvalidFormat;
    *out_value = read_uint32be(buf.raw());
    return 0;
}

static int read_scalar_uint32be_as_int(Project *project, PropKey prop_key, int *out_value) {
    uint32_t x;
    int err = read_scalar_uint32be(project, prop_key, &x);
    if (err)
        return err;
    if (x > (uint32_t)INT_MAX)
        return GenesisErrorInvalidFormat;
    *out_value = x;
    return 0;
}

static int iterate_prefix(Project *project, PropKey prop_key,
        int (*got_one)(Project *, const ByteBuffer &, const ByteBuffer &))
{
    ByteBuffer key_buf;
    key_buf.append_uint32be(prop_key);
    key_buf.append_uint32be(PropKeyDelimiter);
    int index = ordered_map_file_find_prefix(project->omf, key_buf);
    int key_count = ordered_map_file_count(project->omf);

    ByteBuffer *key;
    ByteBuffer value;
    while (index >= 0 && index < key_count) {
        int err = ordered_map_file_get(project->omf, index, &key, value);
        if (err)
            return err;

        if (key->cmp_prefix(key_buf) != 0)
            break;

        err = got_one(project, *key, value);
        if (err)
            return err;

        index += 1;
    }

    return 0;
}

static void project_push_command(Project *project, Command *command) {
    ok_or_panic(project->command_list.append(command));
    project->commands.put(command->id, command);
}

int project_open(const char *path, GenesisContext *genesis_context, SettingsFile *settings_file,
        User *user, Project **out_project)
{
    *out_project = nullptr;

    Project *project = create_zero<Project>();
    if (!project) {
        project_close(project);
        return GenesisErrorNoMem;
    }

    project->path = path;
    project->active_user = user;
    project->genesis_context = genesis_context;
    project->settings_file = settings_file;

    int err = ordered_map_file_open(path, &project->omf);
    if (err) {
        project_close(project);
        return err;
    }

    err = read_scalar_uint256(project, PropKeyProjectId, &project->id);
    if (err) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_uint32be_as_int(project, PropKeySampleRate, &project->sample_rate))) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_string(project, PropKeyTagTitle, project->tag_title))) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_string(project, PropKeyTagArtist, project->tag_artist))) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_string(project, PropKeyTagAlbumArtist, project->tag_album_artist))) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_string(project, PropKeyTagAlbum, project->tag_album))) {
        project_close(project);
        return err;
    }

    if ((err = read_scalar_uint32be_as_int(project, PropKeyTagYear, &project->tag_year))) {
        project_close(project);
        return err;
    }

    // read tracks
    err = iterate_prefix(project, PropKeyTrack, deserialize_track);
    if (err) {
        project_close(project);
        return err;
    }

    // read users
    err = iterate_prefix(project, PropKeyUser, deserialize_user);
    if (err) {
        project_close(project);
        return err;
    }

    // read audio assets
    err = iterate_prefix(project, PropKeyAudioAsset, deserialize_audio_asset);
    if (err) {
        project_close(project);
        return err;
    }

    // read audio clips (depends on audio assets)
    err = iterate_prefix(project, PropKeyAudioClip, deserialize_audio_clip);
    if (err) {
        project_close(project);
        return err;
    }

    // read audio clip segments (depends on audio clips)
    err = iterate_prefix(project, PropKeyAudioClipSegment, deserialize_audio_clip_segment);
    if (err) {
        project_close(project);
        return err;
    }

    // read mixer lines
    if ((err = iterate_prefix(project, PropKeyMixerLine, deserialize_mixer_line))) {
        project_close(project);
        return err;
    }

    // read effects (depends on mixer lines)
    if ((err = iterate_prefix(project, PropKeyEffect, deserialize_effect))) {
        project_close(project);
        return err;
    }

    // read command history (depends on users)
    err = iterate_prefix(project, PropKeyCommand, deserialize_command);
    if (err) {
        project_close(project);
        return err;
    }

    // read undo stack (depends on command history)
    err = iterate_prefix(project, PropKeyUndoStack, deserialize_undo_stack_item);
    if (err) {
        project_close(project);
        return err;
    }

    // read undo stack index (depends on undo stack)
    err = read_scalar_uint32be_as_int(project, PropKeyUndoStackIndex, &project->undo_stack_index);
    if (err) {
        project_close(project);
        return err;
    }
    if (project->undo_stack_index < 0 || project->undo_stack_index > project->undo_stack.length()) {
        project_close(project);
        return GenesisErrorInvalidFormat;
    }

    project_compute_indexes(project);
    ordered_map_file_done_reading(project->omf);

    project_set_up_audio_graph(project);

    *out_project = project;
    return 0;
}

static MixerLine *mixer_line_create(const char *name) {
    MixerLine *mixer_line = ok_mem(create_zero<MixerLine>());
    mixer_line->id = uint256::random();
    mixer_line->name = name;
    mixer_line->sort_key = SortKey::single(nullptr, nullptr);
    mixer_line->solo = false;
    mixer_line->volume = 1.0f;

    return mixer_line;
}

static Effect *create_default_master_send(MixerLine *mixer_line) {
    Effect *effect = ok_mem(create_zero<Effect>());
    effect->id = uint256::random();
    effect->mixer_line_id = mixer_line->id;
    effect->mixer_line = mixer_line;
    effect->effect_type = EffectTypeSend;
    effect->sort_key = SortKey::single(nullptr, nullptr);

    EffectSend *send_effect = &effect->effect.send;
    send_effect->gain = 1.0f;

    EffectSendDevice *send_device = &send_effect->send.device;
    send_device->device_id = DeviceIdMainOut;

    return effect;
}


int project_create(const char *path, GenesisContext *genesis_context, SettingsFile *settings_file,
        const uint256 &id, User *user, Project **out_project)
{
    *out_project = nullptr;
    Project *project = create_zero<Project>();
    if (!project) {
        project_close(project);
        return GenesisErrorNoMem;
    }

    int err = ordered_map_file_open(path, &project->omf);
    if (err) {
        project_close(project);
        return err;
    }
    ordered_map_file_done_reading(project->omf);

    project->id = id;
    project->path = path;
    project->active_user = user;
    project->genesis_context = genesis_context;
    project->settings_file = settings_file;

    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));

    project->users.put(user->id, user);
    project->user_list_dirty = true;
    ok_or_panic(ordered_map_file_batch_put(batch, create_user_key(user->id), omf_buf_obj(user)));

    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyProjectId), omf_buf_uint256(project->id)));
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyUndoStackIndex),
            omf_buf_uint32(project->undo_stack_index)));

    AddTrackCommand *add_track_cmd = project_insert_track_batch(project, batch, nullptr, nullptr);
    project_push_command(project, add_track_cmd);

    // Add master mixer line.
    MixerLine *mixer_line = mixer_line_create("Master");
    project->mixer_lines.put(mixer_line->id, mixer_line);
    project->mixer_line_list_dirty = true;
    ok_or_panic(ordered_map_file_batch_put(batch, create_mixer_line_key(mixer_line->id),
                omf_buf_obj(mixer_line)));
    Effect *master_send = create_default_master_send(mixer_line);
    project->effects.put(master_send->id, master_send);
    project->effects_dirty = true;
    ok_or_panic(ordered_map_file_batch_put(batch, create_effect_key(master_send->id), omf_buf_obj(master_send)));


    // Add default sample rate
    project->sample_rate = 44100;
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeySampleRate),
                omf_buf_uint32(project->sample_rate)));

    // Add default project tags
    project->tag_title = "";
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyTagTitle),
                omf_buf_string(project->tag_title)));
    project->tag_artist = user->name;
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyTagArtist),
                omf_buf_string(project->tag_artist)));
    project->tag_album_artist = user->name;
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyTagAlbumArtist),
                omf_buf_string(project->tag_album_artist)));
    project->tag_album = "";
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyTagAlbum),
                omf_buf_string(project->tag_album)));
    project->tag_year = os_get_current_year();
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyTagYear),
                omf_buf_uint32(project->tag_year)));


    err = ordered_map_file_batch_exec(batch);
    if (err) {
        project_close(project);
        return err;
    }
    project_compute_indexes(project);

    project_set_up_audio_graph(project);

    *out_project = project;
    return 0;
}

void project_close(Project *project) {
    if (project) {
        project_tear_down_audio_graph(project);

        ordered_map_file_close(project->omf);
        for (int i = 0; i < project->command_list.length(); i += 1) {
            Command *cmd = project->command_list.at(i);
            destroy(cmd, 1);
        }
        for (int i = 0; i < project->track_list.length(); i += 1) {
            Track *track = project->track_list.at(i);
            destroy(track, 1);
        }
        for (int i = 0; i < project->user_list.length(); i += 1) {
            User *user = project->user_list.at(i);
            if (user != project->active_user)
                destroy(user, 1);
        }
        destroy(project, 1);
    }
}

static void trigger_undo_changed(Project *project) {
    return trigger_event(project, EventProjectUndoChanged);
}

static void add_undo_for_command(Project *project, OrderedMapFileBatch *batch, Command *command) {
    for (int i = project->undo_stack_index; i < project->undo_stack.length(); i += 1) {
        ordered_map_file_batch_del(batch, create_undo_stack_key(i));
    }
    int this_undo_index = project->undo_stack_index;
    project->undo_stack_index += 1;
    ok_or_panic(project->undo_stack.resize(project->undo_stack_index));
    project->undo_stack.at(this_undo_index) = command;
    ok_or_panic(ordered_map_file_batch_put(batch, create_undo_stack_key(this_undo_index), omf_buf_uint256(command->id)));
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyUndoStackIndex),
            omf_buf_uint32(project->undo_stack_index)));
}

void project_perform_command(Command *command) {
    Project *project = command->project;
    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));

    add_undo_for_command(project, batch, command);

    project_perform_command_batch(project, batch, command);
    project_push_command(project, command);
    ok_or_panic(ordered_map_file_batch_put(batch, create_command_key(command->id), omf_buf_obj(command)));

    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
    trigger_undo_changed(project);
}

void project_perform_command_batch(Project *project, OrderedMapFileBatch *batch, Command *command) {
    assert(project);
    assert(batch);
    assert(command);

    int next_revision = project_get_next_revision(project);
    if (next_revision != command->revision)
        panic("unimplemented: multi-user editing merge conflict");

    command->redo(batch);
}

void project_insert_track(Project *project, const Track *before, const Track *after) {
    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));
    AddTrackCommand *add_track_cmd = project_insert_track_batch(project, batch, before, after);
    add_undo_for_command(project, batch, add_track_cmd);

    project_push_command(project, add_track_cmd);

    ok_or_panic(ordered_map_file_batch_put(batch, create_command_key(add_track_cmd->id), omf_buf_obj((Command *)add_track_cmd)));

    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
    trigger_undo_changed(project);
}

AddTrackCommand *project_insert_track_batch(Project *project, OrderedMapFileBatch *batch,
        const Track *before, const Track *after)
{
    const SortKey *low_sort_key = before ? &before->sort_key : nullptr;
    const SortKey *high_sort_key = after ? &after->sort_key : nullptr;
    SortKey sort_key = SortKey::single(low_sort_key, high_sort_key);

    AddTrackCommand *add_track = create<AddTrackCommand>(project, "Untitled Track", sort_key);
    project_perform_command_batch(project, batch, add_track);

    return add_track;
}

void project_delete_track(Project *project, Track *track) {
    DeleteTrackCommand *delete_track = create<DeleteTrackCommand>(project, track);
    project_perform_command(delete_track);
}

int project_ensure_audio_asset_loaded(Project *project, AudioAsset *audio_asset) {
    if (audio_asset->audio_file)
        return 0;

    ByteBuffer project_dir = os_path_dirname(project->path);
    ByteBuffer full_path = os_path_join(project_dir, audio_asset->path);
    return genesis_audio_file_load(project->genesis_context, full_path.raw(), &audio_asset->audio_file);
}

int project_add_audio_asset(Project *project, const ByteBuffer &full_path, AudioAsset **out_audio_asset) {
    *out_audio_asset = nullptr;

    ByteBuffer ext = os_path_extension(full_path);
    ByteBuffer project_dir = os_path_dirname(project->path);
    ByteBuffer prefix = os_path_basename(full_path);
    os_path_remove_extension(prefix);

    int err;
    ByteBuffer full_dest_asset_path;
    Sha256Hasher hasher;
    if ((err = os_copy_no_clobber(full_path.raw(), project_dir.raw(),
                    prefix.raw(), ext.raw(), full_dest_asset_path, &hasher)))
    {
        return err;
    }
    ByteBuffer digest;
    hasher.get_digest(digest);

    // see if we have an audio asset that matches this digest already
    auto entry = project->audio_assets_by_digest.maybe_get(digest);
    if (entry) {
        // oops, this file is already in the project.
        os_delete(full_dest_asset_path.raw());
        *out_audio_asset = entry->value;
        return GenesisErrorAlreadyExists;
    }

    AudioAsset *audio_asset = create_zero<AudioAsset>();
    if (!audio_asset) {
        os_delete(full_dest_asset_path.raw());
        return GenesisErrorNoMem;
    }

    audio_asset->id = uint256::random();
    audio_asset->path = os_path_basename(full_dest_asset_path);
    audio_asset->sha256sum = digest;

    project_put_audio_asset(project, audio_asset);

    // add to project file
    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));
    ok_or_panic(ordered_map_file_batch_put(batch, create_id_key(PropKeyAudioAsset, audio_asset->id), omf_buf_obj(audio_asset)));
    if ((err = ordered_map_file_batch_exec(batch))) {
        destroy(audio_asset, 1);
        os_delete(full_dest_asset_path.raw());
        return err;
    }
    project_compute_indexes(project);

    *out_audio_asset = audio_asset;
    return 0;
}

void project_add_audio_clip(Project *project, AudioAsset *audio_asset) {
    ByteBuffer name_from_path = audio_asset->path;
    os_path_remove_extension(name_from_path);
    project_perform_command(create<AddAudioClipCommand>(project, audio_asset, name_from_path));
}

void project_add_audio_clip_segment(Project *project, AudioClip *audio_clip, Track *track,
        long start, long end, double pos)
{
    project_perform_command(create<AddAudioClipSegmentCommand>(project, audio_clip, track, start, end, pos));
}

long project_audio_clip_frame_count(Project *project, AudioClip *audio_clip) {
    ok_or_panic(project_ensure_audio_asset_loaded(project, audio_clip->audio_asset));
    GenesisAudioFile *audio_file = audio_clip->audio_asset->audio_file;
    return genesis_audio_file_frame_count(audio_file);
}

int project_audio_clip_sample_rate(Project *project, AudioClip *audio_clip) {
    ok_or_panic(project_ensure_audio_asset_loaded(project, audio_clip->audio_asset));
    GenesisAudioFile *audio_file = audio_clip->audio_asset->audio_file;
    return genesis_audio_file_sample_rate(audio_file);
}

User *user_create(const uint256 &id, const String &name) {
    User *user = ok_mem(create_zero<User>());

    user->id = id;
    user->name = name;

    return user;
}

void user_destroy(User *user) {
    destroy(user, 1);
}

void project_undo(Project *project) {
    assert(project->undo_stack_index > 0);
    int this_cmd_index = project->undo_stack_index - 1;

    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));

    Command *other_command = project->undo_stack.at(this_cmd_index);
    UndoCommand *undo = create<UndoCommand>(project, other_command);
    project_perform_command_batch(project, batch, undo);

    project_push_command(project, undo);
    ok_or_panic(ordered_map_file_batch_put(batch, create_command_key(undo->id), omf_buf_obj((Command *)undo)));

    project->undo_stack_index -= 1;
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyUndoStackIndex),
            omf_buf_uint32(project->undo_stack_index)));

    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
    trigger_undo_changed(project);
}

void project_redo(Project *project) {
    assert(project->undo_stack_index < project->undo_stack.length());
    int this_cmd_index = project->undo_stack_index;

    OrderedMapFileBatch *batch = ok_mem(ordered_map_file_batch_create(project->omf));

    Command *other_command = project->undo_stack.at(this_cmd_index);
    RedoCommand *redo = create<RedoCommand>(project, other_command);
    project_perform_command_batch(project, batch, redo);

    project_push_command(project, redo);
    ok_or_panic(ordered_map_file_batch_put(batch, create_command_key(redo->id), omf_buf_obj((Command *)redo)));

    project->undo_stack_index += 1;
    ok_or_panic(ordered_map_file_batch_put(batch, create_basic_key(PropKeyUndoStackIndex),
            omf_buf_uint32(project->undo_stack_index)));

    ok_or_panic(ordered_map_file_batch_exec(batch));
    project_compute_indexes(project);
    trigger_undo_changed(project);
}

void project_get_effect_string(Project *project, Effect *effect, String &out) {
    switch ((EffectType)effect->effect_type) {
        case EffectTypeSend:
        {
            EffectSend *send = &effect->effect.send;
            switch ((EffectSendType)send->send_type) {
                case EffectSendTypeDevice:
                {
                    EffectSendDevice *send_device = &send->send.device;
                    out = ByteBuffer::format("To %s Device", device_id_str((DeviceId)send_device->device_id));
                    return;
                }
            }
            panic("invalid send type");
        }
    }
    panic("invalid effect type");
}

AddTrackCommand::AddTrackCommand(Project *project, String name, const SortKey &sort_key) :
    Command(project),
    name(name),
    sort_key(sort_key)
{
    track_id = uint256::random();
}

void AddTrackCommand::undo(OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));

    destroy(track, 1);
}

void AddTrackCommand::redo(OrderedMapFileBatch *batch) {
    Track *track = create<Track>();
    track->id = track_id;
    track->name = name;
    track->sort_key = sort_key;
    project->tracks.put(track->id, track);
    project->track_list_dirty = true;

    ok_or_panic(ordered_map_file_batch_put(batch, create_track_key(track_id), omf_buf_obj(track)));
}

void AddTrackCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int AddTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

DeleteTrackCommand::DeleteTrackCommand(Project *project, Track *track) :
    Command(project),
    track_id(track->id)
{
    serialize_object(track, payload);
}

void DeleteTrackCommand::undo(OrderedMapFileBatch *batch) {
    ok_or_panic(deserialize_track_decoded_key(project, track_id, payload));
    ok_or_panic(ordered_map_file_batch_put(batch, create_track_key(track_id), omf_buf_byte_buffer(payload)));
}

void DeleteTrackCommand::redo(OrderedMapFileBatch *batch) {
    Track *track = project->tracks.get(track_id);

    assert(track->audio_clip_segments.length() == 0);

    project->tracks.remove(track_id);
    project->track_list_dirty = true;

    ordered_map_file_batch_del(batch, create_track_key(track_id));
    destroy(track, 1);
}

void DeleteTrackCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int DeleteTrackCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

AddAudioClipCommand::AddAudioClipCommand(Project *project, AudioAsset *audio_asset, const String &name) :
    Command(project),
    audio_asset_id(audio_asset->id),
    name(name)
{
    audio_clip_id = uint256::random();
}

void AddAudioClipCommand::undo(OrderedMapFileBatch *batch) {
    AudioClip *audio_clip = project->audio_clips.get(audio_clip_id);

    project->audio_clips.remove(audio_clip_id);
    project->audio_clip_list_dirty = true;

    ordered_map_file_batch_del(batch, create_id_key(PropKeyAudioClip, audio_clip->id));

    destroy_audio_clip(project, audio_clip);
}

void AddAudioClipCommand::redo(OrderedMapFileBatch *batch) {
    AudioAsset *audio_asset = project->audio_assets.get(audio_asset_id);
    ok_or_panic(project_ensure_audio_asset_loaded(project, audio_asset));

    AudioClip *audio_clip = ok_mem(create_zero<AudioClip>());
    audio_clip->id = audio_clip_id;
    audio_clip->audio_asset_id = audio_asset->id;
    audio_clip->name = name;
    audio_clip->audio_asset = audio_asset;
    audio_clip->project = project;

    project->audio_clips.put(audio_clip->id, audio_clip);
    project->audio_clip_list_dirty = true;

    ok_or_panic(ordered_map_file_batch_put(batch,
                create_id_key(PropKeyAudioClip, audio_clip->id), omf_buf_obj(audio_clip)));

    project_add_node_to_audio_clip(project, audio_clip);
}

void AddAudioClipCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int AddAudioClipCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

AddAudioClipSegmentCommand::AddAudioClipSegmentCommand(Project *project,
        AudioClip *audio_clip, Track *track, long start, long end, double pos) :
    Command(project)
{
    this->audio_clip_segment_id = uint256::random();
    this->audio_clip_id = audio_clip->id;
    this->track_id = track->id;
    this->start = start;
    this->end = end;
    this->pos = pos;
}

void AddAudioClipSegmentCommand::undo(OrderedMapFileBatch *batch) {
    AudioClipSegment *audio_clip_segment = project->audio_clip_segments.get(audio_clip_segment_id);

    project->audio_clip_segments.remove(audio_clip_segment_id);
    project->audio_clip_segments_dirty = true;

    ordered_map_file_batch_del(batch, create_id_key(PropKeyAudioClipSegment, audio_clip_segment->id));

    destroy(audio_clip_segment, 1);
}

void AddAudioClipSegmentCommand::redo(OrderedMapFileBatch *batch) {
    AudioClipSegment *audio_clip_segment = ok_mem(create_zero<AudioClipSegment>());
    audio_clip_segment->id = audio_clip_segment_id;
    audio_clip_segment->start = start;
    audio_clip_segment->end = end;
    audio_clip_segment->pos = pos;
    audio_clip_segment->track_id = track_id;
    audio_clip_segment->track = project->tracks.get(track_id);
    audio_clip_segment->audio_clip_id = audio_clip_id;
    audio_clip_segment->audio_clip = project->audio_clips.get(audio_clip_id);

    project->audio_clip_segments.put(audio_clip_segment->id, audio_clip_segment);
    project->audio_clip_segments_dirty = true;

    ok_or_panic(ordered_map_file_batch_put(batch,
                create_id_key(PropKeyAudioClipSegment, audio_clip_segment->id), omf_buf_obj(audio_clip_segment)));
}

void AddAudioClipSegmentCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int AddAudioClipSegmentCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    return deserialize_object(this, buffer, offset);
}

UndoCommand::UndoCommand(Project *project, Command *other_command) :
    Command(project),
    other_command(other_command)
{
    other_command_id = other_command->id;
}

void UndoCommand::undo(OrderedMapFileBatch *batch) {
    other_command->redo(batch);
}

void UndoCommand::redo(OrderedMapFileBatch *batch) {
    other_command->undo(batch);
}

void UndoCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int UndoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_object(this, buffer, offset))) return err;

    auto entry = project->commands.maybe_get(other_command_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    other_command = entry->value;

    return 0;
}


RedoCommand::RedoCommand(Project *project, Command *other_command) :
    Command(project),
    other_command(other_command)
{
    other_command_id = other_command->id;
}

void RedoCommand::undo(OrderedMapFileBatch *batch) {
    other_command->undo(batch);
}

void RedoCommand::redo(OrderedMapFileBatch *batch) {
    other_command->redo(batch);
}

void RedoCommand::serialize(ByteBuffer &buf) {
    serialize_object(this, buf);
}

int RedoCommand::deserialize(const ByteBuffer &buffer, int *offset) {
    int err;
    if ((err = deserialize_object(this, buffer, offset))) return err;

    auto entry = project->commands.maybe_get(other_command_id);
    if (!entry)
        return GenesisErrorInvalidFormat;

    other_command = entry->value;

    return 0;
}
