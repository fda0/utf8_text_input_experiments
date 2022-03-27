#include "d3d11_1.h"
#include "d3dcompiler.h"

#define aling_to_16(X) (((X) + 0xf) & (~0xf))
#define aling_to_32(X) (((X) + 0xff) & (~0xff))


struct D3d11_State
{
    ID3D11DeviceContext1* device_context;
    IDXGISwapChain1* swap_chain;
    
    ID3D11Texture2D* frame_buffer;
    ID3D11RenderTargetView* frame_buffer_view;
    
    ID3D11DepthStencilState* depth_stencil_state;
    //ID3D11DepthStencilView* depth_buffer_view;
    
    ID3D11Buffer* constant_buffer;
    //ID3D11InputLayout* input_layout;
    //ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* index_buffer;
    ID3D11Buffer* raw_buffer;
    
    
    ID3D11VertexShader* vertex_shader;
    ID3D11RasterizerState1* rasterizer_state;
    ID3D11PixelShader* pixel_shader;
    
    ID3D11ShaderResourceView* raw_buffer_view;
    
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* texture_view;
    ID3D11SamplerState* sampler_state;
    
    ID3D11BlendState* blend_state;
    
#if defined(_DEBUG)
    ID3D11InfoQueue* info_queue;
#endif
};




struct Ui_Constants
{
    f32 screen_width_inv, screen_height_inv;
    f32 atlas_width_inv, atlas_height_inv;
    f32 anim_t;
};

struct Ui_State
{
    // this could be made to grow dynamicly with VirtualAlloc
    u32 raw_data[1024*1024*2];
    u32 raw_count;
    
    // layers used for covinient UI Z-ordering
#define Indicies_LayersCount (2) // can be eaisly extended
#define Indicies_LayerElementCount (1024*512)
#define Indicies_AllCount (Indicies_LayersCount*Indicies_LayerElementCount)
    
    u32 indicies_data[Indicies_LayersCount][Indicies_LayerElementCount];
    u32 indicies_counts[Indicies_LayersCount];
    
    u32 layer;
    Ui_Constants constants;
};



// Graphics global memory
static D3d11_State d3_state;
static Ui_State ui_state;

#define TEXTURE_WIDTH  (2048*4)
#define TEXTURE_HEIGHT (256)




static void d3d11_check_debug_errors()
{
#if defined(_DEBUG)
    d3_state.info_queue->PushEmptyStorageFilter();
    u64 message_count = d3_state.info_queue->GetNumStoredMessages();
    
    for (u64 i = 0; i < message_count; i++)
    {
        u64 message_size = 0;
        d3_state.info_queue->GetMessage(i, nullptr, &message_size); //get the size of the message
        
        D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(message_size); //allocate enough space
        d3_state.info_queue->GetMessage(i, message, &message_size); //get the actual message
        
        //do whatever you want to do with it
        //printf("Directx11: %.*s", message->DescriptionByteLength, message->pDescription);
        OutputDebugStringA(message->pDescription);
        OutputDebugStringA("\n\n");
        
        static b32 skip_stupid_error_debug = 0;
        if (!skip_stupid_error_debug) skip_stupid_error_debug = 1;
        else debug_break();
        
        free(message);
    }
    
    d3_state.info_queue->ClearStoredMessages();
#endif
}



static void initialize_d3d11_graphics(HWND window)
{
    //
    // D3D11 boilerplate based on https://gist.github.com/d7samurai/261c69490cce0620d0bfc93003cd1052
    //
    
    
    u32 d3d11_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    d3d11_device_flags |= (D3D11_CREATE_DEVICE_DEBUG);
#endif
    
    
    ID3D11Device* base_device;
    ID3D11DeviceContext* base_device_context;
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
    
    HRESULT create_dev_res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, d3d11_device_flags,
                                               feature_levels, array_count(feature_levels), D3D11_SDK_VERSION,
                                               &base_device, nullptr, &base_device_context);
    
    
    ////////////////////////////
    ID3D11Device1* device;
    HRESULT device_query_res = base_device->QueryInterface(__uuidof(ID3D11Device1), (void **)(&device));
    
    base_device_context->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)(&d3_state.device_context));
    
    
#if defined(_DEBUG)
    device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)(&d3_state.info_queue));
