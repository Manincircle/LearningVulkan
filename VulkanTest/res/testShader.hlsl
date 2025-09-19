cbuffer UBO : register(b0,space0)
{
    matrix model;
    matrix view;
    matrix proj;
    float3 lightDir; // ����ռ��еĹ��շ���
    float _pad0; // ���
    float3 lightColor; // �����ɫ
    float lightIntensity; // ���ǿ��
    float3 camPos; // ����ռ��е����λ��
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
    
    // ��������ռ�λ��
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

// --- PBR �������� (Cook-Torrance BRDF) ---
#define PI 3.14159265359

// D - ��̬�ֲ����� (Trowbridge-Reitz GGX)
// ������΢�۱��淨�ߵĳ���ֲ����
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

// G - �����ڱκ��� (Schlick-GGX)
// ������΢�۱������ڱε����ԣ����߱�΢�۱�������İ�͹�ڵ���
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

// F - ���������� (Schlick ����)
// �������ڲ�ͬ�Ƕ��£����淴�������ռ�ı���
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


// --- Ƭ����ɫ�������� ---
float4 PSMain(VSOutput input) : SV_TARGET
{
    // --- 1. ����ͼ�л�ȡ�������� ---
    float3 albedo = albedoMap.Sample(pbrSampler, input.texcoord).rgb;
    float metallic = mraMap.Sample(pbrSampler3, input.texcoord).r;
    float roughness = mraMap.Sample(pbrSampler3, input.texcoord).g;
    float ao = mraMap.Sample(pbrSampler3, input.texcoord).b;
    float3 tangentNormal = normalMap.Sample(pbrSampler1, input.texcoord).xyz * 2.0 - 1.0;
    
    // --- 2. ׼�����ռ������������ ---
    float3 N = normalize(mul(tangentNormal,input.TBN)); // ���淨��
    float3 V = normalize(camPos - input.worldPos); // �۲췽��
    float3 L = normalize(lightDir); // ��Դ����
    float3 H = normalize(V + L); // �������

    // --- 3. �������� (Radiance) ---
    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = lightColor * lightIntensity * NdotL;

    // --- 4. Cook-Torrance BRDF ���� ---
    
    // ���� F0 (0������ǵķ�����������)
    // ���ڷǽ�����F0 ͨ���� 0.04�����ڽ�����F0 ���������ɫ��
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // ���� D, G, F ������
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // ����߹ⲿ�� (specular BRDF)
    float3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001; // �� 0.0001 ��ֹ����0
    float3 specular = numerator / denominator;

    // --- 5. ���������ɫ ---
    
    // kS �Ǹ߹ⷴ�䲿�֣�kD �������䲿��
    // ���������غ㶨�ɣ�����֮��ӦС�ڵ���1
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic); // ������û��������

    // �����䲿�� (Lambertian)
    float3 diffuse = kD * albedo / PI;
    
    // ���չ��� = (������ + �߹�) * ����� * �������ڱ�
    // ע�⣺����һ���򻯵�ֱ�ӹ���ģ�ͣ�û�а���������/IBL
    float3 lighting = (diffuse + specular) * Lo * ao;
    
    float3 color = lighting / (lighting + float3(1.0, 1.0, 1.0));

    float4 emissive = emissiveMap.Sample(pbrSampler2, input.texcoord);
    
    return float4(color, 1.0)+emissive;
}