cbuffer UBO : register(b0,space0)
{
    matrix model;
    matrix view;
    matrix proj;
    float3 lightDir; // 世界空间中的光照方向
    float _pad0; // 填充
    float3 lightColor; // 光的颜色
    float lightIntensity; // 光的强度
    float3 camPos; // 世界空间中的相机位置
};
struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3x3 TBN : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    // 计算世界空间位置
    output.worldPos = mul(float4(input.position, 1.0f), model).xyz;
    float4 viewPos = mul(float4(output.worldPos, 1.0f), view);
    output.position = mul(viewPos, proj);
    
    
    output.texcoord = input.texcoord;
    output.TBN = float3x3(
        normalize(mul(float4(input.tangent, 0.0), model).xyz),
        normalize(mul(float4(input.bitangent, 0.0), model).xyz),
        normalize(mul(float4(input.normal, 0.0), model).xyz)
    );
    return output;
}
Texture2D albedoMap : register(t1);
Texture2D normalMap : register(t2);
Texture2D emissiveMap : register(t3);
Texture2D mraMap : register(t4);

SamplerState pbrSampler : register(s1);
SamplerState pbrSampler1 : register(s2);
SamplerState pbrSampler2 : register(s3);
SamplerState pbrSampler3 : register(s4);

// --- PBR 辅助函数 (Cook-Torrance BRDF) ---
#define PI 3.14159265359

// D - 正态分布函数 (Trowbridge-Reitz GGX)
// 描述了微观表面法线的朝向分布情况
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// G - 几何遮蔽函数 (Schlick-GGX)
// 描述了微观表面自遮蔽的属性（光线被微观表面自身的凹凸遮挡）
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// F - 菲涅尔方程 (Schlick 近似)
// 描述了在不同角度下，表面反射光线所占的比率
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


// --- 片段着色器主函数 ---
float4 PSMain(VSOutput input) : SV_TARGET
{
    // --- 1. 从贴图中获取表面属性 ---
    float3 albedo = albedoMap.Sample(pbrSampler, input.texcoord).rgb;
    float metallic = mraMap.Sample(pbrSampler3, input.texcoord).r;
    float roughness = mraMap.Sample(pbrSampler3, input.texcoord).g;
    float ao = mraMap.Sample(pbrSampler3, input.texcoord).b;
    float3 tangentNormal = normalMap.Sample(pbrSampler1, input.texcoord).xyz * 2.0 - 1.0;
    
    // --- 2. 准备光照计算所需的向量 ---
    float3 N = normalize(mul(tangentNormal,input.TBN)); // 表面法线
    float3 V = normalize(camPos - input.worldPos); // 观察方向
    float3 L = normalize(lightDir); // 光源方向
    float3 H = normalize(V + L); // 半程向量

    // --- 3. 计算辐射度 (Radiance) ---
    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = lightColor * lightIntensity * NdotL;

    // --- 4. Cook-Torrance BRDF 计算 ---
    
    // 计算 F0 (0度入射角的菲涅尔反射率)
    // 对于非金属，F0 通常是 0.04。对于金属，F0 是其基础颜色。
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // 计算 D, G, F 三个项
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // 计算高光部分 (specular BRDF)
    float3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001; // 加 0.0001 防止除以0
    float3 specular = numerator / denominator;

    // --- 5. 组合最终颜色 ---
    
    // kS 是高光反射部分，kD 是漫反射部分
    // 根据能量守恒定律，两者之和应小于等于1
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic); // 纯金属没有漫反射

    // 漫反射部分 (Lambertian)
    float3 diffuse = kD * albedo / PI;
    
    // 最终光照 = (漫反射 + 高光) * 辐射度 * 环境光遮蔽
    // 注意：这是一个简化的直接光照模型，没有包含环境光/IBL
    float3 lighting = (diffuse + specular) * Lo * ao;
    
    float3 color = lighting / (lighting + float3(1.0, 1.0, 1.0));

    float4 emissive = emissiveMap.Sample(pbrSampler2, input.texcoord);
    
    return float4(color, 1.0)+emissive;
}