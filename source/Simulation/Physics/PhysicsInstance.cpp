#include "phylogen.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;

void Physics::Instance::unserialize(Stream &inStream, Cell *cell) {
	inStream.read(m_Position);
	inStream.read(m_Direction);
	inStream.read(m_Velocity);
	inStream.read(m_Radius);
	inStream.read(m_TouchedThisFrame);
	inStream.read(m_Valid);
}

void Physics::Instance::serialize(Stream &outStream) const {
	outStream.write(m_Position);
	outStream.write(m_Direction);
	outStream.write(m_Velocity);
	outStream.write(m_Radius);
	outStream.write(m_TouchedThisFrame);
	outStream.write(m_Valid);
}
