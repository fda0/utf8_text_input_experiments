
struct String
{
    u8 *str;
    u64 size;
};

static String operator "" _f0(const char *str, size_t size)
{
    return {(u8 *)str, (u64)size};
}

static String cstr_to_string(char *cstr)
{
    return {(u8 *)cstr, strlen(cstr)};
}




static String str_prefix(String text, u64 size)
{
    size = get_min(size, text.size);
    text.size = size;
    return text;
}

static String str_postfix(String text, u64 size)
{
    size = get_min(text.size, size);
    u64 distance = text.size - size;
    
    text.str = text.str + distance;
    text.size = size;
    return text;
};

static String str_skip(String text, u64 distance)
{
    distance = get_min(distance, text.size);
    String result = {};
    result.str = text.str + distance;
    result.size = text.size - distance;
    return result;
}

static String str_chop(String text, u64 distance_from_end)
{
    if (distance_from_end > text.size)
    {
        text.size = 0;
    }
    else
    {
        text.size = text.size - distance_from_end;
    }
    return text;
}

static String str_substr(String text, u64 distance, u64 length)
{
    String result = str_skip(text, distance);
    result.size = get_min(result.size, length);
    return result;
}





static b32 is_whitespace(u32 c)
{
    // does the same as isspace in clib
    // checks for tab, carriage return, newline, vertical tab, or form feed
    return (c == ' ' || (c >= 0x8 && c <= 0xD));
}








#define Bitmask_1 0x00000001
#define Bitmask_2 0x00000003
#define Bitmask_3 0x00000007
#define Bitmask_4 0x0000000f
#define Bitmask_5 0x0000001f
#define Bitmask_6 0x0000003f
#define Bitmask_7 0x0000007f
#define Bitmask_8 0x000000ff
#define Bitmask_9 0x000001ff
#define Bitmask_10 0x000003ff
#define Bitmask_11 0x000007ff
#define Bitmask_12 0x00000fff
#define Bitmask_13 0x00001fff
#define Bitmask_14 0x00003fff
#define Bitmask_15 0x00007fff
#define Bitmask_16 0x0000ffff

#define Bitmask_17 0x0001ffff
#define Bitmask_18 0x0003ffff
#define Bitmask_19 0x0007ffff
#define Bitmask_20 0x000fffff
#define Bitmask_21 0x001fffff
#define Bitmask_22 0x003fffff
#define Bitmask_23 0x007fffff
#define Bitmask_24 0x00ffffff
#define Bitmask_25 0x01ffffff
#define Bitmask_26 0x03ffffff
#define Bitmask_27 0x07ffffff
#define Bitmask_28 0x0fffffff
#define Bitmask_29 0x1fffffff
#define Bitmask_30 0x3fffffff
#define Bitmask_31 0x7fffffff
static u8 utf8_top5b_class[32] = {
    // Maps top 5 bits of the (hopefully) first utf8 byte in
    //  codepoint sequence to the length of the utf8 codepoint.
    // Values 0 & 5 are invalid.
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
};


struct Unicode_Consume
{
    u32 inc;
    u32 codepoint;
};
struct Unicode_Consume_Reverse
{
    u32 dec;
    u32 codepoint;
};


static Unicode_Consume utf8_consume(u8 *str, u64 max)
{
    Unicode_Consume result = {1, ~0u};
    
    if (max > 0)
    {
        u8 byte = str[0];
        u8 byte_class = utf8_top5b_class[byte >> 3];
        
        switch (byte_class)
        {
            case 1:
            {
                result.codepoint = byte;
            } break;
            case 2:
            {
                if (1 < max)
                {
                    u8 cont_byte = str[1];
                    if (utf8_top5b_class[cont_byte >> 3] == 0)
                    {
                        result.codepoint = (byte & Bitmask_5) << 6;
                        result.codepoint |= (cont_byte & Bitmask_6);
                        result.inc = 2;
                    }
                }
            } break;
            case 3:
            {
                if (2 < max)
                {
                    u8 cont_byte[2] = {str[1], str[2]};
                    if (utf8_top5b_class[cont_byte[0] >> 3] == 0 &&
                        utf8_top5b_class[cont_byte[1] >> 3] == 0)
                    {
                        result.codepoint = (byte & Bitmask_4) << 12;
                        result.codepoint |= ((cont_byte[0] & Bitmask_6) << 6);
                        result.codepoint |=  (cont_byte[1] & Bitmask_6);
                        result.inc = 3;
                    }
                }
            } break;
            case 4:
            {
                if (3 < max)
                {
                    u8 cont_byte[3] = {str[1], str[2], str[3]};
                    if (utf8_top5b_class[cont_byte[0] >> 3] == 0 &&
                        utf8_top5b_class[cont_byte[1] >> 3] == 0 &&
                        utf8_top5b_class[cont_byte[2] >> 3] == 0)
                    {
                        result.codepoint = (byte & Bitmask_3) << 18;
                        result.codepoint |= ((cont_byte[0] & Bitmask_6) << 12);
                        result.codepoint |= ((cont_byte[1] & Bitmask_6) <<  6);
                        result.codepoint |=  (cont_byte[2] & Bitmask_6);
                        result.inc = 4;
                    }
                }
            } break;
        }
    }
    
    return result;
}

