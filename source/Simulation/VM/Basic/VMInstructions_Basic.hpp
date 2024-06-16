#pragma once

#include "../VMInstance.hpp"
#include "ThreadPool.hpp"
#include "Simulation/Controller.hpp"

namespace phylo::VM::Basic::Instructions
{
	using Register = VM::Instance::Register;

	class NoOperation final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister) { return 0ull; }
	};

	//static constexpr uint16 _REG1_R_REG2_R = (1_u16 << 14) | (1_u16 << 15);
	//static constexpr uint16 _REG1_V_REG2_R = (1_u16 << 15);
	//static constexpr uint16 _REG1_R_REG2_V = (1_u16 << 14);
	//static constexpr uint16 _REG1_V_REG2_V = 0_u16;

	class Copy final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister);
	};

	class Load final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister);
	};

	class Store final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister);
	};

	class LoadStore final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, const Register &sourceRegister);
	};

	class Jump final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance);
	};

	class Jump_Z final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Jump_NZ final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Jump_GZ final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Jump_LZ final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Jump_GEZ final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Jump_LEZ final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 distance, int16 value);
	};

	class Add_Integer final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Subtract_Integer final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Multiply_Integer final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Divide_Integer final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Modulo_Integer final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Add_Float final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2);
	};

	class Subtract_Float final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2);
	};

	class Multiply_Float final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2);
	};

	class Divide_Float final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2);
	};

	class Modulo_Float final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, float operand1, float operand2);
	};

	class Logical_AND final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Logical_NAND final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Logical_OR final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Logical_NOR final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Logical_XOR final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand1, int16 operand2);
	};

	class Logical_NEGATE final
	{
	public:
		static uint64 execute(Instance & __restrict instance, Register &returnRegister, int16 operand);
	};
}
