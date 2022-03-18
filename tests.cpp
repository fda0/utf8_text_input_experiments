#define S8_Min (-0x80)
#define S8_Max ( 0x7f)
#define S16_Min (-0x8000)
#define S16_Max ( 0x7fff)
#define S32_Min (-0x80000000LL)
#define S32_Max ( 0x7fffffffLL)
#define S64_Min (-0x8000000000000000LL)
#define S64_Max ( 0x7fffffffffffffffLL)
//
#define U8_Max (0xffU)
#define U16_Max (0xffffU)
#define U32_Max (0xffffffffULL)
#define U64_Max (0xffffffffffffffffULL)

#define assert_eq_array_count(Arr1, Arr2) static_assert(array_count(Arr1) == array_count(Arr2), "Unequal array counts (2)")
#define assert_eq_array_count3(Arr1, Arr2, Arr3) static_assert(array_count(Arr1) == array_count(Arr2) && array_count(Arr1) == array_count(Arr3), "Unequal array counts (3)")
#define assert_eq_array_count4(Arr1, Arr2, Arr3, Arr4) static_assert(array_count(Arr1) == array_count(Arr2) && array_count(Arr1) == array_count(Arr3) && array_count(Arr1) == array_count(Arr4), "Unequal array counts (4)")

#define for_array(Index, Arr) for(u64 Index = 0; Index < array_count(Arr); Index += 1)


static void run_string_unit_tests()
{
    {
        String in[]  = {"yes1234"_f0, "no"_f0, "abc"_f0, "xyz"_f0, "Tulipan"_f0};
        u64 in2[]    = {3,              2,         10,         U64_Max,    0};
        String out[] = {"yes"_f0,    "no"_f0,  "abc"_f0, "xyz"_f0, ""_f0};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            String res = str_prefix(in[i], in2[i]);
            assert(str_equals(res, out[i]));
        }
    }
    {
        String in[]  = {"yes1234"_f0, "no"_f0, "abc"_f0, "xyz"_f0, "Tulipan"_f0};
        u64 in2[]    = {3,              2,         10,         U64_Max,    0};
        String out[] = {"234"_f0,     "no"_f0, "abc"_f0, "xyz"_f0, ""_f0};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            String res = str_postfix(in[i], in2[i]);
            assert(str_equals(res, out[i]));
        }
    }
    {
        String in[]  = {"yes1234"_f0, "no"_f0, "abc"_f0, "xyz"_f0, "Tulipan"_f0};
        u64 in2[]    = {3,              2,         10,         U64_Max,    0};
        String out[] = {"1234"_f0,    ""_f0,   ""_f0,    ""_f0,    "Tulipan"_f0};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            String res = str_skip(in[i], in2[i]);
            assert(str_equals(res, out[i]));
        }
    }
    {
        String in[]  = {"yes1234"_f0, "no"_f0, "abc"_f0, "xyz"_f0, "Tulipan"_f0};
        u64 in2[]    = {3,              2,         10,         U64_Max,    0};
        String out[] = {"yes1"_f0,    ""_f0,   ""_f0,    ""_f0,    "Tulipan"_f0};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            String res = str_chop(in[i], in2[i]);
            assert(str_equals(res, out[i]));
        }
    }
    {
        String in[]  = {"yes1234"_f0, "no"_f0, "abc"_f0, "xyz"_f0, "Tulipan"_f0};
        u64 in2[]    = {3,              2,         1,          2,          1000};
        u64 in3[]    = {2,              2,         0,          U64_Max,    1};
        String out[] = {"12"_f0,      ""_f0,   ""_f0,    "z"_f0,   ""_f0};
        assert_eq_array_count4(in, in2, in3, out);
        
        for_array(i, in) {
            String res = str_substr(in[i], in2[i], in3[i]);
            assert(str_equals(res, out[i]));
        }
    }
    {
        String in[]  = {"Łabędzie"_f0, "Abc2"_f0, "ŁabędZIE"_f0};
        String in2[] = {"Łabędzie"_f0, "Abc"_f0,  "łabędzie"_f0};
        b32 out[]    = {true,          false,     false};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            b32 res = str_equals(in[i], in2[i]);
            assert(res == out[i]);
        }
    }
    {
        String in[]  = {"Łabędzie"_f0, "Abc2"_f0, "ŁabędZIE"_f0};
        String in2[] = {"Łabędzie"_f0, "Abc"_f0,  "łabędzie"_f0};
        b32 out[]    = {true,          false,     true};
        assert_eq_array_count3(in, in2, out);
        
        for_array(i, in) {
            b32 res = str_equals_ignore_case(in[i], in2[i]);
            assert(res == out[i]);
        }
    }
}










