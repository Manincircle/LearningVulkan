cbuffer MyUniformBuffer : register(b0, space0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct VSInput {
    float3 position : POSITION;
    float3 velocity : VELOCITY;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position =float4(input.position, 1.0f);
    output.position = mul(output.position, model);
    output.position = mul(output.position, view);
    output.position = mul(output.position, proj);
    output.color = float4(input.velocity, 1.0f);
    //output.color = float4(1.0f,1.0f,1.0f,1.0f);
    return output;
}

Texture2D myTexture : register(t1, space0);
SamplerState mySampler : register(s1, space0);

float4 PSMain(VSOutput input) : SV_TARGET {
    return float4(1.0f,0.0f,0.0f,1.0f);
}