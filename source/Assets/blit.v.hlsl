struct VInput
{
   float4 Position : POSITION;
};

struct PInput
{
   float4 Position : SV_POSITION;
};

PInput main(VInput input)
{
   PInput output;

   output.Position = input.Position;

   return output;
}