static void run_utf8_unit_tests()
{
    struct Debug_Test_Utf8_Reference
    {
        u32 codepoint;
        u8 utf8[4];
        u32 utf8_count;
    };
    
    Debug_Test_Utf8_Reference utf8_refs[] =
    {
        {'A', {'A'}, 1}, // [0] A, latin captial A
        {0x142, {0xC5, 0x82}, 2}, // [1] ł, latin l with stroke
        {0x7FF, {0xDF, 0xBF}, 2}, // [2] last 2 byte utf8, NKO TAMAN SIGN
        
        {0x800, {0xE0, 0xA0, 0x80}, 3}, // [3] first 3 byte utf8, Samaritan letter alaf
        {0x2776, {0xE2, 0x9D, 0xB6}, 3}, // [4] negative (black) circled digit one
        {0xFB13, {0xEF, 0xAC, 0x93}, 3}, // [5] Armenian small ligature men now
        {0xFFFD, {0xEF, 0xBF, 0xBD}, 3}, // [6] Replacement character
        {0xFFFE, {0xEF, 0xBF, 0xBE}, 3}, // [7*] penultimate 3 byte utf8, shouldn't happen
        {0xFFFF, {0xEF, 0xBF, 0xBF}, 3}, // [8*] last 3 byte utf8, shouldn't happen
        
        {0x10000, {0xF0, 0x90, 0x80, 0x80}, 4}, // [9] First 4 byte utf8, Linear B syllabe B0008 A
        {0x27123, {0xF0, 0xA7, 0x84, 0xA3}, 4}, // [10] Cjk ideograph extension B
        {0x1F980, {0xF0, 0x9F, 0xA6, 0x80}, 4}, // [11] crab emoji
        {0x10FFFD, {0xF4, 0x8F, 0xBF, 0xBD}, 4}, // [12] Last codepoint, Plane 16 private use
        {0x10FFFE, {0xF4, 0x8F, 0xBF, 0xBE}, 4}, // [13*] Last codepoint + 1, shouldn't happen
        {0x10FFFF, {0xF4, 0x8F, 0xBF, 0xBF}, 4}, // [14*] Last codepoint + 2, shouldn't happen
    };
    
    for_array(ref_index, utf8_refs)
    {
        Debug_Test_Utf8_Reference ref = utf8_refs[ref_index];
        
        u8 buffer[16]; // only 4 used
        u32 count = utf8_write(buffer, ref.codepoint);
        
        assert(count == ref.utf8_count);
        for (u32 i = 0; i < count; i += 1)
        {
            assert(buffer[i] == ref.utf8[i]);
        }
        
        
        // reverse
        Unicode_Consume consume = utf8_consume(buffer, count);
        assert(consume.inc == count);
        assert(consume.codepoint == ref.codepoint);
        
        Unicode_Consume consume2 = utf8_consume(buffer, 16);
        assert(consume2.inc == count);
        assert(consume2.codepoint == ref.codepoint);
    }
    
    
    
    //~ Reverse consume
    String consume_strings[] = {
        "yes"_f0,
        "Tosty"_f0,
        "Łabędzie"_f0,
        "漢字"_f0,
    };
    for_array(consume_string_index, consume_strings)
    {
        String text = consume_strings[consume_string_index];
        Unicode_Consume forward[512];
        s64 forward_count = 0;
        
        {
            String t = text;
            while (t.size)
            {
                assert(array_count(forward) > forward_count);
                forward[forward_count] = utf8_consume(t);
                t = str_skip(t, forward[forward_count].inc);
                forward_count += 1;
            }
        }
        
        {
            String t = text;
            s64 rev_count = 0;
            
            while (t.size)
            {
                Unicode_Consume_Reverse rev = utf8_consume_reverse(t);
                t = str_chop(t, rev.dec);
                
                s64 f_index = forward_count - rev_count - 1;
                assert(f_index >= 0 && f_index < forward_count);
                
                Unicode_Consume f = forward[f_index];
                assert(f.inc == rev.dec);
                assert(f.codepoint == rev.codepoint);
                
                rev_count += 1;
                assert(rev_count <= forward_count);
            }
        }
    }
}











