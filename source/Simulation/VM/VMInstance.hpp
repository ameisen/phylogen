#pragma once

// #define ENABLE_TRANSLATION_TABLE 1

#include "VMControllerAlias.hpp"
#include "VMInstructions.hpp"

namespace phylo
{
	class Cell;
	namespace VM
	{
		struct Instance
		{
			class alignas(uint16) Register final : trait_simple
			{
				uint16 m_Value = 0;

			public:
				Register() = default;

				Register(const Register &val) : m_Value(val.m_Value) {}

				Register(uint16 val) : m_Value(val) {}

				Register(float val) : m_Value(uint16(val * 65535.5f))
				{
					xassert(val >= 0.0f && val <= 1.0f, "Register float assignment out of range");
				}

				Register &operator = (uint16 val) __restrict
				{
					m_Value = val;
					return *this;
				}
				Register &operator = (float val) __restrict
				{
					xassert(val >= 0.0f && val <= 1.0f, "Register float assignment out of range");
					m_Value = uint16(val * 65535.5f);
					return *this;
				}
				Register &operator = (Register val) __restrict
				{
					m_Value = val.m_Value;
					return *this;
				}
				operator const uint16() const __restrict
				{
					return m_Value;
				}
				operator const int16() const __restrict
				{
					return int16(m_Value);
				}
				operator const float() const __restrict
				{
					return float(m_Value) / 65535.0f;
				}
			};

			static constexpr uint16			NumRegisters = 32;

			array<Register, NumRegisters>	m_Registers; // 16-bit registers.
			Cell *m_Cell = nullptr;
			uint                            m_ProgramCounter = 0;

			Instance();

			union Operation
			{
				uint64         m_OperationValue;
				struct
				{
					uint64 OpCode : 14;
					uint64 Operand1Type : 1;
					uint64 Operand2Type : 1;
					uint64 ResultRegister : 16;
					uint64 Operand1 : 16;
					uint64 Operand2 : 16;
				};
			};

			using CounterType = array<uint32, NumOperations + 1>;

#define _VM_STRINGVIEWCASE(x) case x: return #x;

			static string_view getInstructionName(uint16 instruction)
			{
				switch (VM::Operation(instruction))
				{
					_VM_STRINGVIEWCASE(VM::Operation::NOP)
					_VM_STRINGVIEWCASE(VM::Operation::Copy)
					_VM_STRINGVIEWCASE(VM::Operation::Load)
					_VM_STRINGVIEWCASE(VM::Operation::Store)
					_VM_STRINGVIEWCASE(VM::Operation::Load_Store)
					_VM_STRINGVIEWCASE(VM::Operation::Add_Integer)
					_VM_STRINGVIEWCASE(VM::Operation::Subtract_Integer)
					_VM_STRINGVIEWCASE(VM::Operation::Multiply_Integer)
					_VM_STRINGVIEWCASE(VM::Operation::Divide_Integer)
					_VM_STRINGVIEWCASE(VM::Operation::Modulo_Integer)
					_VM_STRINGVIEWCASE(VM::Operation::Add_Float)
					_VM_STRINGVIEWCASE(VM::Operation::Subtract_Float)
					_VM_STRINGVIEWCASE(VM::Operation::Multiply_Float)
					_VM_STRINGVIEWCASE(VM::Operation::Divide_Float)
					_VM_STRINGVIEWCASE(VM::Operation::Modulo_Float)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalAND)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalNAND)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalOR)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalNOR)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalNEGATE)
					_VM_STRINGVIEWCASE(VM::Operation::LogicalXOR)
					_VM_STRINGVIEWCASE(VM::Operation::Jump)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_Z)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_NZ)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_GZ)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_LZ)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_GEZ)
					_VM_STRINGVIEWCASE(VM::Operation::Jump_LEZ)
					_VM_STRINGVIEWCASE(VM::Operation::Sleep)
					_VM_STRINGVIEWCASE(VM::Operation::Sleep_Touch)
					_VM_STRINGVIEWCASE(VM::Operation::Sleep_Attack)
					_VM_STRINGVIEWCASE(VM::Operation::Move)
					_VM_STRINGVIEWCASE(VM::Operation::Rotate)
					_VM_STRINGVIEWCASE(VM::Operation::Split)
					_VM_STRINGVIEWCASE(VM::Operation::Burn)
					_VM_STRINGVIEWCASE(VM::Operation::Suicide)
					_VM_STRINGVIEWCASE(VM::Operation::Color_Green)
					_VM_STRINGVIEWCASE(VM::Operation::Color_Red)
					_VM_STRINGVIEWCASE(VM::Operation::Color_Blue)
					_VM_STRINGVIEWCASE(VM::Operation::Grow)
					_VM_STRINGVIEWCASE(VM::Operation::GetEnergy)
					_VM_STRINGVIEWCASE(VM::Operation::GetLight_Green)
					_VM_STRINGVIEWCASE(VM::Operation::GetLight_Red)
					_VM_STRINGVIEWCASE(VM::Operation::GetWaste)
					_VM_STRINGVIEWCASE(VM::Operation::WasTouched)
					_VM_STRINGVIEWCASE(VM::Operation::WasAttacked)
					_VM_STRINGVIEWCASE(VM::Operation::See)
					_VM_STRINGVIEWCASE(VM::Operation::Size)
					_VM_STRINGVIEWCASE(VM::Operation::MySize)
					_VM_STRINGVIEWCASE(VM::Operation::Armor)
					_VM_STRINGVIEWCASE(VM::Operation::MyArmor)
					_VM_STRINGVIEWCASE(VM::Operation::Attack)
					_VM_STRINGVIEWCASE(VM::Operation::Transfer)
				}
				return "invalid";
			}

