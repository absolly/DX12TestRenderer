struct VS_OUTPUT{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

float4 main(VS_OUTPUT i) : SV_TARGET
{
    // return vertex color
    return i.color;
}