#endif
    
    
    
    
    ////////////////////////////
    {
        IDXGIDevice1* dxgi_device;
        device->QueryInterface(__uuidof(IDXGIDevice1), (void **)(&dxgi_device));
        
        IDXGIAdapter* dxgi_adapter;
        dxgi_device->GetAdapter(&dxgi_adapter);
        
        IDXGIFactory2* dxgi_factory;
        dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), (void **)(&dxgi_factory));
        
        
        ////////////////////////////
        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.Width              = 0; // use window width
        swap_chain_desc.Height             = 0; // use window height
        swap_chain_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        swap_chain_desc.Stereo             = FALSE;
        swap_chain_desc.SampleDesc.Count   = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount        = 2;
        swap_chain_desc.Scaling            = DXGI_SCALING_STRETCH;
        swap_chain_desc.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;
        swap_chain_desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swap_chain_desc.Flags              = 0;
        
        
        dxgi_factory->CreateSwapChainForHwnd(device, window, &swap_chain_desc,
                                             nullptr, nullptr, &d3_state.swap_chain);
    }
    
    
    ////////////////////////////
    {
        d3_state.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)(&d3_state.frame_buffer));
        device->CreateRenderTargetView(d3_state.frame_buffer, nullptr, &d3_state.frame_buffer_view);
    }
    
    
    ////////////////////////////
#if 0
    {
        D3D11_TEXTURE2D_DESC depth_buffer_desc;
        d3_state.frame_buffer->GetDesc(&depth_buffer_desc); // base on framebuffer properties
        
        depth_buffer_desc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        
        ID3D11Texture2D* depth_buffer;
        device->CreateTexture2D(&depth_buffer_desc, nullptr, &depth_buffer);
        
        device->CreateDepthStencilView(depth_buffer, nullptr, &d3_state.depth_buffer_view);
    }
#endif
    
    ////////////////////////////
    {
        // @todo precompile shaders
        ID3DBlob* vs_errors;
        ID3DBlob* vs_blob;
        HRESULT compile_res = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "vs_main",
                                                 "vs_5_0", 0, 0, &vs_blob, &vs_errors);
        
        if (compile_res)
        {
            LPVOID void_errors = vs_errors->GetBufferPointer();
            char *char_errors = (char *)void_errors;
            debug_break();
        }
        
        
        device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                   nullptr, &d3_state.vertex_shader);
        
#if 0
        D3D11_INPUT_ELEMENT_DESC input_element_desc[] =
        {
            //{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        
        device->CreateInputLayout(input_element_desc, array_count(input_element_desc),
                                  vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                  &d3_state.input_layout);
#endif
    }
    
    
    ////////////////////////////
    {
        ID3DBlob* ps_blob;
        D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &ps_blob, nullptr);
        device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &d3_state.pixel_shader);
    }
    
    
    ////////////////////////////
    {
        D3D11_RASTERIZER_DESC1 rasterizer_desc = {};
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        device->CreateRasterizerState1(&rasterizer_desc, &d3_state.rasterizer_state);
    }
    
    
    ////////////////////////////
    {
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        device->CreateSamplerState(&sampler_desc, &d3_state.sampler_state);
    }
    
    
    ////////////////////////////
#if 1
    {
        D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
        //depth_stencil_desc.DepthEnable    = FALSE;
        //depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        //depth_stencil_desc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
        device->CreateDepthStencilState(&depth_stencil_desc, &d3_state.depth_stencil_state);
    }
#endif
    
    
    
    ////////////////////////////
    {
        D3D11_BUFFER_DESC constant_buffer_desc = {};
        constant_buffer_desc.ByteWidth      = (aling_to_16(sizeof(ui_state.constants)));
        constant_buffer_desc.Usage          = D3D11_USAGE_DYNAMIC;
        constant_buffer_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        device->CreateBuffer(&constant_buffer_desc, nullptr, &d3_state.constant_buffer);
    }
    
    
    ////////////////////////////
#if 0
    D3D11_BUFFER_DESC vertex_buffer_desc = {};
    vertex_buffer_desc.ByteWidth = sizeof(raw_vertex_data);
    vertex_buffer_desc.Usage     = D3D11_USAGE_IMMUTABLE;
    vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vertex_data = {raw_vertex_data};
    device->CreateBuffer(&vertex_buffer_desc, &vertex_data, &d3_state.vertex_buffer);
