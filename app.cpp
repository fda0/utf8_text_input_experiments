/*
[ ] avoid scanning the whole text input if it isn't dirty (should be easy)
[ ] dynamically estimate required texture size for a set of fonts?

[ ] add more textboxes to this demo program

[ ] support copy/paste?

[ ] d3d11 support framebuffer resize

[ ] use Rect / V2 types to simplify api?
[ ] complete string api (str_compare, str_index_of, str_index_of_reverse, str_starts_with, str_ends_with, str_trim_whitespace)
[ ] test string api
[ ] more testing for text_input?
*/


#pragma comment(lib, "User32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "uxtheme.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"




#include "app_shared.h"
#include "d3d11_graphics.cpp"

#include "utf8_strings.h"
#include "utf8_textedit.h"
#include "tests.cpp"

#define STBTT_STATIC 1
#define STBTT_assert(x) assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "app_glyph_cache.h"





struct Input_Event
{
    u8 vk_code; // is equal to zero if str is filled
    u8 shift : 1;
    u8 control : 1;
    u8 alt : 1;
    u8 released : 1;
    char cstr[4]; // single codepoint encoded in utf8 - zero terminated if it's less than 4 bytes
};

struct App_State
{
    HWND window;
    wchar_t wm_char_high_surrogate;
    
    Input_Event input_events[64];
    u32 input_event_count;
    
    b8 left_mouse_down;
    b8 left_mouse_down_first_frame;
    v2 mouse_pos;
    void *ui_active;
    
    s64 last_frame_time;
    f32 delta_t;
    f32 anim_t;
    f32 screen_width, screen_height;
};
static App_State app_state;












static s32 throw_windows_message(char *message)
{
    MSGBOXPARAMSA params = {};
    params.cbSize = sizeof(params);
    params.lpszText = message;
    params.dwStyle = MB_ICONERROR | MB_TASKMODAL;
    params.dwStyle |= MB_OK;
    
    s32 message_res = MessageBoxIndirectA(&params);
    return message_res;
}

static void assert_and_throw_message(b32 expression, char *message)
{
    if (!expression)
    {
        throw_windows_message(message);
        ExitProcess(1);
    }
}


static LRESULT window_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_QUIT:
        case WM_CLOSE:
        {
            ExitProcess(0);
        } break;
        
        case WM_SYSKEYDOWN:
        {
            if ((wParam == VK_F4) && (lParam & 29)) {
                ExitProcess(0);
            }
        } break;
        
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
            b8 is_down = (message == WM_LBUTTONDOWN);
            app_state.left_mouse_down_first_frame = is_down;
            
            if (app_state.input_event_count < array_count(app_state.input_events))
            {
                Input_Event *event = &app_state.input_events[app_state.input_event_count++];
                *event = {};
                event->vk_code = (u8)VK_LBUTTON;
                event->released = !is_down;
            }
        } break;
        
        case WM_KEYDOWN:
        {
            if (app_state.input_event_count < array_count(app_state.input_events))
            {
                Input_Event *event = &app_state.input_events[app_state.input_event_count++];
                *event = {};
                event->vk_code = (u8)wParam;
                event->shift = !!(GetKeyState(VK_SHIFT) & (1 << 15));
                event->control = !!(GetKeyState(VK_CONTROL) & (1 << 15));
                event->alt = !!(lParam & (1 << 29));
            }
        } break;
        
        case WM_CHAR:
        {
            wchar_t character = (wchar_t)wParam;
            wchar_t wide_chars[2];
            u32 wide_char_count = 0;
            
            if (IS_HIGH_SURROGATE(character))
            {
                app_state.wm_char_high_surrogate = character;
            }
            else if (IS_LOW_SURROGATE(character))
            {
                if (IS_SURROGATE_PAIR(app_state.wm_char_high_surrogate, character))
                {
                    wide_chars[0] = app_state.wm_char_high_surrogate;
                    wide_chars[1] = character;
                    wide_char_count = 2;
                }
                
                app_state.wm_char_high_surrogate = 0;
            }
            else
            {
                app_state.wm_char_high_surrogate = 0;
                wide_chars[0] = character;
                wide_char_count = 1;
            }
            
            
            if (wide_char_count &&
                app_state.input_event_count < array_count(app_state.input_events))
            {
                // Filter out garbage characters that get sent when you press Control + some key
                // (You might want to make an exception for a new line character)
                if (!(wide_char_count == 1 && wide_chars[0] < ' ' || wide_chars[0] == 0x7F))
                {
                    Input_Event *event = &app_state.input_events[app_state.input_event_count++];
                    *event = {};
                    event->shift = !!(GetKeyState(VK_SHIFT) & (1 << 15));
                    event->control = !!(GetKeyState(VK_CONTROL) & (1 << 15));
                    
                    s32 count = WideCharToMultiByte(CP_UTF8, 0,
                                                    wide_chars, wide_char_count,
                                                    event->cstr, array_count(event->cstr),
                                                    nullptr, false);
                }
            }
        } break;
        
        default:
        {
            result = DefWindowProcW(window, message, wParam, lParam);
        } break;
    }
    return result;
}







