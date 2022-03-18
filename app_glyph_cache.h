enum Glyph_Status
{
    Glyph_Empty,
    Glyph_Invalid,
    Glyph_OnlyMetrics,
    Glyph_Loaded,
};

struct Glyph
{
    // coordinates in texture;
    u16 tex_x, tex_y;
    u16 width, height;
    s16 offset_x, offset_y;
    f32 advance;
    Glyph_Status status;
    
    // The hash is equal to the codepoint value of the glyph.
    // This means that we do not support glyphs that are produced out of multiple connected codepoints.
    // Unicode in theory supports glyphs that can be made out of billions of codepoints
    //   for Arabic and other languages where the codepoints can connect to create one wide glyph.
    // Another example would be color skin modifiers for emojis.
    u32 hash; // only bottom 21 bits are used
    
    u16 next_in_collision;
    u16 next_in_lru;
    u16 prev_in_lru;
};

#define Font_AsciiBlockStart (' ')
#define FontAsciiBlockLast ('~')
#define Font_AsciiBlockCount (FontAsciiBlockLast - '!' + 1)

struct Font
{
    unsigned char *font_file_data;
    stbtt_fontinfo stb_info;
    f32 font_scale;
    f32 ascent, descent, line_gap;
    f32 space_width;
    
    u16 max_width, max_height;
    u16 tex_x, tex_y;
    
    Glyph ascii_block[Font_AsciiBlockCount];
    
    u32 data_table_count;
    Glyph *data_table;
    
    u32 hash_table_count;
    u32 hash_table_key_mask;
    u16 *hash_table;
};







static void clear_glyph_font_data(Glyph *glyph)
{
    glyph->width = glyph->height = 0;
    glyph->offset_x = glyph->offset_y = 0;
    glyph->advance = 0;
    glyph->status = {};
    glyph->hash = 0;
}

static void initialize_glyph(Glyph *glyph, Font *font, u32 codepoint,
                             b32 increment_texture_position = false)
{
    clear_glyph_font_data(glyph);
    glyph->hash = codepoint;
    
    // @todo Use this: stbtt_MakeCodepointBitmap() -- renders into bitmap provided by the user
    // Currently it uses malloc inside stb_truetype.h
    stbtt_fontinfo *info = &font->stb_info;
    
    int glyph_index = stbtt_FindGlyphIndex(info, codepoint);
    if (glyph_index > 0)
    {
        s32 width, height, offset_x, offset_y;
        u8 *mono_bitmap = stbtt_GetGlyphBitmapSubpixel(info, 0, font->font_scale,
                                                       0.f, 0.f, // subpixel position
                                                       glyph_index,
                                                       &width, &height, &offset_x, &offset_y);
        
        glyph->offset_x = (s16)offset_x;
        glyph->offset_y = (s16)offset_y;
        glyph->width = (u16)width;
        glyph->height = (u16)height;
        
        s32 advance, left_side_bearing;
        stbtt_GetCodepointHMetrics(info, codepoint, &advance, &left_side_bearing);
        glyph->advance = (f32)advance * font->font_scale;
        
        
        if (mono_bitmap &&
            (width > 0 && width < font->max_width &&
             height > 0 && height < font->max_height))
        {
            glyph->status = Glyph_Loaded;
            
            if (increment_texture_position)
            {
                glyph->tex_x = font->tex_x;
                glyph->tex_y = font->tex_y;
                font->tex_x += (u16)width + 1;
            }
            
            d3d11_update_glyph_texture(glyph->tex_x, glyph->tex_y, width, height, mono_bitmap);
        }
        else if (glyph->advance != 0)
        {
            glyph->status = Glyph_OnlyMetrics;
        }
        else
        {
            glyph->status = Glyph_Invalid;
        }
        
        
        if (mono_bitmap)
        {
            stbtt_FreeBitmap(mono_bitmap, 0);
        }
    }
    else
    {
        glyph->status = Glyph_Invalid;
    }
}



