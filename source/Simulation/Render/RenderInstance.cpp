#include "phylogen.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;

void Render::Instance::unserialize(Stream &inStream, Cell *cell) 
{
   inStream.read(m_RenderData); // make sure to clobber CellPtr when reading.
   m_RenderData.CellPtr = cell;
}

void Render::Instance::serialize(Stream &outStream) const 
{
   outStream.write(m_RenderData); // make sure to clobber CellPtr when reading.
}