static f32 render_codepoint(Font *font, u32 codepoint, f32 x, f32 y, u32 rgba)
{
    Glyph first_glyph = get_glyph(font, codepoint);
    
    if (first_glyph.status == Glyph_Invalid)
    {
        // You can do something more fancy for missing codepoints here
        // But I feel like it should be more application specific.
        // You might want to use smaller font to draw codepoint code like: U+1234
        // Or you can to draw a fancy rectangle or a valve-style-checkerboard
        rgba ^= Bitmask_24;
        codepoint = '?';
        first_glyph = get_glyph(font, codepoint);
    }
    
    
    Glyph glyph = first_glyph;
    x = roundf(x + (f32)glyph.offset_x);
    y = roundf(y + (f32)glyph.offset_y);
    
    for (s32 index = 0;;)
    {
#if 0 // debug code for coloring glyph segments
        s32 ii = index + 1;
        rgba = (255 << 24 |
                ii*32 << 16 |
                ii*32 << 8 |
                ii*32 << 0);
#endif
        
        push_and_fill_quad_indicies(1);
        
        u32 *raw_u32 = push_raw_data(8);
        f32 *raw_f32 = (f32 *)raw_u32;
        raw_u32[0] = ui_state.active_clip_address;
        raw_f32[1] = x;
        raw_f32[2] = y;
        raw_f32[3] = (f32)glyph.width;
        raw_f32[4] = (f32)glyph.height;
        raw_f32[5] = (f32)glyph.tex_x;
        raw_f32[6] = (f32)glyph.tex_y;
        raw_u32[7] = rgba;
        
        
        index += 1;
        if (index <= (s32)first_glyph.additional_segment_count)
        {
            x += (f32)glyph.width;
            glyph = get_glyph(font, codepoint | (index << 21));
            
            if (glyph.status != Glyph_Loaded)
            {
                assert(0);
                break;
            }
        }
        else
        {
            break;
        }
    }
    
    return first_glyph.advance;
}




