cbuffer MyUniformBuffer : register(b0, space0)   // spaceN = descriptor set N, bM = binding M
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct VSInput {
    float3 position : POSITION;
    float3 color    : COLOR;
    float2 texCoord : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color    : COLOR;
    float2 texCoord : TEXCOORD;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(proj, mul(view, mul(model, float4(input.position, 1.0f))));
    output.color = input.color;
    output.texCoord = input.texCoord;
    return output;
}

Texture2D myTexture : register(t1, space0);
SamplerState mySampler : register(s1, space0);

float4 PSMain(VSOutput input) : SV_TARGET {
    float4 color = myTexture.Sample(mySampler, input.texCoord);
    //float4 color = float4(input.texCoord,0.0f, 1.0f);
    return color;
}
