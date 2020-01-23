cbuffer RenderState
{
   float4x4 Projection;
   float4x4 View;
   float4x4 VP;
   float4   Time; // Absolute, Normalized, Normalized to 10, Normalized to 100
   float4   Color;
   float4   Scale_ColorLerp_MoveForward;
   float4   WorldRadius_Lit_NuclearScaleLerp;
}

struct _InstanceData
{
   float4x4 Transform;
   float4 Color1;
   float4 Color2;
   float4 TrendVelocity;
   float Radius;
   float TimeOffset;
   float T1;
   float T2;
   float4 Armor_NucleusScale;
};

StructuredBuffer<_InstanceData> InstanceData : register(t0);
Texture2D                       LightTexture : register(t1);
Texture2D                       WasteTexture : register(t2);

SamplerState LightTextureSampler : register(s1);
SamplerState WasteTextureSampler : register(s2);

struct PInput
{
   float4 Position : SV_POSITION;
   sample float2 VertexPosition : TEXCOORD0;
   uint InstanceID : TEXCOORD1;
   sample float4 SV_Position : TEXCOORD2;
};

float4 main(PInput input) : SV_TARGET0
{
   // Out of boredom...

   _InstanceData thisInstanceData = InstanceData[input.InstanceID];


   if (dot(input.VertexPosition, input.VertexPosition) > 1.0)
   {
      discard;
   }

   float2 uv = (input.VertexPosition + 1.0) / 2.0;

   float4 Out = Color;// *0.25;

   float waste = WasteTexture.Sample(WasteTextureSampler, uv).r;
   float3 wasteColor = float3(0.8, 0.52, 0.25);
   Out.rgb = saturate(lerp(Out.rgb, (Out.rgb * wasteColor) + (wasteColor * 0.25), waste));

   float3 greenColor = float3(0, 0.25, 0);
   Out.rgb = saturate(lerp(Out.rgb, (Out.rgb * greenColor) + (greenColor * 0.25), LightTexture.Sample(LightTextureSampler, uv).r));

   Out.rgb = lerp(Out.rgb * 0.5, Out.rgb, WorldRadius_Lit_NuclearScaleLerp.y);

   return float4(pow(Out.rgb, 1.0 / 2.2), Out.a);
}