static void draw_text_input_inner(Text_Input *text, Font *font,
                                  Rect text_rect, f32 padding_x, f32 padding_y,
                                  b32 update_scroll_x_and_skip_drawing)
{
    f32 scroll_x;
    
    if (!update_scroll_x_and_skip_drawing)
    {
        // pretty hacky animation technique but it works for this purpose
        // produces animations with exponential falloff
        f32 epsilon = 1.f;
        f32 scroll_speed = (app_state.ui_active == text ? 10.f : 20.f);
        
        f32 diff = text->target_scroll_x - text->current_scroll_x;
        if (fabsf(diff) <= epsilon)
        {
            text->current_scroll_x = text->target_scroll_x;
        }
        else
        {
            text->current_scroll_x += diff * app_state.delta_t * scroll_speed;
        }
        
        scroll_x = text->current_scroll_x;
    }
    else
    {
        scroll_x = text->target_scroll_x;
    }
    
    
    
    f32 text_x1 = text_rect.x + text_rect.w;
    f32 text_y1 = text_rect.y + text_rect.h;
    
    f32 pos_x = text_rect.x + padding_x + scroll_x;
    f32 base_y = text_y1 - padding_y + font->descent;
    
    
    if (!update_scroll_x_and_skip_drawing)
    {
        ui_state.layer = 0;
        f32 border_share = 0.1f;
        render_text_input_rect(text_rect, 0.2f, border_share, 0xff11aaff, 0xff001122);
        ui_state.layer = 1;
        
        f32 border = border_share*text_rect.h*0.5f;
        Rect text_rect_minus_border = rect_contract(text_rect, border);
        set_clip_rect(text_rect_minus_border);
    }
    
    
    f32 cursor_y0 = text_y1 - padding_y + font->descent - font->ascent;
    f32 cursor_h = font->ascent - font->descent;
    f32 cursor_w = 2.f + cursor_h*0.05f;
    
    
    s64 cursor_min = get_min(text->cursor_pos, text->mark_pos);
    s64 cursor_max = get_max(text->cursor_pos, text->mark_pos);
    f32 selection_min = 0.f;
    f32 selection_max = 0.f;
    
    f32 mouse_best_distance = 10000000.f;
    s64 mouse_set_cursor_pos = -1;
    f32 current_target_scroll_diff = text->target_scroll_x - text->current_scroll_x;
    
    
    s64 byte_index = 0;
    String text_string = text->buffer;
    
    
    
    auto update_cursor = [&]() {
        if (!update_scroll_x_and_skip_drawing)
        {
            if (cursor_min == byte_index) {
                selection_min = pos_x;
            }
            if (cursor_max == byte_index) {
                selection_max = pos_x;
            }
            
            if (text->cursor_pos == byte_index)
            {
                render_text_input_rect(pos_x, cursor_y0,
                                       cursor_w, cursor_h, 
                                       0.05f, 0.f, 0xffff'ffff, 0xffff'ffff);
            }
        }
        else
        {
            if (text->cursor_pos == byte_index)
            {
                if (pos_x + padding_x > text_x1)
                {
                    text->target_scroll_x -= (pos_x + padding_x - text_x1);
                }
                if (pos_x - padding_x < text_rect.x)
                {
                    text->target_scroll_x += (-pos_x + padding_x + text_rect.x);
                }
            }
            
            
            
            if (app_state.ui_active == text)
            {
                f32 diff = fabsf(app_state.mouse_pos.x - pos_x + current_target_scroll_diff);
                if (diff < mouse_best_distance)
                {
                    mouse_best_distance = diff;
                    mouse_set_cursor_pos = byte_index;
                }
            }
        }
    };
    
    
    
    
    
    update_cursor();
    
    
    
    while (text_string.size)
    {
        Glyph glyph = {};
        u32 consume_inc = 0;
        u32 glyph_color = 0xff000513;
        
        Unicode_Consume consume = utf8_consume(text_string);
        consume_inc = consume.inc;
        
        text_string = str_skip(text_string, consume.inc);
        f32 glyph_advance = 0.f;
        
        if (!update_scroll_x_and_skip_drawing)
        {
            glyph_advance = render_codepoint(font, consume.codepoint, pos_x, base_y, glyph_color);
        }
        else
        {
            glyph_advance = get_glyph(font, consume.codepoint).advance;
        }
        
        pos_x += glyph_advance;
        byte_index += consume_inc;
        
        update_cursor();
    }
    
    
    
    
    if (!update_scroll_x_and_skip_drawing)
    {
        ui_state.layer = 0;
        if (selection_min != selection_max)
        {
            f32 selection_width = selection_max - selection_min + cursor_w;
            
            render_text_input_rect(selection_min, cursor_y0,
                                   selection_width, cursor_h,
                                   0.05f, 0.05f, 0xff0000ff, 0xff000066);
        }
        
        reset_clip_rect();
    }
    else
    {
        if (text->target_scroll_x < 0)
        {
            f32 empty_space_on_right_side = (text_x1 - padding_x) - pos_x;
            if (empty_space_on_right_side > 0)
            {
                text->target_scroll_x += empty_space_on_right_side;
            }
        }
        
        if (text->target_scroll_x > 0 &&
            pos_x < (text_x1 - padding_x))
        {
            text->target_scroll_x = 0;
        }
        
        text->scroll_anim_start = scroll_x;
        text->scroll_anim_t = 0.f;
    }
    
    
    if (mouse_set_cursor_pos >= 0)
    {
        text->cursor_pos = mouse_set_cursor_pos;
        if (app_state.left_mouse_down_first_frame) {
            text->mark_pos = text->cursor_pos;
        }
    }
}