static Unicode_Consume utf8_consume(String string) {
    return utf8_consume(string.str, string.size);
}







static Unicode_Consume_Reverse utf8_consume_reverse(u8 *str, u64 max)
{
    Unicode_Consume_Reverse result = {1, ~0u};
    
    u64 length = 0;
    for (s64 index = (s64)max - 1;
         ;
         index -= 1)
    {
        if (length >= 4 || index < 0) {
            return result;
        }
        
        length += 1;
        u8 byte = str[index];
        
        if ((byte & 0b1100'0000) == 0b1100'0000 ||
            (byte & 0b1000'0000) == 0b0000'0000)
        {
            break;
        }
    }
    
    Unicode_Consume consume = utf8_consume(str + (max - length), length);
    result.dec = consume.inc;
    result.codepoint = consume.codepoint;
    return result;
}

static Unicode_Consume_Reverse utf8_consume_reverse(String string) {
    return utf8_consume_reverse(string.str, string.size);
}





static u32 utf8_write(u8 *str, u32 codepoint)
{
    u32 inc = 0;
    if (codepoint <= 0x7F)
    {
        str[0] = (u8)codepoint;
        inc = 1;
    }
    else if (codepoint <= 0x7FF)
    {
        str[0] = (Bitmask_2 << 6) | ((codepoint >> 6) & Bitmask_5);
        str[1] = (1 << 7) | (codepoint & Bitmask_6);
        inc = 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        str[0] = (Bitmask_3 << 5) | ((codepoint >> 12) & Bitmask_4);
        str[1] = (1 << 7) | ((codepoint >> 6) & Bitmask_6);
        str[2] = (1 << 7) | ( codepoint       & Bitmask_6);
        inc = 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        str[0] = (Bitmask_4 << 4) | ((codepoint >> 18) & Bitmask_3);
        str[1] = (1 << 7) | ((codepoint >> 12) & Bitmask_6);
        str[2] = (1 << 7) | ((codepoint >>  6) & Bitmask_6);
        str[3] = (1 << 7) | ( codepoint        & Bitmask_6);
        inc = 4;
    }
    else
    {
        str[0] = '?';
        inc = 1;
    }
    
    return inc;
}




static b32 str_equals(String str_a, String str_b)
{
    if (str_a.size != str_b.size) {
        return false;
    }
    
    for (u64 index = 0; index < str_a.size; index += 1)
    {
        if (str_a.str[index] != str_b.str[index]) {
            return false;
        }
    }
    
    return true;
}






static u32 unicode_codepoint_to_lower(u32 cp)
{
    // Taken from single header library utf8.h
    // from: https://github.com/sheredom/utf8.h/blob/master/utf8.h
    
    if (((0x0041 <= cp) && (0x005a >= cp)) ||
        ((0x00c0 <= cp) && (0x00d6 >= cp)) ||
        ((0x00d8 <= cp) && (0x00de >= cp)) ||
        ((0x0391 <= cp) && (0x03a1 >= cp)) ||
        ((0x03a3 <= cp) && (0x03ab >= cp)) ||
        ((0x0410 <= cp) && (0x042f >= cp)))
    {
        cp += 32;
    }
    else if ((0x0400 <= cp) && (0x040f >= cp))
    {
        cp += 80;
    }
    else if (((0x0100 <= cp) && (0x012f >= cp)) ||
             ((0x0132 <= cp) && (0x0137 >= cp)) ||
             ((0x014a <= cp) && (0x0177 >= cp)) ||
             ((0x0182 <= cp) && (0x0185 >= cp)) ||
             ((0x01a0 <= cp) && (0x01a5 >= cp)) ||
             ((0x01de <= cp) && (0x01ef >= cp)) ||
             ((0x01f8 <= cp) && (0x021f >= cp)) ||
             ((0x0222 <= cp) && (0x0233 >= cp)) ||
             ((0x0246 <= cp) && (0x024f >= cp)) ||
             ((0x03d8 <= cp) && (0x03ef >= cp)) ||
             ((0x0460 <= cp) && (0x0481 >= cp)) ||
             ((0x048a <= cp) && (0x04ff >= cp)))
    {
        cp |= 0x1;
    }
    else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
             ((0x0179 <= cp) && (0x017e >= cp)) ||
             ((0x01af <= cp) && (0x01b0 >= cp)) ||
             ((0x01b3 <= cp) && (0x01b6 >= cp)) ||
             ((0x01cd <= cp) && (0x01dc >= cp)))
    {
        cp += 1;
        cp &= ~0x1;
    }
    else
    {
        switch (cp)
        {
            default: break;
            case 0x0178: cp = 0x00ff; break;
            case 0x0243: cp = 0x0180; break;
            case 0x018e: cp = 0x01dd; break;
            case 0x023d: cp = 0x019a; break;
            case 0x0220: cp = 0x019e; break;
            case 0x01b7: cp = 0x0292; break;
            case 0x01c4: cp = 0x01c6; break;
            case 0x01c7: cp = 0x01c9; break;
            case 0x01ca: cp = 0x01cc; break;
            case 0x01f1: cp = 0x01f3; break;
            case 0x01f7: cp = 0x01bf; break;
            case 0x0187: cp = 0x0188; break;
            case 0x018b: cp = 0x018c; break;
            case 0x0191: cp = 0x0192; break;
            case 0x0198: cp = 0x0199; break;
            case 0x01a7: cp = 0x01a8; break;
            case 0x01ac: cp = 0x01ad; break;
            case 0x01af: cp = 0x01b0; break;
            case 0x01b8: cp = 0x01b9; break;
            case 0x01bc: cp = 0x01bd; break;
            case 0x01f4: cp = 0x01f5; break;
            case 0x023b: cp = 0x023c; break;
            case 0x0241: cp = 0x0242; break;
            case 0x03fd: cp = 0x037b; break;
            case 0x03fe: cp = 0x037c; break;
            case 0x03ff: cp = 0x037d; break;
            case 0x037f: cp = 0x03f3; break;
            case 0x0386: cp = 0x03ac; break;
            case 0x0388: cp = 0x03ad; break;
            case 0x0389: cp = 0x03ae; break;
            case 0x038a: cp = 0x03af; break;
            case 0x038c: cp = 0x03cc; break;
            case 0x038e: cp = 0x03cd; break;
            case 0x038f: cp = 0x03ce; break;
            case 0x0370: cp = 0x0371; break;
            case 0x0372: cp = 0x0373; break;
            case 0x0376: cp = 0x0377; break;
            case 0x03f4: cp = 0x03b8; break;
            case 0x03cf: cp = 0x03d7; break;
            case 0x03f9: cp = 0x03f2; break;
            case 0x03f7: cp = 0x03f8; break;
            case 0x03fa: cp = 0x03fb; break;
        }
    }
    
    return cp;
}




static b32 str_equals_ignore_case(String str_a, String str_b)
{
    for (;;)
    {
        if (!str_a.size || !str_b.size) {
            return (str_a.size == str_b.size);
        }
        
        Unicode_Consume consume_a = utf8_consume(str_a);
        Unicode_Consume consume_b = utf8_consume(str_b);
        u32 lower_a = unicode_codepoint_to_lower(consume_a.codepoint);
        u32 lower_b = unicode_codepoint_to_lower(consume_b.codepoint);
        
        if (lower_a != lower_b) {
            return false;
        }
        
        str_a = str_skip(str_a, consume_a.inc);
        str_b = str_skip(str_b, consume_b.inc);
    }
}
