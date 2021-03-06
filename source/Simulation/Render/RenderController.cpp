#include "phylogen.hpp"
#include "RenderController.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;
using namespace phylo::Render;

// until we have a real wide_array implementation, we need to presize it.
static constexpr usize WideArraySize = 5000000ull;

Controller::Controller(Simulation &simulation) : m_Simulation(simulation)
{
}

Controller::~Controller()
{
}

void Controller::update() 
{
}
