#include "font_size.hpp"

static void ft_ok(FT_Error err) {
    if (err)
        panic("freetype error");
}

FontSize::FontSize(Gui *gui, FT_Face font_face, int font_size) :
    _max_above_size(0),
    _max_below_size(0),
    _gui(gui),
    _font_face(font_face),
    _font_size(font_size)
{
    // pre-fill some characters in the cache so that we have a good measurement of
    // _max_above_size and _max_below_size
    static const char * some_characters =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%^&*()[{]}?/|.<>?";
    for (const char *ptr = some_characters; *ptr; ptr += 1)
        font_cache_entry((uint32_t)*ptr);
}

FontSize::~FontSize() {
    HashMap<uint32_t, FontCacheValue, hash_uint32_t>::Iterator it = _font_cache.value_iterator();
    while (it.has_next()) {
        FontCacheValue entry = it.next();
        FT_Done_Glyph(entry.glyph);
    }
}

uint32_t hash_uint32_t(const uint32_t &x) {
    return x;
}

FontCacheValue FontSize::font_cache_entry(uint32_t codepoint) {
    FontCacheValue value;
    if (_font_cache.get(codepoint, &value))
        return value;

    ft_ok(FT_Set_Char_Size(_font_face, 0, _font_size * 64, 0, 0));
    FT_UInt glyph_index = FT_Get_Char_Index(_font_face, codepoint);
    ft_ok(FT_Load_Glyph(_font_face, glyph_index, FT_LOAD_RENDER));
    FT_GlyphSlot glyph_slot = _font_face->glyph;
    FT_Glyph glyph;
    ft_ok(FT_Get_Glyph(glyph_slot, &glyph));
    ft_ok(FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 0));
    FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph) glyph;

    int bmp_start_top = bitmap_glyph->top;
    int bmp_height = bitmap_glyph->bitmap.rows;
    int this_above_size = bmp_start_top;
    int this_below_size = bmp_height - this_above_size;

    if (this_above_size > _max_above_size)
        _max_above_size = this_above_size;
    if (this_below_size > _max_below_size)
        _max_below_size = this_below_size;

    value = FontCacheValue{glyph, bitmap_glyph, glyph_index, this_above_size, this_below_size};
    _font_cache.put(codepoint, value);
    return value;
}
