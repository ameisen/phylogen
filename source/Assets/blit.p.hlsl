struct PInput
{
   float4 Position : SV_POSITION;
};

float4 main(PInput input) : SV_TARGET0
{
   return float4(0.0, 1.0, 0.0, 1.0);
}
