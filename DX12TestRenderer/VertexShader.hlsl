struct VS_INPUT{
	float3 pos : POSITION;
	float4 color : COLOR;
};

struct VS_OUTPUT{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT i)
{
    // just pass vertex position straight through
	VS_OUTPUT o;
	o.position = float4(i.pos,1);
	o.color = i.color;
	return o;
}