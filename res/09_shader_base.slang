struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

//struct PushBlock {
//    float myValue;
    // 这里可以加更多变量
//}

[[vk::push_constant]]
float time;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    //float t = pc.myValue;
    float pulse = 0.5 + 0.5 * sin(time);
    return float4(vertIn.color * pulse, 1.0);
    //return float4(vertIn.color, 1.0);
}