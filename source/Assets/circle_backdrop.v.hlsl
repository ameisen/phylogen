cbuffer RenderState
{
   float4x4 Projection;
   float4x4 View;
   float4x4 VP;
   float4   Time; // Absolute, Normalized, Normalized to 100
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

struct VInput
{
   float2 Position : POSITION;
   uint InstanceID : SV_InstanceID;
};

struct PInput
{
   float4 Position : SV_POSITION;
   sample float2 VertexPosition : TEXCOORD0;
   uint InstanceID : TEXCOORD1;
   sample float4 SV_Position : TEXCOORD2;
};

PInput main(VInput input)
{
   PInput output;

   _InstanceData ThisInstanceData = InstanceData[input.InstanceID];

   const float targetRadius = ThisInstanceData.Radius * Scale_ColorLerp_MoveForward.x;

   output.VertexPosition = input.Position / 1.2;

   float4x4 TransposeTransform = transpose(ThisInstanceData.Transform);
   float4x4 MVP = mul(TransposeTransform, VP);

   //float2 forward = TransposeTransform[0].xy * (Scale_ColorLerp_MoveForward.w * targetRadius);

   //output.Position = mul(float4(((input.Position + forward) / 1.2) * targetRadius, 0.0, 1.0), MVP);
   output.Position = mul(float4(((input.Position) / 1.2) * targetRadius, 0.0, 1.0), MVP);

   output.Position.z = 0.5;

   output.InstanceID = input.InstanceID;

   output.SV_Position = float4(input.Position, 0.0, 1.0);

   return output;
}