#define _VM_STRINGDESCCASE(x, y) case x: return #y;

			static string_view getInstructionDescription(uint16 instruction)
			{
				switch (VM::Operation(instruction))
				{
					_VM_STRINGDESCCASE(VM::Operation::NOP, "No Operation")
					_VM_STRINGDESCCASE(VM::Operation::Copy, "Copies a value from one register to another")
					_VM_STRINGDESCCASE(VM::Operation::Load, "Loads a register with a value from a dynamically-indexed source register")
					_VM_STRINGDESCCASE(VM::Operation::Store, "Stores a value from a source register into a dynamically-indexed destination register")
					_VM_STRINGDESCCASE(VM::Operation::Load_Store, "Copies a value from one dynamically-indexed register to another")
					_VM_STRINGDESCCASE(VM::Operation::Add_Integer, "Adds to a register")
					_VM_STRINGDESCCASE(VM::Operation::Subtract_Integer, "Subtracts from a register")
					_VM_STRINGDESCCASE(VM::Operation::Multiply_Integer, "Multiplies on a register")
					_VM_STRINGDESCCASE(VM::Operation::Divide_Integer, "Divides on a register")
					_VM_STRINGDESCCASE(VM::Operation::Modulo_Integer, "Modulos on a register")
					_VM_STRINGDESCCASE(VM::Operation::Add_Float, "Adds to a register, floating point")
					_VM_STRINGDESCCASE(VM::Operation::Subtract_Float, "Subtracts from a register, floating point")
					_VM_STRINGDESCCASE(VM::Operation::Multiply_Float, "Multiplies on a register, floating point")
					_VM_STRINGDESCCASE(VM::Operation::Divide_Float, "Divides on a register, floating point")
					_VM_STRINGDESCCASE(VM::Operation::Modulo_Float, "Modulos on a register, floating point")
					_VM_STRINGDESCCASE(VM::Operation::LogicalAND, "Logical AND")
					_VM_STRINGDESCCASE(VM::Operation::LogicalNAND, "Logical NAND")
					_VM_STRINGDESCCASE(VM::Operation::LogicalOR, "Logical OR")
					_VM_STRINGDESCCASE(VM::Operation::LogicalNOR, "Logical NOR")
					_VM_STRINGDESCCASE(VM::Operation::LogicalNEGATE, "Logical NEGATE")
					_VM_STRINGDESCCASE(VM::Operation::LogicalXOR, "Logical XOR")
					_VM_STRINGDESCCASE(VM::Operation::Jump, "Jump")
					_VM_STRINGDESCCASE(VM::Operation::Jump_Z, "Jump if Zero")
					_VM_STRINGDESCCASE(VM::Operation::Jump_NZ, "Jump if Not Zero")
					_VM_STRINGDESCCASE(VM::Operation::Jump_GZ, "Jump if Greater than Zero")
					_VM_STRINGDESCCASE(VM::Operation::Jump_LZ, "Jump if Less than Zero")
					_VM_STRINGDESCCASE(VM::Operation::Jump_GEZ, "Jump if Greater than or Equal to Zero")
					_VM_STRINGDESCCASE(VM::Operation::Jump_LEZ, "Jump if Less than or Equal to Zero")
					_VM_STRINGDESCCASE(VM::Operation::Sleep, "Sleep")
					_VM_STRINGDESCCASE(VM::Operation::Sleep_Touch, "Sleep until touched")
					_VM_STRINGDESCCASE(VM::Operation::Sleep_Attack, "Sleep until attacked")
					_VM_STRINGDESCCASE(VM::Operation::Move, "Accelerate Cell")
					_VM_STRINGDESCCASE(VM::Operation::Rotate, "Rotate Cell")
					_VM_STRINGDESCCASE(VM::Operation::Split, "Split Cell")
					_VM_STRINGDESCCASE(VM::Operation::Burn, "Burn stored energy")
					_VM_STRINGDESCCASE(VM::Operation::Suicide, "Commit Suicide")
					_VM_STRINGDESCCASE(VM::Operation::Color_Green, "Increase Green Coloration")
					_VM_STRINGDESCCASE(VM::Operation::Color_Red, "Increase Red Coloration")
					_VM_STRINGDESCCASE(VM::Operation::Color_Blue, "Increase Blue Coloration")
					_VM_STRINGDESCCASE(VM::Operation::Grow, "Embiggen Cell")
					_VM_STRINGDESCCASE(VM::Operation::GetEnergy, "Store cell's energy in register")
					_VM_STRINGDESCCASE(VM::Operation::GetLight_Green, "Get light value of environment for Green")
					_VM_STRINGDESCCASE(VM::Operation::GetLight_Red, "Get light value of environment for Red")
					_VM_STRINGDESCCASE(VM::Operation::GetWaste, "Get waste value of environment for Blue")
					_VM_STRINGDESCCASE(VM::Operation::WasTouched, "Return if cell has been touched")
					_VM_STRINGDESCCASE(VM::Operation::WasAttacked, "Return if cell has been attacked")
					_VM_STRINGDESCCASE(VM::Operation::See, "Detect if there is a cell in front of cell")
					_VM_STRINGDESCCASE(VM::Operation::Size, "Detect size of cell in front of cell")
					_VM_STRINGDESCCASE(VM::Operation::MySize, "Store cell's own size in register")
					_VM_STRINGDESCCASE(VM::Operation::Armor, "Detect armor of cell in front of cell")
					_VM_STRINGDESCCASE(VM::Operation::MyArmor, "Store cell's own armor in register")
					_VM_STRINGDESCCASE(VM::Operation::Attack, "Attack cell in front of cell")
					_VM_STRINGDESCCASE(VM::Operation::Transfer, "Copy bytecode into other cell's bytecode")
				}
				return "an invalid opcode";
			}