static void draw_text_input(Text_Input *text, Font *font,
                            Rect text_rect, f32 padding_x, f32 padding_y)
{
    if (app_state.ui_active == text)
    {
        text->is_dirty = true;
    }
    
    if (text->is_dirty)
    {
        draw_text_input_inner(text, font, text_rect, padding_x, padding_y, true);
        text->is_dirty = false;
    }
    
    draw_text_input_inner(text, font, text_rect, padding_x, padding_y, false);
}






int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    run_string_unit_tests();
    run_utf8_unit_tests();
    run_text_input_unit_tests();
    
    
    
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &window_procedure;
    window_class.hInstance = hInstance;
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.lpszClassName = L"TextInputClass";
    RegisterClassExW(&window_class);
    
    
    //DWORD window_style = WS_OVERLAPPEDWINDOW; // @todo support framebuffer resizing
    DWORD window_style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    app_state.window = CreateWindowExW(0, window_class.lpszClassName, L"Text Input",
                                       window_style, CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, nullptr);
    
    
    initialize_d3d11_graphics(app_state.window);
    ShowWindow(app_state.window, SW_SHOW);
    
    
    
    
    char *font_data = nullptr;
    {
        auto arial = L"C:\\Windows\\Fonts\\arial.ttf";
        auto times = L"C:\\Windows\\Fonts\\times.ttf";
        auto hack = L"C:/Users/Bowser/Appdata/Local/Microsoft/Windows/Fonts/Hack-Regular.ttf";
        auto courier = L"C:\\Windows\\Fonts\\cour.ttf";
        auto alger = L"C:\\Windows\\Fonts\\alger.ttf"; // doesn't have Polish letters
            
        HANDLE font_file = CreateFileW(times, GENERIC_READ,
                                       FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr,
                                       OPEN_EXISTING, 0, nullptr);
        assert_and_throw_message(font_file != INVALID_HANDLE_VALUE, "Failed to open arial.ttf");
        
        
        DWORD font_size = GetFileSize(font_file, nullptr);
        font_data = (char *)VirtualAlloc(0, font_size+1, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        
        
        DWORD bytes_read = 0;
        b32 read_res = ReadFile(font_file, font_data, font_size, &bytes_read, nullptr);
        assert_and_throw_message(read_res && bytes_read == font_size, "Failed to read arial.ttf");
        
        font_data[font_size] = 0;
        CloseHandle(font_file);
    }
    
    Font *font = (Font *)VirtualAlloc(0, sizeof(Font), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    initialize_font(font, font_data, 160.f);
    
    
    ui_state.constants.atlas_width_inv = 1.f / TEXTURE_WIDTH;
    ui_state.constants.atlas_height_inv = 1.f / TEXTURE_HEIGHT;
    
    
    
    
    
    u8 text_input_memory[32];
    Text_Input text_input = {};
    text_input.buffer = {
        text_input_memory, 0, array_count(text_input_memory)
    };
    
    
    app_state.last_frame_time = time_perf();
    u64 frame_number = 0;
    
    for(;;)
    {
        frame_number += 1;
        start_frame_for_ui_state();
        app_state.input_event_count = 0;
        app_state.left_mouse_down_first_frame = false;
        
        {
            app_state.input_event_count = 0;
            
            MSG msg;
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            
            POINT mouse_point;
            GetCursorPos(&mouse_point);
            ScreenToClient(app_state.window, &mouse_point);
            app_state.mouse_pos = {
                (f32)mouse_point.x,
                (f32)mouse_point.y
            };
            
            app_state.left_mouse_down = !!(GetKeyState(VK_LBUTTON) & (1 << 15));
        }
        
        {
            s64 now_time = time_perf();
            app_state.delta_t = time_elapsed(now_time, app_state.last_frame_time);
            app_state.last_frame_time = now_time;
            
            app_state.anim_t += app_state.delta_t;
            while (app_state.anim_t > 1000.f) {
                app_state.anim_t -= 1000.f;
            }
            
            ui_state.constants.anim_t = app_state.anim_t;
        }
        
        
        
        RECT client_rect;
        GetClientRect(app_state.window, &client_rect);
        f32 screen_width = (f32)client_rect.right;
        f32 screen_height = (f32)client_rect.bottom;
        
        if (screen_width && screen_height &&
            app_state.screen_width != screen_width &&
            app_state.screen_height != screen_height)
        {
            app_state.screen_width = screen_width;
            app_state.screen_height = screen_height;
            
            ui_state.constants.screen_width_inv = 1.f / screen_width;
            ui_state.constants.screen_height_inv = 1.f / screen_height;
            
            // @todo resize frame buffer etc
        }
        
        
        
        f32 text_padding_y = font->max_height*0.1f;
        f32 text_padding_x = text_padding_y*2.f + 4.f;
        Rect text_input_rect = {};
        text_input_rect.w = font->max_height*5.f + 2.f*text_padding_x;
        text_input_rect.h = font->max_height + 2.f*text_padding_y;
        text_input_rect.x = screen_width*0.5f - text_input_rect.w*0.5f;
        text_input_rect.y = screen_height*0.5f - text_input_rect.h*0.5f;
        
        
        
        
        for (u64 input_index = 0; input_index < app_state.input_event_count; input_index += 1)
        {
            Input_Event event = app_state.input_events[input_index];
            
            switch (event.vk_code)
            {
                case 0: {
                    String new_text = cstr_to_string(event.cstr);
                    text_input_write(&text_input, new_text);
                } break;
                
                case VK_BACK:
                case VK_DELETE: {
                    s64 dir = (event.vk_code == VK_BACK ? -1 : 1);
                    u32 flags = TextInputMove_Select;
                    if (event.control) { flags |= TextInputMove_ByWords; }
                    
                    if (!text_input_has_selection(&text_input)) {
                        text_input_move_cursor(&text_input, dir, flags);
                    }
                    text_input_write(&text_input, ""_f0);
                } break;
                
                case VK_LEFT:
                case VK_RIGHT: {
                    s64 dir = (event.vk_code == VK_LEFT ? -1 : 1);
                    u32 flags = 0;
                    if (event.shift) { flags |= TextInputMove_Select; }
                    if (event.control) { flags |= TextInputMove_ByWords; }
                    text_input_move_cursor(&text_input, dir, flags);
                } break;
                
                case VK_HOME:
                case VK_END: {
                    s64 dir = (event.vk_code == VK_HOME ? -1 : 1);
                    u32 flags = TextInputMove_ByMax;
                    if (event.shift) { flags |= TextInputMove_Select; }
                    text_input_move_cursor(&text_input, dir, flags);
                } break;
                
                case VK_LBUTTON: {
                    if (!event.released && v2_in_rect(text_input_rect, app_state.mouse_pos)) {
                        app_state.ui_active = &text_input;
                    } else {
                        app_state.ui_active = nullptr;
                    }
                } break;
                
                case 'A': {
                    if (event.control && !event.alt)
                    {
                        text_input_move_cursor(&text_input, -1, TextInputMove_ByMax);
                        text_input_move_cursor(&text_input, 1, TextInputMove_ByMax | TextInputMove_Select);
                    }
                } break;
            }
        }
        
        if (!app_state.left_mouse_down) {
            app_state.ui_active = nullptr;
        }
        
        
        
        draw_text_input(&text_input, font, text_input_rect, text_padding_x, text_padding_y);
        
        
        
        d3d11_render();
    }
}

