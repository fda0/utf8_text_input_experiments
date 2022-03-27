
cbuffer constants : register(b0)
{
    float2 screen_dim_inv;
    float2 atlas_dim_inv;
    float anim_t;
}

struct Vs_In
{
    uint vertex_id : SV_VertexID;
};

struct Vs_Out
{
    float4 pixel_p : SV_POSITION;
    float2 tex_uv : TEX_UV; // reused as .x = radius, .y = border
    
    nointerpolation float4 color1       : COLOR1;
    nointerpolation float4 color2       : COLOR2;
    nointerpolation float2 center_p     : CENTER_POS;
    nointerpolation float2 half_dim     : HALF_DIM;
    nointerpolation float radius        : RADIUS;
    nointerpolation uint type           : TYPE;
};

ByteAddressBuffer raw_buffer : register(t0);


float sdf_rect(float2 pixel_p, float2 origin_p, float2 dim, float radius)
{
    float2 pos = pixel_p - origin_p;
    float2 delta = abs(pos) - dim + radius;
    return length(max(delta, 0.)) + min(max(delta.x, delta.y), 0.0) - radius;
}

float4 decode_rgba(uint rgba)
{
    float4 color;
    color.r = float(rgba         & 0xff) / 255.;
    color.g = float((rgba >>  8) & 0xff) / 255.;
    color.b = float((rgba >> 16) & 0xff) / 255.;
    color.a = float((rgba >> 24)       ) / 255.;
    return color;
}




Vs_Out vs_main(Vs_In input)
{
    Vs_Out output;
    
    uint base_address = input.vertex_id & 0xffFFff; // 24 bits
    uint xy_corner = input.vertex_id >> 30;
    output.type = (input.vertex_id >> 24) & 0x3F; // 6 bits
    
    uint clip_rect_address = raw_buffer.Load(base_address);
    
    
    // load position
    float2 origin_p;
    origin_p.x = asfloat(raw_buffer.Load(base_address + 4));
    origin_p.y = asfloat(raw_buffer.Load(base_address + 8));
    
    float2 dim;
    dim.x = asfloat(raw_buffer.Load(base_address + 12));
    dim.y = asfloat(raw_buffer.Load(base_address + 16));
    
    output.half_dim = dim * 0.5;
    output.center_p = origin_p + output.half_dim;
    
    
    float2 pixel_dim = output.half_dim;
    if (output.type == 0) {
        pixel_dim += 1.; // add 1 pixel for sdf falloff border
    }
    
    output.pixel_p.x = output.center_p.x + (xy_corner & 1 ? pixel_dim.x : -pixel_dim.x);
    output.pixel_p.y = output.center_p.y + (xy_corner & 2 ? pixel_dim.y : -pixel_dim.y);
    output.pixel_p.z = 0.;
    output.pixel_p.w = 1.;
    
    
    
    // clipping
    float4 clip_rect = asfloat(raw_buffer.Load4(clip_rect_address));
    float2 move_uv_by = 0.;
    
    if (output.pixel_p.x < clip_rect.x) { // x min
        move_uv_by.x = clip_rect.x - output.pixel_p.x;
        output.pixel_p.x = clip_rect.x;
    }
    if (output.pixel_p.x > clip_rect.z) { // x max
        dim.x -= output.pixel_p.x - clip_rect.z;
        output.pixel_p.x = clip_rect.z;
    }
    if (output.pixel_p.y < clip_rect.y) { // y min
        move_uv_by.y = clip_rect.y - output.pixel_p.y;
        output.pixel_p.y = clip_rect.y;
    }
    if (output.pixel_p.y > clip_rect.w) { // y max
        dim.y -= output.pixel_p.y - clip_rect.w;
        output.pixel_p.y = clip_rect.w;
    }
    
    
    
    // a little bit messy data loading for type specific thing
    output.tex_uv = asfloat(raw_buffer.Load2(base_address + 20)); // load 20 + 24
    output.color1 = decode_rgba(raw_buffer.Load(base_address + 28));
    
    if (output.type == 0) // textbox background
    {
        output.tex_uv *= output.half_dim.y;
        output.color2 = decode_rgba(raw_buffer.Load(base_address + 32));
    }
    else // textured glyph quad
    {
        if (xy_corner & 1) {
            output.tex_uv.x += dim.x;
        }
        if (xy_corner & 2) {
            output.tex_uv.y += dim.y;
        }
        
        output.tex_uv += move_uv_by;
        output.tex_uv *= atlas_dim_inv;
    }
    
    
    
    output.pixel_p.xy *= screen_dim_inv*2.;
    output.pixel_p.xy -= 1.;
    output.pixel_p.y *= -1.;
    return output;
}




Texture2D    glyph_texture : register(t0);
SamplerState glyph_sampler : register(s0);

float4 ps_main(Vs_Out input) : SV_TARGET
{
    float4 color;
    
    if (input.type == 0)
    {
        float radius = input.tex_uv.x;
        float border = -input.tex_uv.y;
        
        float dist = sdf_rect(input.pixel_p.xy, input.center_p, input.half_dim, radius);
        
        float in_shape = 1. - clamp(dist, 0., 1.);
        float in_inner = smoothstep(border - 1., border, dist);
        
        color = lerp(input.color1, input.color2, in_inner);
        color.a *= in_shape;
        
        float texture_alpha = glyph_texture.Sample(glyph_sampler, (input.pixel_p.xy + anim_t*150.) * float2(0.0004, 0.01)).a;
        color.rgb *= (0.5f + 0.5f*texture_alpha);
    }
    else
    {
        float texture_alpha = glyph_texture.Sample(glyph_sampler, input.tex_uv).a;
        color = input.color1;
        color.a *= texture_alpha;
    }
    
    return color;
}

