#include "phylogen.hpp"
#include "VMInstructions_Basic.hpp"

using namespace phylo;
using namespace phylo::VM;
using namespace phylo::VM::Basic;
using namespace phylo::VM::Basic::Instructions;

uint64 Copy::execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister)
{
	returnRegister = sourceRegister;

	return 0ull;
}

uint64 Load::execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister)
{
	const auto srcIndex = uint16(sourceRegister) % Instance::NumRegisters;

	returnRegister = instance.m_Registers[srcIndex];

	return 0ull;
}

uint64 Store::execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister)
{
	const auto dstIndex = uint16(returnRegister) % Instance::NumRegisters;

	instance.m_Registers[dstIndex] = sourceRegister;

	return 0ull;
}

uint64 LoadStore::execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister)
{
	const auto srcIndex = uint16(sourceRegister) % Instance::NumRegisters;
	const auto dstIndex = uint16(returnRegister) % Instance::NumRegisters;

	instance.m_Registers[dstIndex] = instance.m_Registers[dstIndex];

	return 0ull;
}

uint64 Jump::execute(Instance & __restrict instance, Register &returnRegister, int16 distance)
{
	instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
	
	returnRegister = 1_u16;
	return 0ull;
}

uint64 Jump_Z::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value == 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Jump_NZ::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value != 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Jump_GZ::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value > 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Jump_LZ::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value < 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Jump_GEZ::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value >= 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Jump_LEZ::execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value)
{
	if (value <= 0)
	{
		instance.m_ProgramCounter = (instance.m_ProgramCounter + distance) % instance.m_ByteCode.size();
		returnRegister = 1_u16;
	}
	else
	{
		returnRegister = 0_u16;
	}

	return 0ull;
}

uint64 Add_Integer::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = uint16( operand1 + operand2 );

	return 0ull;
}

uint64 Subtract_Integer::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = uint16( operand1 - operand2 );

	return 0ull;
}

uint64 Multiply_Integer::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = uint16( operand1 * operand2 );

	return 0ull;
}

uint64 Divide_Integer::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	if (operand2 == 0)
	{
		returnRegister = 0_u16;
	}
	else
	{
		returnRegister = uint16( operand1 / operand2 );
	}

	return 0ull;
}

uint64 Modulo_Integer::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	if (operand2 == 0)
	{
		returnRegister = 0_u16;
	}
	else
	{
		returnRegister = uint16( operand1 % operand2 );
	}

	return 0ull;
}

uint64 Add_Float::execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2)
{
	returnRegister = saturate(operand1 + operand2);

	return 0ull;
}

uint64 Subtract_Float::execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2)
{
	returnRegister = saturate(operand1 - operand2);

	return 0ull;
}

uint64 Multiply_Float::execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2)
{
	returnRegister = saturate(operand1 * operand2);

	return 0ull;
}

uint64 Divide_Float::execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2)
{
	if (operand2 == 0.0f)
	{
		returnRegister = 0.0f;
	}
	else
	{
		returnRegister = saturate(operand1 / operand2);
	}

	return 0ull;
}

uint64 Modulo_Float::execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2)
{
	if (operand2 == 0)
	{
		returnRegister = 0.0f;
	}
	else
	{
		returnRegister = saturate(xtd::mod(operand1, operand2));
	}

	return 0ull;
}

uint64 Logical_AND::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = (operand1 && operand2) ? 1_u16 : 0_u16;

	return 0ull;
}

uint64 Logical_NAND::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = (!(operand1 && operand2)) ? 1_u16 : 0_u16;

	return 0ull;
}

uint64 Logical_OR::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = (operand1 || operand2) ? 1_u16 : 0_u16;

	return 0ull;
}

uint64 Logical_NOR::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = (!(operand1 || operand2)) ? 1_u16 : 0_u16;

	return 0ull;
}

uint64 Logical_XOR::execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2)
{
	returnRegister = ((operand1 ? 1 : 0) ^ (operand2 ? 1 : 0)) ? 1_u16 : 0_u16;

	return 0ull;
}

uint64 Logical_NEGATE::execute(Instance & __restrict instance, Register &returnRegister, int16 operand)
{
	returnRegister = (operand) ? 0_u16 : 1_u16;

	return 0ull;
}
