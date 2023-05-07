#include "Phylogen.hpp"
#include "Controller.hpp"

#include "Cell.hpp"

namespace phylo::ComponentControllerCommon {
	void update_cell_instance(Cell *cell, Physics::Instance *newInstance) {
		cell->update_instance(newInstance);
	}
	void update_cell_instance(Cell *cell, VM::Instance *newInstance) {
		cell->update_instance(newInstance);
	}
	void update_cell_instance(Cell *cell, Render::Instance *newInstance) {
		cell->update_instance(newInstance);
	}
}
