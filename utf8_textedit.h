
struct Mutable_String
{
    u8 *str;
    u64 size;
    u64 cap;
    
    operator String() {
        return {str, size};
    }
};


struct Text_Input
{
    Mutable_String buffer;
    s64 cursor_pos, mark_pos;
    
    f32 target_scroll_x;
    f32 scroll_anim_start, scroll_anim_t; // for animation
    f32 current_scroll_x;
    
    // @todo store how much bytes one should skip for draw only calls when text input isn't dirty
    
    b32 is_dirty; // @todo store last width of text input so it automatically becoes dirty if width changes
    // could be set to -1.f to force dirty state
};







static String truncate_invalid_utf8_ending(String input)
{
    // This function assumes that our input was a valid utf8 string at some point
    //   but it got truncated without much care - possibly in the middle of a codepoint.
    // It removes up to 3 bytes if string's ending is an incorrect utf8 sequence.
    
    String result = input;
    
    s32 follower_byte_count = 0; // counter for bytes with 10xxxxxx bits
    s64 end_at = get_max(0, input.size - 4);
    
    for (s64 index = input.size-1; index >= end_at; index -= 1)
    {
        u8 byte = input.str[index];
        u8 byte_class = utf8_top5b_class[byte >> 3];
        
        if (byte_class == 0)
        {
            follower_byte_count += 1;
        }
        else
        {
            if (byte_class != follower_byte_count + 1)
            {
                result.size = index;
            }
            
            break;
        }
    }
    
    return result;
}






struct Text_Replace_Range_Result
{
    b32 did_anything;
    s64 cursor_pos;
};

static Text_Replace_Range_Result text_replace_range(Mutable_String *buffer,
                                                    s64 selection_start, s64 one_past_selection_end,
                                                    String insert_text)
{
    selection_start = get_min(get_max(0, selection_start), (s64)buffer->size);
    one_past_selection_end = get_min(get_max(0, one_past_selection_end), (s64)buffer->size);
    
    if (selection_start > one_past_selection_end)
    {
        s64 temp = selection_start;
        selection_start = one_past_selection_end;
        one_past_selection_end = temp;
    }
    
    Text_Replace_Range_Result result = {};
    
    s64 to_delete = one_past_selection_end - selection_start;
    s64 initial_buffer_size_delta = insert_text.size - to_delete;
    
    { // truncate insert_text to fit in the buffer;
        s64 max_positive_buffer_delta = buffer->cap - buffer->size;
        
        if (max_positive_buffer_delta < initial_buffer_size_delta)
        {
            s64 reduce_size = initial_buffer_size_delta - max_positive_buffer_delta;
            assert((s64)insert_text.size >= reduce_size);
            
            insert_text.size -= reduce_size;
            insert_text = truncate_invalid_utf8_ending(insert_text);
        }
    }
    
    s64 buffer_size_delta = insert_text.size - to_delete;
    
    if ((!buffer_size_delta && !to_delete) ||
        (initial_buffer_size_delta > 0 && buffer_size_delta < 0))
    {
        return result;
    }
    
    
    if (buffer_size_delta > 0)
    {
        for (s64 index = buffer->size - 1;
             index >= one_past_selection_end;
             index -= 1)
        {
            buffer->str[index + buffer_size_delta] = buffer->str[index];
        }
    }
    else if (buffer_size_delta < 0)
    {
        for (s64 index = one_past_selection_end;
             index < (s64)buffer->size;
             index += 1)
        {
            buffer->str[index + buffer_size_delta] = buffer->str[index];
        }
    }
    
    
    for (u64 index = 0; index < insert_text.size; index += 1)
    {
        buffer->str[selection_start + index] = insert_text.str[index];
    }
    
    buffer->size += buffer_size_delta;
    
    result.did_anything = true;
    result.cursor_pos = selection_start + insert_text.size;
    return result;
}



static void text_input_write(Text_Input *text_input, String new_text)
{
    Text_Replace_Range_Result res = text_replace_range(&text_input->buffer, text_input->cursor_pos,
                                                       text_input->mark_pos, new_text);
    
    if (res.did_anything)
    {
        text_input->cursor_pos = text_input->mark_pos = res.cursor_pos;
        text_input->is_dirty = true;
    }
}