#if ENABLE_TRANSLATION_TABLE
			uint8 OpTranslationTable[256];
#endif

			enum class SleepState
			{
				None = 0,
				Touched,
				Attacked
			} m_SleepState = SleepState::None;
			uint64                   m_SleepCount = 0;
			array<uint64>            m_ByteCode; // This is aligned to 64 bits. Actual size is below. It will always be at least 8 bytes.

			void generate_bytecode_hash();

			void set_bytecode(const array_view<uint64> &bytecode)
			{
				xassert(m_ProgramCounter == 0, "Cannot set the bytecode of an active VM");
				m_ByteCode = bytecode;
				generate_bytecode_hash();
			}
			void set_bytecode(array_view<uint64> &&bytecode)
			{
				xassert(m_ProgramCounter == 0, "Cannot set the bytecode of an active VM");
				m_ByteCode = bytecode;
				generate_bytecode_hash();
			}
			void set_bytecode_live(array_view<uint64> &&bytecode)
			{
				m_ByteCode = bytecode;
				m_ProgramCounter %= m_ByteCode.size();
				generate_bytecode_hash();
			}
			void tick(Controller *controller, typename CounterType &counter);

			void mutate();

			// VM instructions
			uint64 op_Sleep(Register &resultRegister, uint16 ticks);
			uint64 op_SleepTouched(Register &resultRegister, Controller *controller);
			uint64 op_SleepAttacked(Register &resultRegister, Controller *controller);
			uint64 op_Copy(Register &resultRegister, const Register &reg);
			uint64 op_Load(Register &resultRegister, const Register &reg);
			uint64 op_Store(Register &resultRegister, const Register &reg);
			uint64 op_LoadStore(Register &resultRegister, const Register &reg);

			uint64 op_Add(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_Subtract(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_Multiply(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_Divide(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_Modulo(Register &resultRegister, int16 oper1, int16 oper2);

			uint64 op_AddF(Register &resultRegister, float oper1, float oper2);
			uint64 op_SubtractF(Register &resultRegister, float oper1, float oper2);
			uint64 op_MultiplyF(Register &resultRegister, float oper1, float oper2);
			uint64 op_DivideF(Register &resultRegister, float oper1, float oper2);
			uint64 op_ModuloF(Register &resultRegister, float oper1, float oper2);

			uint64 op_LAND(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_LNAND(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_LOR(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_LNOR(Register &resultRegister, int16 oper1, int16 oper2);
			uint64 op_LNEGATE(Register &resultRegister, int16 oper);
			uint64 op_LXOR(Register &resultRegister, int16 oper1, int16 oper2);

			uint64 op_Move(Register &resultRegister, float amount);
			uint64 op_Rotate(Register &resultRegister, float amount);
			uint64 op_Jump(Register &resultRegister, int16 amount);
			uint64 op_JumpZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_JumpNZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_JumpGZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_JumpLZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_JumpGEZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_JumpLEZ(Register &resultRegister, int16 amount, int16 value);
			uint64 op_Split(Register &resultRegister, Controller *controller);
			uint64 op_Burn(Register &resultRegister, Controller *controller);
			uint64 op_Suicide(Register &resultRegister, Controller *controller);
			uint64 op_ColorGreen(Register &resultRegister, Controller *controller);
			uint64 op_ColorRed(Register &resultRegister, Controller *controller);
			uint64 op_ColorBlue(Register &resultRegister, Controller *controller);
			uint64 op_Grow(Register &resultRegister, float value);
			uint64 op_GetEnergy(Register &resultRegister, Controller *controller);
			uint64 op_GetLightGreen(Register &resultRegister, Controller *controller);
			uint64 op_GetLightRed(Register &resultRegister, Controller *controller);
			uint64 op_GetWaste(Register &resultRegister, Controller *controller);

			uint64 op_WasTouched(Register &resultRegister, Controller *controller);
			uint64 op_WasAttacked(Register &resultRegister, Controller *controller);
			uint64 op_See(Register &resultRegister, Controller *controller);
			uint64 op_Size(Register &resultRegister, Controller *controller);
			uint64 op_MySize(Register &resultRegister, Controller *controller);
			uint64 op_Armor(Register &resultRegister, Controller *controller);
			uint64 op_MyArmor(Register &resultRegister, Controller *controller);

			uint64 op_Attack(Register &resultRegister, Controller *controller);

			uint64 op_Transfer(Register &resultRegister, Controller *controller, int16 oper1, int16 oper2);

			enum class OperandType
			{
				Value,
				Register
			};

			template <typename A, typename B>
			static uint64 OperationBuilder(VM::Operation operation, uint16 result, OperandType type1, A oper1, OperandType type2, B oper2)
			{
				Operation opcode;
				opcode.OpCode = uint64(operation);
				opcode.Operand1Type = (type1 == OperandType::Register) ? 1 : 0;
				opcode.Operand2Type = (type2 == OperandType::Register) ? 1 : 0;
				opcode.ResultRegister = result;
				opcode.Operand1 = uint16(Register(oper1));
				opcode.Operand2 = uint16(Register(oper2));

				return opcode.m_OperationValue;
			}

			template <typename A>
			static uint64 OperationBuilder(VM::Operation operation, uint16 result, OperandType type1, A oper1)
			{
				Operation opcode;
				opcode.OpCode = uint64(operation);
				opcode.Operand1Type = (type1 == OperandType::Register) ? 1 : 0;
				opcode.Operand2Type = 0;
				opcode.ResultRegister = result;
				opcode.Operand1 = uint16(Register(oper1));
				opcode.Operand2 = 0;

				return opcode.m_OperationValue;
			}

			static uint64 OperationBuilder(VM::Operation operation, uint16 result)
			{
				Operation opcode;
				opcode.OpCode = uint64(operation);
				opcode.Operand1Type = 0;
				opcode.Operand2Type = 0;
				opcode.ResultRegister = result;
				opcode.Operand1 = 0;
				opcode.Operand2 = 0;

				return opcode.m_OperationValue;
			}

			void unserialize(Stream &inStream, Cell *cell);
			void serialize(Stream &outStream) const;
		};
	}
}
