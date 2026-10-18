cbuffer ColorGrade : register(b0, space3)
{
    float3 tint;
    float saturation;
    float4 water;
};

Texture2D world_texture : register(t0, space2);
SamplerState world_sampler : register(s0, space2);

struct FragmentInput {
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float4 main(FragmentInput input) : SV_Target
{
    float4 color = world_texture.Sample(world_sampler, input.uv);
    if (water.y > 0.0 && color.a > 0.5) {
        uint width, height;
        world_texture.GetDimensions(width, height);
        float2 dimensions = float2(width, height);
        float2 position = input.uv * dimensions - water.zw;
        float wave = sin(position.x * 0.055 + position.y * 0.11 + water.x * 1.3)
            + 0.5 * sin(position.x * -0.081 + position.y * 0.06 - water.x * 0.9);
        float2 offset = float2(wave, sin(position.x * 0.035 + water.x)) * water.y * 1.8;
        float4 refracted = world_texture.Sample(world_sampler, input.uv + offset / dimensions);
        float4 edge = world_texture.Sample(world_sampler, input.uv + float2(0, 2.0) / dimensions);
        float coverage = min(color.a, min(refracted.a, edge.a));
        color.rgb = lerp(color.rgb, refracted.rgb, coverage);
        float shimmer = wave * 0.045 * water.y * coverage;
        color.rgb = saturate(color.rgb * (1.0 + shimmer) + float3(0.025, 0.034, 0.038) * shimmer);
    }
    float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    return float4(lerp(luminance.xxx, color.rgb, saturation) * tint, 1.0);
}
