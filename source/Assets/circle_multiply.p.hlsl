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

   float dotComp = lerp(0.83, 1.0, thisInstanceData.Armor_NucleusScale.x);

   if (dot(input.VertexPosition, input.VertexPosition) > dotComp)
   {
      discard;
   }

   float4 Out = lerp(InstanceData[input.InstanceID].Color1, InstanceData[input.InstanceID].Color2, Scale_ColorLerp_MoveForward.y) * Color;

   if (WorldRadius_Lit_NuclearScaleLerp.y > 0.0)
   {
      float2 posW = mul(input.SV_Position, transpose(thisInstanceData.Transform)).xy;
      float2 uv = posW / WorldRadius_Lit_NuclearScaleLerp.x;
      uv += 1.0;
      uv *= 0.5;

      float waste = WasteTexture.Sample(WasteTextureSampler, uv).r;
      float3 wasteColor = float3(0.8, 0.52, 0.25);
      Out.rgb = saturate(lerp(Out.rgb, (Out.rgb * wasteColor) + (wasteColor * 0.25), waste));

      float light = LightTexture.Sample(LightTextureSampler, uv).r;
      light *= 0.8;
      light += 0.2;
      light = sqrt(light);
      Out.rgb *= light;
   }

   // Perhaps add in a little random noise, maybe? 3:
   float2 noiseUV = input.SV_Position.xy * 0.1;
   float noise = LightTexture.Sample(LightTextureSampler, noiseUV).r;
   Out.rgb = lerp(Out.rgb, Out.rgb * noise, 0.5);

   return float4(pow(Out.rgb, 1.0 / 2.2), Out.a);
}
