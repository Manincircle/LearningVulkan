// UBO ֻ��Ҫ View �� Proj ����
cbuffer SkyboxUBO : register(b0)
{
    matrix view;
    matrix proj;
};

// ������ɫ������ֻ��һ��λ��
struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0; // ���ݸ�PS�Ĳ�������
};

// ������ɫ��
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // �ؼ����ɣ��Ƴ� view �����ƽ�Ʋ���
    // �����������ʼ�������Ϊ���ģ������������
    matrix viewNoTranslation = view;
    viewNoTranslation[3][0] = 0; // x
    viewNoTranslation[3][1] = 0; // y
    viewNoTranslation[3][2] = 0; // z
    
    // Ӧ����ת��ͶӰ
    float4 pos = mul(float4(input.position, 1.0f), viewNoTranslation);
    output.position = mul(pos, proj);

    // �� z ����ǿ����Ϊ w ������
    // ���ʹ��������ֵ�ھ���͸�ӳ�����ʼ��Ϊ 1.0 (��Զ)
    // ȷ����պ�������������������ĺ���
    output.position.z = output.position.w;

    // ������λ����Ϊ�������򴫵ݸ�Ƭ����ɫ��
    output.texcoord = input.position;
    
    return output;
}

// ��������ͼ�Ͳ�����
TextureCube skyboxTexture : register(t1); // <-- ʹ�� TextureCube ����
SamplerState skyboxSampler : register(s1);

// Ƭ����ɫ��
float4 PSMain(VSOutput input) : SV_TARGET
{
    // ʹ�ô�VS���ݹ����ķ�������������ͼ�ϲ���
    return skyboxTexture.Sample(skyboxSampler, normalize(input.texcoord));
}