static void initialize_font(Font *font, char *font_file_data, f32 pixel_scale)
{
    font->font_file_data = (unsigned char *)font_file_data;
    
    assert(pixel_scale >= 1.f);
    pixel_scale = get_max(pixel_scale, 1.f);
    
    
    int init_offset = stbtt_GetFontOffsetForIndex(font->font_file_data, 0);
    b32 ok = (init_offset >= 0);
    
    if (ok)
    {
        stbtt_fontinfo *info = &font->stb_info;
        ok = (0 != stbtt_InitFont(info, font->font_file_data, init_offset));
        
        if (ok)
        {
            font->font_scale = stbtt_ScaleForPixelHeight(info, pixel_scale);
            
            {
                s32 x0, y0, x1, y1;
                stbtt_GetFontBoundingBox(info, &x0, &y0, &x1, &y1);
                f32 bounding_width = (x1 - x0) * font->font_scale;
                f32 bounding_height = (y1 - y0) * font->font_scale;
                assert(bounding_width > 0.f && bounding_height > 0);
                
                font->max_width = (u16)get_max(1.f, ceilf(bounding_width) + 1);
                font->max_height = (u16)get_max(1.f, ceilf(bounding_height) + 1);
            }
            
            {
                s32 ascent, descent, line_gap;
                stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);
                font->ascent   = (f32)ascent   * font->font_scale;
                font->descent  = (f32)descent  * font->font_scale;
                font->line_gap = (f32)line_gap * font->font_scale;
            }
            
            {
                s32 advance_width, left_side_bearing;
                stbtt_GetCodepointHMetrics(info, ' ', &advance_width, &left_side_bearing);
                font->space_width = (f32)advance_width * font->font_scale;
            }
            
            for (s32 ascii_index = 0;
                 ascii_index < array_count(font->ascii_block);
                 ascii_index += 1)
            {
                initialize_glyph(font->ascii_block + ascii_index, font,
                                 Font_AsciiBlockStart + ascii_index, true);
            }
        }
    }
    
    
    
    if (ok)
    {
        s32 texture_width_left = (TEXTURE_WIDTH - font->tex_x);
        s32 dynamic_glyph_slots = texture_width_left / font->max_width;
        
        if (dynamic_glyph_slots > 0)
        {
            font->data_table_count = dynamic_glyph_slots + 1; // + 1 for "null slot" with index 0
            
            Bit_Scan_Result msb = find_most_significant_bit(dynamic_glyph_slots * 4 / 3);
            font->hash_table_count = (1 << (msb.index + 1));
            font->hash_table_key_mask = font->hash_table_count - 1;
            
            
            u64 total_size = (sizeof(Glyph)*font->data_table_count +
                              sizeof(u16)*font->hash_table_count);
            
            font->data_table = (Glyph *)allocate_memory(total_size);
            font->hash_table = (u16 *)(font->data_table + font->data_table_count);
            
            
            // Initialize Least Recently Used cache
            for (s64 data_index = 0; data_index < font->data_table_count; data_index += 1)
            {
                Glyph *glyph = font->data_table + data_index;
                glyph->next_in_lru = (u16)(data_index + 1);
                glyph->prev_in_lru = (u16)(data_index - 1);
                
                if (data_index > 0)
                {
                    glyph->tex_x = font->tex_x;
                    glyph->tex_y = font->tex_y;
                    font->tex_x += font->max_width;
                    assert(font->tex_x <= TEXTURE_WIDTH);
                }
            }
            
            u64 last_index = (font->data_table_count - 1);
            font->data_table[0].prev_in_lru = (u16)last_index;
            font->data_table[last_index].next_in_lru = 0;
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        memset(font, 0, sizeof(*font));
    }
}



static Glyph *get_free_glyph_slot(Font *font)
{
    u16 oldest = font->data_table[0].prev_in_lru;
    Glyph *glyph_oldest = font->data_table + oldest;
    
    if (glyph_oldest->hash)
    {
        // used glyph - needs to be evicted
        u32 key = 1 + (glyph_oldest->hash & font->hash_table_key_mask);
        u16 *scan_for_hash_table_index = font->hash_table + key;
        
        for (;;)
        {
            Glyph *glyph_check = font->data_table + *scan_for_hash_table_index;
            
            if (glyph_check == glyph_oldest)
            {
                *scan_for_hash_table_index = glyph_check->next_in_collision;
                glyph_check->next_in_collision = 0;
                break;
            }
            
            assert(glyph_check->next_in_collision);
            scan_for_hash_table_index = &glyph_check->next_in_collision;
        }
        
        assert(!glyph_oldest->next_in_collision);
        clear_glyph_font_data(glyph_oldest);
    }
    
    return glyph_oldest;
}

static u16 get_data_index_from_glyph_pointer(Font *font, Glyph *glyph)
{
    assert((u64)glyph >= (u64)font->data_table);
    u64 diff = (u64)glyph - (u64)font->data_table;
    diff /= sizeof(Glyph);
    u16 result = (u16)diff;
    return result;
}

static Glyph get_glyph(Font *font, u32 codepoint)
{
    if (codepoint >= Font_AsciiBlockStart &&
        codepoint < FontAsciiBlockLast)
    {
        u32 glyph_index = codepoint - Font_AsciiBlockStart;
        return font->ascii_block[glyph_index];
    }
    
    u32 hash = codepoint;
    u32 key = 1 + (hash & font->hash_table_key_mask);
    
    Glyph *glyph = font->data_table + font->hash_table[key];
    for (;;)
    {
        if (glyph->hash == hash) {
            break;
        }
        if (!glyph->next_in_collision) {
            glyph = nullptr;
            break;
        }
        
        glyph = font->data_table + glyph->next_in_collision;
    }
    
    if (!glyph)
    {
        glyph = get_free_glyph_slot(font);
        initialize_glyph(glyph, font, codepoint);
        
        glyph->next_in_collision = font->hash_table[key];
        font->hash_table[key] = get_data_index_from_glyph_pointer(font, glyph);
    }
    
    
    // update position in RLU
    {
        Glyph *next = font->data_table + glyph->next_in_lru;
        Glyph *prev = font->data_table + glyph->prev_in_lru;
        prev->next_in_lru = glyph->next_in_lru;
        next->prev_in_lru = glyph->prev_in_lru;
        
        glyph->next_in_lru = font->data_table[0].next_in_lru;
        glyph->prev_in_lru = 0;
        
        u16 this_glyph_index = get_data_index_from_glyph_pointer(font, glyph);
        font->data_table[glyph->next_in_lru].prev_in_lru = this_glyph_index;
        font->data_table[glyph->prev_in_lru].next_in_lru = this_glyph_index;
    }
    
    return *glyph;
}

