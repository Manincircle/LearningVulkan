struct VSInput {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color    : COLOR;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position =float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

Texture2D myTexture : register(t1, space0);
SamplerState mySampler : register(s1, space0);

float4 PSMain(VSOutput input) : SV_TARGET {
    return float4(input.color, 1.0f);
}