#endif
    
    
    ////////////////////////////
    {
        D3D11_BUFFER_DESC index_buffer_desc = {};
        index_buffer_desc.ByteWidth = sizeof(ui_state.indicies_data);
        index_buffer_desc.Usage     = D3D11_USAGE_DYNAMIC;
        index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        index_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&index_buffer_desc, nullptr, &d3_state.index_buffer);
    }
    
    
    ////////////////////////////
    {
        D3D11_BUFFER_DESC raw_buffer_desc = {};
        raw_buffer_desc.ByteWidth = sizeof(ui_state.raw_data);
        raw_buffer_desc.Usage     = D3D11_USAGE_DYNAMIC;
        raw_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
        raw_buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        raw_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        HRESULT raw_res = device->CreateBuffer(&raw_buffer_desc, nullptr, &d3_state.raw_buffer);
        break_at(!raw_res);
    }
    
    
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC raw_view_desc = {};
        raw_view_desc.Format = DXGI_FORMAT_R32_TYPELESS;
        raw_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        raw_view_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        raw_view_desc.BufferEx.NumElements = sizeof(ui_state.raw_data) / sizeof(u32);
        
        HRESULT raw_view_res = device->CreateShaderResourceView(d3_state.raw_buffer, &raw_view_desc, &d3_state.raw_buffer_view);
        break_at(!raw_view_res);
    }
    
    
    ////////////////////////////
    {
        D3D11_TEXTURE2D_DESC texture_desc = {};
        texture_desc.Width              = TEXTURE_WIDTH;  // in data.h
        texture_desc.Height             = TEXTURE_HEIGHT; // in data.h
        texture_desc.MipLevels          = 1;
        texture_desc.ArraySize          = 1;
        texture_desc.Format             = DXGI_FORMAT_A8_UNORM;
        texture_desc.SampleDesc.Count   = 1;
        texture_desc.Usage              = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        
        //D3D11_SUBRESOURCE_DATA texture_data = {};
        //texture_data.pSysMem            = raw_texture_data;
        //texture_data.SysMemPitch        = TEXTURE_WIDTH * 1; // 4 bytes per pixel
        //
        //device->CreateTexture2D(&texture_desc, &texture_data, &d3_state.texture);
        device->CreateTexture2D(&texture_desc, nullptr, &d3_state.texture);
        
        device->CreateShaderResourceView(d3_state.texture, nullptr, &d3_state.texture_view);
    }
    
    
    ////////////////////////////
    {
        D3D11_BLEND_DESC blend_desc = {};
        blend_desc.RenderTarget[0].BlendEnable = true;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&blend_desc, &d3_state.blend_state);
    }
    
    
    
    d3d11_check_debug_errors();
}


static void d3d11_update_glyph_texture(u32 tex_x, u32 tex_y,
                                       u32 image_width, u32 image_height,
                                       u32 image_pitch, u8 *image_memory)
{
    assert(image_width && image_height);
    
    D3D11_BOX update_box = {};
    update_box.back = 1;
    
    update_box.left = tex_x;
    update_box.right = tex_x + image_width;
    update_box.top = tex_y;
    update_box.bottom = tex_y + image_height;
    assert(update_box.right <= TEXTURE_WIDTH && update_box.bottom <= TEXTURE_HEIGHT);
    
    d3_state.device_context->UpdateSubresource(d3_state.texture, 0, &update_box,
                                               image_memory, image_pitch,
                                               image_pitch*image_height); // @todo check if that arg can be 0?
}



