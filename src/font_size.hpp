#ifndef FONT_SIZE_HPP
#define FONT_SIZE_HPP

#include "hash_map.hpp"
#include "freetype.hpp"

#include <stdint.h>

uint32_t hash_uint32_t(const uint32_t &);

struct FontCacheValue {
    FT_Glyph glyph;
    FT_BitmapGlyph bitmap_glyph;
    FT_UInt glyph_index;
    int above_size;
    int below_size;
};

class Gui;
class FontSize {
public:
    FontSize(Gui *gui, FT_Face font_face, int font_size);
    ~FontSize();
    FontSize &operator=(const FontSize&) = delete;
    FontSize(const FontSize&) = delete;

    FontCacheValue font_cache_entry(uint32_t codepoint);

    int _max_above_size;
    int _max_below_size;

private:
    HashMap<uint32_t, FontCacheValue, hash_uint32_t> _font_cache;

    Gui *_gui;
    FT_Face _font_face;
    int _font_size;
};

#endif