static void run_text_input_unit_tests()
{
    {
        String in_text[] = {
            "--Ax-B C"_f0, "Ab   Cd   Ef"_f0, "underflow"_f0, "overflow"_f0, "move overflow"_f0,
            " a b c "_f0, "abc\t d+ef- \n-ghi"_f0,
        };
        s64 in_pos[] = {
            0, 2, -100, 100, 1,
            5, 11,
        };
        s64 in_move_by_words[] = {
            2, 1, 1, 1, 100,
            -2, -1,
        };
        s64 out[] = {
            7, 10, 0, 8, 13,
            1, 7,
        };
        assert_eq_array_count4(in_text, in_pos, in_move_by_words, out);
        
        for_array(i, in_text)
        {
            s64 res = str_move_pos_by_words(in_text[i], in_pos[i], in_move_by_words[i]);
            assert(res == out[i]);
        }
    }
    
    
    
    
    
    
    
    
    {
        String in[] = {
            "Swan"_f0, "Łabędź"_f0, "Swan 🦢"_f0, "A"_f0, "Ź"_f0, ""_f0,
            "Łabęd\xC5"_f0, "Swan \xf0\x9f\xa6"_f0, "\xf0\x9f"_f0, "\xf0"_f0,
            "ﬓ"_f0, "\xef\xac"_f0, "ń\xef"_f0,
            
        };
        String out[] = {
            "Swan"_f0, "Łabędź"_f0, "Swan \xf0\x9f\xa6\xa2"_f0, "A"_f0, "Ź"_f0, ""_f0,
            "Łabęd"_f0, "Swan "_f0, ""_f0, ""_f0,
            "ﬓ"_f0, ""_f0, "ń"_f0,
        };
        assert_eq_array_count(in, out);
        
        for_array(i, in) {
            String res = truncate_invalid_utf8_ending(in[i]);
            assert(str_equals(res, out[i]));
        }
    }
    
    
    
    {
        u8 memory[32];
        Mutable_String buffer = {};
        buffer.str = memory;
        buffer.cap = array_count(memory);
        
        text_replace_range(&buffer, 0, 0, "abcdef"_f0);
        assert(str_equals(buffer, "abcdef"_f0));
        
        text_replace_range(&buffer, 1, 2, ""_f0);
        assert(str_equals(buffer, "acdef"_f0));
        
        text_replace_range(&buffer, 3, 1, ""_f0);
        assert(str_equals(buffer, "aef"_f0));
        
        text_replace_range(&buffer, 1, 100, "Łabędź"_f0);
        assert(str_equals(buffer, "aŁabędź"_f0));
        
        text_replace_range(&buffer, -10, 100, "Swan 🦢"_f0);
        assert(str_equals(buffer, "Swan 🦢"_f0));
        
        text_replace_range(&buffer, 4, 4, " emoji is:"_f0);
        assert(str_equals(buffer, "Swan emoji is: 🦢"_f0)); // 19 bytes
        
        text_replace_range(&buffer, 100, 100, ", padding."_f0); // add 10 bytes
        assert(str_equals(buffer, "Swan emoji is: 🦢, padding."_f0)); // 29 bytes
        
        text_replace_range(&buffer, 0, 0, "!🦢"_f0); // try to add 5 bytes
        assert(str_equals(buffer, "!Swan emoji is: 🦢, padding."_f0)); // 30 bytes - emoji should be truncated
        
        text_replace_range(&buffer, 0, 2, "🦢"_f0); // remove 2 bytes, add 4 bytes
        assert(str_equals(buffer, "🦢wan emoji is: 🦢, padding."_f0)); // 32 bytes
        
        text_replace_range(&buffer, 0, 32, ""_f0);
        assert(str_equals(buffer, ""_f0));
    }
    
    
    
    {
        u8 memory[8];
        Text_Input text = {};
        text.buffer.str = memory;
        text.buffer.cap = array_count(memory);
        
        text_input_write(&text, "12345678abcdef"_f0);
        assert(text.cursor_pos == text.mark_pos);
        assert(text.cursor_pos == 8);
        assert(str_equals(text.buffer, "12345678"_f0));
        
        text_input_move_cursor(&text, -1, TextInputMove_Select);
        text_input_write(&text, "łx"_f0);
        assert(text.mark_pos == 8);
        assert(text.cursor_pos == 7);
        assert(str_equals(text.buffer, "12345678"_f0));
        
        text_input_move_cursor(&text, -1, TextInputMove_Select);
        text_input_write(&text, "łx"_f0);
        assert(text.cursor_pos == text.mark_pos);
        assert(text.cursor_pos == 8);
        assert(str_equals(text.buffer, "123456ł"_f0));
    }
}

