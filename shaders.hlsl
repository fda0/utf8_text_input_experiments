
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
    float2 radius_or_uv : RADIUS_OR_UV;
    
    nointerpolation float4 color1       : COLOR1;
    nointerpolation float4 color2       : COLOR2;
    nointerpolation float2 center_p     : CENTER_POS;
    nointerpolation float2 half_dim     : HALF_DIM;
    nointerpolation uint type           : TYPE;
};

ByteAddressBuffer raw_buffer : register(t0);


float sdf_rect(float2 pixel_p, float2 origin_p, float2 dim, float radius)
{
    float2 pos = pixel_p - origin_p;
    float2 delta = abs(pos) - dim + radius;
    return length(max(delta, 0.)) + min(max(delta.x, delta.y), 0.0) - radius;
}

float sdf_circle(float2 pixel_p, float2 origin_p, float radius)
{
    float2 pos = pixel_p - origin_p;
    return length(pos) - radius;
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
    
    float2 origin_p;
    origin_p.x = asfloat(raw_buffer.Load(base_address));
    origin_p.y = asfloat(raw_buffer.Load(base_address + 4));
    
    float2 dim;
    dim.x = asfloat(raw_buffer.Load(base_address + 8));
    dim.y = asfloat(raw_buffer.Load(base_address + 12));
    
    output.half_dim = dim * 0.5;
    output.center_p = origin_p + output.half_dim;
    
    
    float2 pixel_dim = output.half_dim;
    if (output.type == 0) {
        pixel_dim += 1.;
    }
    
    output.pixel_p.x = output.center_p.x + (xy_corner & 1 ? pixel_dim.x : -pixel_dim.x);
    output.pixel_p.y = output.center_p.y + (xy_corner & 2 ? pixel_dim.y : -pixel_dim.y);
    output.pixel_p.z = 0.;
    output.pixel_p.w = 1.;
    
    
    output.radius_or_uv.x = asfloat(raw_buffer.Load(base_address + 16));
    
    if (output.type == 0)
    {
        uint rgba1 = raw_buffer.Load(base_address + 20);
        output.color1 = decode_rgba(rgba1);
    }
    else
    {
        output.radius_or_uv.y = asfloat(raw_buffer.Load(base_address + 20));
        
        if (xy_corner & 1) {
            output.radius_or_uv.x += dim.x;
        }
        if (xy_corner & 2) {
            output.radius_or_uv.y += dim.y;
        }
        
        output.radius_or_uv *= atlas_dim_inv;
    }
    
    
    uint rgba2 = raw_buffer.Load(base_address + 24);
    output.color2 = decode_rgba(rgba2);
    
    
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
        float radius = input.radius_or_uv.x;
#if 0
        float dist = sdf_circle(input.pixel_p.xy, input.center_p, radius);
#else
        float dist = sdf_rect(input.pixel_p.xy, input.center_p, input.half_dim, radius);
#endif
        
        //float in_shape = smoothstep(1., 0., dist);
        float in_shape = 1. - clamp(dist, 0., 1.);
        
        float border = input.half_dim.x * -0.01; // @todo expose as parameter to the textbox? use half_dim.y?
        float in_inner = smoothstep(border, border + 2., dist);
        
        color = lerp(input.color1, input.color2, in_inner);
        color.a *= in_shape;
        
        float texture_alpha = glyph_texture.Sample(glyph_sampler, (input.pixel_p.xy + anim_t*150.) * float2(0.0004, 0.01)).a;
        color.rgb *= (0.5f + 0.5f*texture_alpha);
    }
    else
    {
        float texture_alpha = glyph_texture.Sample(glyph_sampler, input.radius_or_uv).a;
        color = input.color2;
        color.a *= texture_alpha;
    }
    
    return color;
}