static void d3d11_render()
{
    FLOAT background_color[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    
    
    // @todo revisit this code after adding framebuffer resizing
    D3D11_TEXTURE2D_DESC buffer_desc;
    d3_state.frame_buffer->GetDesc(&buffer_desc);
    
    D3D11_VIEWPORT viewport = {
        0.0f, 0.0f,
        //(float)client_rect.right, (float)client_rect.bottom,
        (float)(buffer_desc.Width), (float)(buffer_desc.Height),
        //500.f, 1000.f,
        0.0f, 1.0f
    };
    
    ////////////////////////////
    float w = viewport.Width / viewport.Height; // width (aspect ratio)
    float h = 1.0f;                             // height
    float n = 1.0f;                             // near
    float f = 9.0f;                             // far
    
    
    
    ////////////////////////////
    ID3D11DeviceContext1* device_context = d3_state.device_context;
    
    {
        D3D11_MAPPED_SUBRESOURCE mapped_constants;
        device_context->Map(d3_state.constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_constants);
        memcpy(mapped_constants.pData, &ui_state.constants, sizeof(ui_state.constants));
        device_context->Unmap(d3_state.constant_buffer, 0);
    }
    
    u32 indicies_total_size = 0;
    {
        D3D11_MAPPED_SUBRESOURCE mapped_indicies;
        device_context->Map(d3_state.index_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_indicies);
        
        for (u32 layer_index = 0; layer_index < Indicies_LayersCount; layer_index += 1)
        {
            u32 this_layer_elements_count = ui_state.indicies_counts[layer_index];
            
            memcpy((u32 *)mapped_indicies.pData + indicies_total_size,
                   &ui_state.indicies_data[layer_index][0],
                   sizeof(u32)*this_layer_elements_count);
            
            indicies_total_size += this_layer_elements_count;
        }
        
        device_context->Unmap(d3_state.index_buffer, 0);
    }
    
    {
        D3D11_MAPPED_SUBRESOURCE mapped_raw;
        device_context->Map(d3_state.raw_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_raw);
        memcpy(mapped_raw.pData, ui_state.raw_data, sizeof(u32)*ui_state.raw_count);
        device_context->Unmap(d3_state.raw_buffer, 0);
    }
    
    
    ////////////////////////
    device_context->ClearRenderTargetView(d3_state.frame_buffer_view, background_color);
    //device_context->ClearDepthStencilView(d3_state.depth_buffer_view, D3D11_CLEAR_DEPTH, 1.0f, 0);
    
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //device_context->IASetInputLayout(d3_state.input_layout);
    //device_context->IASetVertexBuffers(0, 1, &d3_state.vertex_buffer, &stride, &offset);
    device_context->IASetIndexBuffer(d3_state.index_buffer, DXGI_FORMAT_R32_UINT, 0);
    
    device_context->VSSetShader(d3_state.vertex_shader, nullptr, 0);
    device_context->VSSetConstantBuffers(0, 1, &d3_state.constant_buffer);
    device_context->VSSetShaderResources(0, 1, &d3_state.raw_buffer_view);
    
    device_context->RSSetViewports(1, &viewport);
    device_context->RSSetState(d3_state.rasterizer_state);
    
    device_context->PSSetShader(d3_state.pixel_shader, nullptr, 0);
    device_context->PSSetConstantBuffers(0, 1, &d3_state.constant_buffer);
    device_context->PSSetShaderResources(0, 1, &d3_state.texture_view);
    device_context->PSSetSamplers(0, 1, &d3_state.sampler_state);
    
    device_context->OMSetRenderTargets(1, &d3_state.frame_buffer_view, nullptr);
    //device_context->OMSetRenderTargets(1, &d3_state.frame_buffer_view, d3_state.depth_buffer_view);
    device_context->OMSetDepthStencilState(d3_state.depth_stencil_state, 0);
    device_context->OMSetBlendState(d3_state.blend_state, nullptr, 0xffffffff);
    
    ////////////////////////
    device_context->DrawIndexed(indicies_total_size, 0, 0);
    
    ////////////////////////
    d3_state.swap_chain->Present(1, 0);
    
    
    d3d11_check_debug_errors();
}






//
// Draw
//
static void clear_ui_state()
{
    ui_state.raw_count = 0;
    memset(ui_state.indicies_counts, 0, sizeof(ui_state.indicies_counts));
    ui_state.layer = 0;
}


static u32 *push_indicies(u32 count)
{
    u32 layer = ui_state.layer;
    assert(layer >= 0 && layer < Indicies_LayersCount);
    
    u32 *indicies = ui_state.indicies_data[layer] + ui_state.indicies_counts[layer];
    ui_state.indicies_counts[layer] += count;
    assert(ui_state.indicies_counts[layer] <= Indicies_LayerElementCount);
    return indicies;
}

static u32 *push_and_fill_quad_indicies(u32 type)
{
    u32 *indicies = push_indicies(6);
    
    u32 address = ui_state.raw_count * 4;
    type <<= 24;
    
    indicies[0] = ((0b00u << 30u) | address | type);
    indicies[1] = ((0b01u << 30u) | address | type);
    indicies[2] = ((0b10u << 30u) | address | type);
    indicies[3] = ((0b01u << 30u) | address | type);
    indicies[4] = ((0b11u << 30u) | address | type);
    indicies[5] = ((0b10u << 30u) | address | type);
    return indicies;
}

static u32 *push_raw(u32 count)
{
    u32 *raw_u32 = ui_state.raw_data + ui_state.raw_count;
    f32 *raw_f32 = (f32 *)raw_u32;
    ui_state.raw_count += count;
    assert(ui_state.raw_count <= array_count(ui_state.raw_data));
    return raw_u32;
}


static void render_text_input_rect(f32 x, f32 y, f32 w, f32 h, f32 radius, u32 rgba_inner, u32 rgba_border)
{
    push_and_fill_quad_indicies(0);
    
    u32 *raw_u32 = push_raw(7);
    f32 *raw_f32 = (f32 *)raw_u32;
    raw_f32[0] = x;
    raw_f32[1] = y;
    raw_f32[2] = w;
    raw_f32[3] = h;
    raw_f32[4] = radius;
    raw_u32[5] = rgba_inner;
    raw_u32[6] = rgba_border;
}

static void render_text_input_rect(Rect rect, f32 radius, u32 rgba_inner, u32 rgba_border)
{
    render_text_input_rect(rect.x, rect.y, rect.w, rect.h, radius, rgba_inner, rgba_border);
}



static void render_glyph_tex(f32 x, f32 y, f32 w, f32 h, f32 tex_x, f32 tex_y, u32 rgba)
{
    push_and_fill_quad_indicies(1);
    
    u32 *raw_u32 = push_raw(7);
    f32 *raw_f32 = (f32 *)raw_u32;
    raw_f32[0] = roundf(x);
    raw_f32[1] = roundf(y);
    raw_f32[2] = w;
    raw_f32[3] = h;
    raw_f32[4] = tex_x;
    raw_f32[5] = tex_y;
    raw_u32[6] = rgba;
}