static b32 text_input_has_selection(Text_Input *text_input)
{
    return (text_input->cursor_pos != text_input->mark_pos);
}










static s64 str_move_pos_by_codepoints(String text, s64 pos, s64 move_by_codepoint_count)
{
    if (pos > (s64)text.size) {
        pos = text.size;
    } else if (pos < 0) {
        pos = 0;
    }
    else
    {
        s64 change = 0;
        
        if (move_by_codepoint_count > 0)
        {
            text = str_skip(text, pos);
            while (move_by_codepoint_count && text.size)
            {
                Unicode_Consume consume = utf8_consume(text);
                text = str_skip(text, consume.inc);
                change += consume.inc;
                move_by_codepoint_count -= 1;
            }
        }
        else if (move_by_codepoint_count < 0)
        {
            text = str_prefix(text, pos);
            while (move_by_codepoint_count && text.size)
            {
                Unicode_Consume_Reverse consume = utf8_consume_reverse(text);
                text = str_chop(text, consume.dec);
                change -= consume.dec;
                move_by_codepoint_count += 1;
            }
        }
        
        pos += change;
    }
    
    return pos;
}



static b32 is_word_separator(u32 c)
{
    return (is_whitespace(c) ||
            (c >= '!' && c <= '/') ||
            (c >= ':' && c <= '@') ||
            (c >= '[' && c <= '^') ||
            (c >= '{' && c <= '~'));
}


static s64 str_move_pos_by_words(String text, s64 pos, s64 move_by_word_count)
{
    if (pos > (s64)text.size) {
        pos = text.size;
    } else if (pos < 0) {
        pos = 0;
    }
    else
    {
        s64 change = 0;
        b32 skipping_over_separators = true;
        
        if (move_by_word_count > 0)
        {
            text = str_skip(text, pos);
            while (text.size)
            {
                Unicode_Consume consume = utf8_consume(text);
                text = str_skip(text, consume.inc);
                
                if (is_word_separator(consume.codepoint))
                {
                    if (!skipping_over_separators)
                    {
                        move_by_word_count -= 1;
                        skipping_over_separators = true;
                    }
                }
                else
                {
                    if (!move_by_word_count) { 
                        break;
                    }
                    skipping_over_separators = false;
                }
                
                
                change += consume.inc;
            }
        }
        else if (move_by_word_count < 0)
        {
            text = str_prefix(text, pos);
            while (text.size)
            {
                Unicode_Consume_Reverse consume = utf8_consume_reverse(text);
                text = str_chop(text, consume.dec);
                
                if (is_word_separator(consume.codepoint))
                {
                    if (!skipping_over_separators)
                    {
                        move_by_word_count += 1;
                        if (!move_by_word_count) { 
                            break;
                        }
                        skipping_over_separators = true;
                    }
                }
                else
                {
                    skipping_over_separators = false;
                }
                
                change -= consume.dec;
            }
        }
        
        pos += change;
    }
    
    return pos;
}





enum Text_Input_Move_Flags
{
    TextInputMove_Select = (1 << 0),
    TextInputMove_ByWords = (1 << 1),
    TextInputMove_ByMax = (1 << 2),
};

static void text_input_move_cursor(Text_Input *text_input, s64 move_by, u32 flags)
{
    text_input->is_dirty = true;
    
    if (!flags && text_input_has_selection(text_input))
    {
        if (move_by > 0) {
            s64 max_pos = get_max(text_input->cursor_pos, text_input->mark_pos);
            text_input->cursor_pos = text_input->mark_pos = max_pos;
        } else {
            s64 min_pos = get_min(text_input->cursor_pos, text_input->mark_pos);
            text_input->cursor_pos = text_input->mark_pos = min_pos;
        }
        
        return;
    }
    
    
    if (flags & TextInputMove_ByMax) {
        text_input->cursor_pos = (move_by >= 0 ? text_input->buffer.size : 0);
    } else if (flags & TextInputMove_ByWords) {
        text_input->cursor_pos = str_move_pos_by_words(text_input->buffer, text_input->cursor_pos, move_by);
    } else {
        text_input->cursor_pos = str_move_pos_by_codepoints(text_input->buffer, text_input->cursor_pos, move_by);
    }
    
    
    if (!(flags & TextInputMove_Select))
    {
        text_input->mark_pos = text_input->cursor_pos;
    }
}


