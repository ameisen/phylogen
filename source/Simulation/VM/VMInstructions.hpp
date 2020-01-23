#pragma once

namespace phylo::VM
{
	enum class Operation : uint16
	{
		NOP = 0,    // No operation. Don't do anything. Rather expensive.
		Copy,       // Stores value into destination register.
		Load,       // Treats input as the index of a register to read and store into destination register.
		Store,      // Treats destination register as index of register which is storing the index of the destination register. Stores value within indirect destination.
		Load_Store,  // Stores indirect value into indirect destination.

		Add_Integer,        // Add two integers.
		Subtract_Integer,   // Subtract two integers.
		Multiply_Integer,   // Multiply two integers.
		Divide_Integer,     // Divide two integers.
		Modulo_Integer,     // Get remainder of division of two integers.

		Add_Float,       // Add two floats.
		Subtract_Float,  // Subtract two floats.
		Multiply_Float,  // Multiply two floats.
		Divide_Float,    // Divide two floats.
		Modulo_Float,    // Get the remainder of division of two floats.

		LogicalAND,       // Logical AND
		LogicalNAND,      // Logical NAND
		LogicalOR,        // Logical OR
		LogicalNOR,       // Logical NOR
		LogicalNEGATE,    // Logical Negate
		LogicalXOR,       // Logical XOR

		// TODO bitwise
		// TODO cast double->int, int->double
		// TODO comparisons

		Jump,       // Logical jump in bytecode.  
		Jump_Z,      // Logical jump in bytecode if second operand is 0.
		Jump_NZ,     // Logical jump in bytecode if second operand is not 0.
		Jump_GZ,     // Logical jump in bytecode if second operand is > 0.
		Jump_LZ,     // Logical jump in bytecode if second operand is < 0.
		Jump_GEZ,    // Logical jump in bytecode if second operand is >= 0.
		Jump_LEZ,    // Logical jump in bytecode if second operand is <= 0.
		Sleep,      // Sleep for a given number of ticks. Efficient way to wait.
		Sleep_Touch, // Sleep until touched.
		Sleep_Attack,   // Sleep until attacked.
		Move,       // Move.
		Rotate,     // Rotate.
		Split,      // Instruct the cell to divide (very lossy, inefficient, but only way to reproduce.)
		Burn,       // Burn energy for no benefit to the cell. Always burns a fixed percentage of energy.
		Suicide,    // The cell kills itself
		Color_Green,   // Increase chemical quantity of Chlorophyl A
		Color_Red,   // Increase chemical quantity of Chlorophyl A
		Color_Blue,    // Increase chemical quantity of Cytosil
		Grow,       // Causes the cell to try to grow.
		GetEnergy,  // Gets the energy of the cell as a function of capacity, from 0 to 1
		GetLight_Green,  // Get the light intensity of Green light
		GetLight_Red,  // Get the light intensity of Red light
		GetWaste,  // Get the light intensity of Blue waste

		WasTouched, // Were we touched? Gets count.
		WasAttacked,// Were we attacked? Gets count.
		See,        // Is there something in front of me in attacking distance?
		Size,       // How large is the object in front of me?
		MySize,     // How large am I?
		Armor,      // Target armor
		MyArmor,    // My armor

		Attack,     // Attack and potentially consume another cell.

		Transfer,     // Insert bytecode into other cell.

		MaximumCount,
	};

	static constexpr usize NumOperations = usize(Operation::MaximumCount);
}
