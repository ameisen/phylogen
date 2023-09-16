#include "phylogen.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;
using namespace phylo::VM;

Instance::Instance()
{
	m_ByteCode.resize(1, 0); // Default to 8 bytes of nothingness.
#if ENABLE_TRANSLATION_TABLE
	for (uint i = 0; i < 256; ++i)
	{
		OpTranslationTable[i] = i;
	}
#endif
}

void Instance::generate_bytecode_hash()
{
	uint32 hash_value = 0;
	uint8 *octetsDst = (uint8 *)&hash_value;
	for (auto codet : m_ByteCode)
	{
		const uint8 *octetsSrc = (const uint8 *)&codet;
		octetsDst[0] += octetsSrc[0] + octetsSrc[3];
		octetsDst[1] += octetsSrc[1] + octetsSrc[3];
		octetsDst[2] += octetsSrc[2] + octetsSrc[3];
		octetsDst[0] += octetsSrc[4] + octetsSrc[7];
		octetsDst[1] += octetsSrc[5] + octetsSrc[7];
		octetsDst[2] += octetsSrc[6] + octetsSrc[7];
	}
	const vector4F hsv = { (float(octetsDst[0]) / 255.5f) * 6.0f, float(octetsDst[1]) / 255.5f, float(octetsDst[2]) / 255.5f, 1.0f };
	m_Cell->setColorHash1(hsv);
}

void Instance::mutate()
{
	Cell &cell = *m_Cell;

	// until we do bytecode saving, there's no reason to copy.
	array<uint64> &bytecode = m_ByteCode;

	bool mutated = false;

	auto &randomGen = cell.getRandom();

	Cell *cell_ptr = &cell;
	auto mutateRoll = [&randomGen]() -> float { return randomGen.uniform(0.0f, 1.0f); };

	// Mutate Bytecode.

	if (bytecode.size() && mutateRoll() < options::LiveMutationChance)
	{
		// Mutate Bytecode.

		uint idx = cell_ptr->getRandom().uniform(0u, 6u);

		switch (idx)
		{
		case 0:
		{
			uint8 replacement = randomGen.uniform<uint8>(0, 255); // is this range correct?
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size() * sizeof(bytecode[0]));
			array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
			mutateArray[mutateOffset] = replacement;
		}
		break;
		case 1:
		{
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size() * sizeof(bytecode[0]));
			array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
			++mutateArray[mutateOffset];
			break;
		}
		case 2:
		{
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size() * sizeof(bytecode[0]));
			array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
			--mutateArray[mutateOffset];
		}
		break;
		case 3:
		{
			uint8 operand = randomGen.uniform<uint8>(0, 255);
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size() * sizeof(bytecode[0]));
			array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
			mutateArray[mutateOffset] += operand;
			break;
		}
		case 4:
		{
			uint8 operand = randomGen.uniform<uint8>(0, 255);
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size() * sizeof(bytecode[0]));
			array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
			mutateArray[mutateOffset] -= operand;
		}
		break;
		case 5:
		{
			usize insertion = randomGen.uniform<usize>(0, usize(-1)); // is this range correct?
			auto mutateOffset = randomGen.uniform<uint>(0, bytecode.size());
			bytecode.insert(mutateOffset, insertion);
		}
		break;
		case 6:
		{
			auto mutateOffset = randomGen.uniform_exclusive<uint>(0, bytecode.size());
			bytecode.erase_at(mutateOffset);
		}
		break;
		case 7:
		{
			const usize copyOffset = randomGen.uniform_exclusive<usize>(0, bytecode.size());
			const usize copyMaxSize = bytecode.size() - copyOffset;
			if (copyMaxSize > 0)
			{
				using bytecode_type = typename std::remove_reference<decltype(bytecode)>::type::value_type;
				thread_local static array<bytecode_type> temp_bytecode;
				const usize copySize = randomGen.uniform<usize>(1ull, copyMaxSize);
				const usize mutateOffset = randomGen.uniform<usize>(0, bytecode.size());

				if (mutateOffset == 0) // We are copying to the front of the bytecode.
				{
					const usize originalSize = bytecode.size();
					const usize newSize = originalSize + copySize;

					// enlarge bytecode array to fit the new chunk.
					bytecode.resize(newSize);
					// memmove the existing data in the array 'copySize' down.
					xassert(originalSize <= newSize, "Error");
					xassert(originalSize <= (newSize - copySize), "Error");
					memmove(
						bytecode.data() + copySize,
						bytecode.data(),
						originalSize * sizeof(bytecode_type)
					);
					// copy the range we are copying to the start.
					xassert(copySize <= newSize, "Error");
					xassert(copySize <= (newSize - (copySize + copyOffset)), "Error");
					memcpy(
						bytecode.data(),
						bytecode.data() + copySize + copyOffset,
						copySize * sizeof(bytecode_type)
					);
				}
				else if (mutateOffset == bytecode.size()) // We are copying to the end of the bytecode.
				{
					const usize originalSize = bytecode.size();
					const usize newSize = originalSize + copySize;
					
					// enlarge bytecode array to fit the new chunk.
					bytecode.resize(newSize);
					// Copy the range to the end.
					xassert(copySize <= (newSize - originalSize), "Error");
					xassert(copySize <= (newSize - copyOffset), "Error");
					memcpy(
						bytecode.data() + originalSize,
						bytecode.data() + copyOffset,
						copySize * sizeof(bytecode_type)
					);
				}
				else // We are copying somewhere inside. This could be optimized.
				{
					const usize dataFromMutateOffset = bytecode.size() - mutateOffset;
					// Resize the 'temp_bytecode' array for the copy.
					temp_bytecode.resize(copySize);
					// Copy the data into the temporary array.
					xassert(copySize <= (bytecode.size() - copyOffset), "Error");
					memcpy(temp_bytecode.data(), bytecode.data() + copyOffset, copySize * sizeof(bytecode_type));
					// Enlarge the bytecode array.
					bytecode.resize(bytecode.size() + copySize);

					// Move data at the copy offset to the end of the copy range.
					xassert(dataFromMutateOffset <= (bytecode.size() - (mutateOffset + copySize)), "Error");
					xassert(dataFromMutateOffset <= (bytecode.size() - mutateOffset), "Error");
					memmove(
						bytecode.data() + mutateOffset + copySize,
						bytecode.data() + mutateOffset,
						dataFromMutateOffset * sizeof(bytecode_type)
					);

					// Copy the mutating data.
					xassert(copySize <= (bytecode.size() - mutateOffset), "Error");
					memcpy(bytecode.data() + mutateOffset, temp_bytecode.data(), copySize * sizeof(bytecode_type));
				}
			}
		}
		break;
		}

		mutated = true;
	}

	if (mutated)
	{
		if (bytecode.size() == 0)
		{
			bytecode.push_back(0);
		}
		if (bytecode.size() >= options::MaxBytecodeSize)
		{
			bytecode.resize(options::MaxBytecodeSize);
			bytecode.truncate(options::MaxBytecodeSize);
		}

		set_bytecode_live((array<uint64> &&)bytecode);
	}
}

uint64 Instance::op_Copy(Register &resultRegister, const Register &reg)
{
	resultRegister = reg;

	return 0;
}

uint64 Instance::op_Load(Register &resultRegister, const Register &reg)
{
	// Reg is storing the index of the actual register holding our data.
	const uint srcIndex = uint16(reg) % uint(m_Registers.size());

	resultRegister = m_Registers[srcIndex];
	return 0;
}

uint64 Instance::op_Store(Register &resultRegister, const Register &reg)
{
	// The destination register stores the index of the actual register we want to populate. Weird!
	const uint dstIndex = uint16(resultRegister) % uint(m_Registers.size());

	m_Registers[dstIndex] = reg;
	return 0;
}

uint64 Instance::op_LoadStore(Register &resultRegister, const Register &reg)
{
	const uint srcIndex = uint16(reg) % uint(m_Registers.size());
	const uint dstIndex = uint16(resultRegister) % uint(m_Registers.size());

	m_Registers[dstIndex] = m_Registers[srcIndex];
	return 0;
}

uint64 Instance::op_Jump(Register &resultRegister, int16 amount)
{
	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value != 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpNZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value == 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpGZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value <= 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpLZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value >= 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpGEZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value < 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_JumpLEZ(Register &resultRegister, int16 amount, int16 value)
{
	if (value < 0)
	{
		resultRegister = 0_u16;
		return 0;
	}

	m_ProgramCounter += amount;
	m_ProgramCounter %= m_ByteCode.size();
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");

	resultRegister = 1_u16;
	return 0;
}

uint64 Instance::op_Add(Register &resultRegister, int16 oper1, int16 oper2)
{
	resultRegister = Register(uint16(oper1 + oper2));

	return 0;
}

uint64 Instance::op_Subtract(Register &resultRegister, int16 oper1, int16 oper2)
{
	resultRegister = Register(uint16(oper1 - oper2));

	return 0;
}

uint64 Instance::op_Multiply(Register &resultRegister, int16 oper1, int16 oper2)
{
	resultRegister = Register(uint16(oper1 * oper2));

	return 0;
}

uint64 Instance::op_Divide(Register &resultRegister, int16 oper1, int16 oper2)
{
	if (oper2 == 0)
	{
		resultRegister = Register(0_u16);
		return 0;
	}

	resultRegister = Register(uint16(oper1 / oper2));

	return 0;
}

uint64 Instance::op_Modulo(Register &resultRegister, int16 oper1, int16 oper2)
{
	if (oper2 == 0)
	{
		resultRegister = Register(0_u16);
		return 0;
	}

	resultRegister = Register(uint16(oper1 % oper2));

	return 0;
}

uint64 Instance::op_AddF(Register &resultRegister, float oper1, float oper2)
{
	resultRegister = Register(clamp(oper1 + oper2, 0.0f, 1.0f));

	return 0;
}

uint64 Instance::op_SubtractF(Register &resultRegister, float oper1, float oper2)
{
	resultRegister = Register(clamp(oper1 - oper2, 0.0f, 1.0f));

	return 0;
}

uint64 Instance::op_MultiplyF(Register &resultRegister, float oper1, float oper2)
{
	resultRegister = Register(clamp(oper1 * oper2, 0.0f, 1.0f));

	return 0;
}

uint64 Instance::op_DivideF(Register &resultRegister, float oper1, float oper2)
{
	if (oper2 == 0.0)
	{
		resultRegister = Register(0.0f);
		return 0;
	}

	resultRegister = Register(clamp(oper1 / oper2, 0.0f, 1.0f));

	return 0;
}

uint64 Instance::op_ModuloF(Register &resultRegister, float oper1, float oper2)
{
	if (oper2 == 0.0)
	{
		resultRegister = Register(0.0f);
		return 0;
	}

	resultRegister = Register(clamp(xtd::mod(oper1, oper2), 0.0f, 1.0f));

	return 0;
}

uint64 Instance::op_LAND(Register &resultRegister, int16 oper1, int16 oper2)
{
	const bool bool1 = oper1 != 0;
	const bool bool2 = oper2 != 0;

	resultRegister = Register((bool1 && bool2) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_LNAND(Register &resultRegister, int16 oper1, int16 oper2)
{
	const bool bool1 = oper1 != 0;
	const bool bool2 = oper2 != 0;

	resultRegister = Register((!(bool1 && bool2)) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_LOR(Register &resultRegister, int16 oper1, int16 oper2)
{
	const bool bool1 = oper1 != 0;
	const bool bool2 = oper2 != 0;

	resultRegister = Register((bool1 || bool2) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_LNOR(Register &resultRegister, int16 oper1, int16 oper2)
{
	const bool bool1 = oper1 != 0;
	const bool bool2 = oper2 != 0;

	resultRegister = Register((!(bool1 || bool2)) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_LNEGATE(Register &resultRegister, int16 oper)
{
	const bool bool1 = oper != 0;

	resultRegister = Register((!bool1) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_LXOR(Register &resultRegister, int16 oper1, int16 oper2)
{
	const uint bool1 = (oper1 != 0) ? 1 : 0;
	const uint bool2 = (oper2 != 0) ? 1 : 0;

	resultRegister = Register((uint16(bool1 ^ bool2) & 1) ? 1_u16 : 0_u16);

	return 0;
}

uint64 Instance::op_Sleep(Register &resultRegister, uint16 ticks)
{
	m_SleepCount += ticks;

	resultRegister = traits<uint16>::ones;
	return 0;
}

uint64 Instance::op_Move(Register &resultRegister, float amount)
{
  m_Cell->m_MoveSpeed = amount;
  m_Cell->m_MoveState = !m_Cell->m_MoveState;

	resultRegister = traits<uint16>::ones;
  return 0;
}

uint64 Instance::op_Rotate(Register &resultRegister, float amount)
{
	amount *= 2.0f;
	amount -= 1.0f;
	vector2F direction = m_Cell->m_PhysicsInstance->m_Direction;
	float radians = atan2(direction.y, direction.x);
	radians += (amount * xtd::pi<float> * 2.0f);
	direction = { cos(radians), sin(radians) };
	m_Cell->m_PhysicsInstance->m_Direction = direction;

	resultRegister = traits<uint16>::ones;
	return uint64((float(options::BaseRotateCost) * amount + 1.0f) + 0.5f);
}

uint64 Instance::op_Split(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	if (cell->collidedThisTick())
	{
		resultRegister = 0_u16;
		return options::BaseSplitCost;
	}

	// Temporary hack.
	//if (cell->m_Simulation.get_num_cells() > 1000)
	//{
	//   return 0;
	//}

	float Radius = cell->m_PhysicsInstance->m_Radius;
	if (Radius < options::MinCellSize * 2.0f)
	{
		resultRegister = 0_u16;
		return options::BaseSplitCost;
	}

	// Split task here
	// Split the sphere as a circle.

	if (Radius < options::MinCellSize * 2.0f)
	{
		resultRegister = 0_u16;
		return options::BaseSplitCost;
	}

	float newRadius = Radius * 0.5f;       // Because this is based upon the 2d radius rather than the 3d storage volume for energy, splitting is a lossy operation for the cell.
	float halfRadius = Radius * 0.5f;
	float quarterRadius = Radius * 0.25f;

	vector2F direction = cell->m_PhysicsInstance->m_Direction;
	float radians = cell->getRandom().uniform<float>(0.0f, 2.0f * xtd::pi<float>);
	direction = { cos(radians), sin(radians) }; // Axis of creation.
	direction *= halfRadius;
	//direction *= 0.25; // because.

	vector2F pos1 = cell->m_PhysicsInstance->m_Position + direction;
	vector2F pos2 = cell->m_PhysicsInstance->m_Position - direction;

	// Validate that the position is valid.
	Cell *foundCell = controller->m_Simulation.findCell(pos2, newRadius * 0.8f, cell);
	if (foundCell)
	{
		resultRegister = 0_u16;
		return options::BaseSplitCost;
	}

	//if (cell->getRandom().uniform(0.0, 1.0) > 0.5)
	//{
	//   swap(pos1, pos2);
	//}

	xassert(pos1 != pos2, "These must not be the same");

	const float costMultiplier = max(float(m_ByteCode.size()) / float(options::BaselineBytecodeSize * 16), 1.0f);
	uint64 newEnergy = cell->m_uEnergy;
	float newEnergyLoss = float(newEnergy) * 0.5f; // Reproduction requires 50 % of energy baseline.
	uint64 energyLoss = min(newEnergy, uint64((newEnergyLoss * costMultiplier)));
	newEnergy = newEnergy - energyLoss;
	if (newEnergy == 0)
	{
		resultRegister = 0_u16;
		return options::BaseSplitCost;
	}

	uint64 halfEnergy = newEnergy / 2u;
	uint64 originalEnergy = cell->m_uEnergy;

	// Adjust main cell
	cell->setRadius(newRadius);
	const auto cellCapacity = cell->getObjectCapacity();
	cell->m_PhysicsInstance->m_Position = pos1;
	cell->m_uEnergy = min(cellCapacity, halfEnergy);

	float newRadians = cell->getRandom().uniform<float>(0.0f, 2.0f * xtd::pi<float>);
	vector2F newDirection = { cos(newRadians), sin(newRadians) };

	// Detect off-by-ones in the integer divide-by-two
	// If you have an indivisible number, like '5', 5 / 2 == 2. Thus, you lose '1' energy point.
	// You can only have a remainder of one in a divide-by-two. We could also do this with a modulus and save
	// the branch. Not convinced as to which is faster.
	if (halfEnergy + halfEnergy != originalEnergy)
	{
		halfEnergy += 1;
	}

	++cell->m_NumChildren;

	scoped_lock<mutex> _lock(controller->m_SerializedLock);
	controller->m_SerializedTasks += {cell->m_CellID, [=]() {
		// Create new cell
		Cell *pNewCell = &cell->m_Simulation.getNewCell(cell);

		//scoped_lock<mutex> _lock(controller->m_UnserializedLock); // Contention is impossible.
		//controller->m_UnserializedTasks += [=]() {
		   // I'm moving mutation work back to unserialized space, and using the original cells random generator.
		   // Should be faster.
		array<uint64> bytecode = cell->m_VMInstance->m_ByteCode;
		{
			auto mutateRoll = [pNewCell]() -> float { return pNewCell->getRandom().uniform(0.0f, 1.0f); };

			if (bytecode.size() && mutateRoll() < options::MutationSubstitutionChance)
			{
				uint8 replacement = pNewCell->getRandom().uniform<uint8>(0, 255); // is this range correct?
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, (bytecode.size() * sizeof(bytecode[0])) - 1);
				array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
				mutateArray[mutateOffset] = replacement;
			}
			if (bytecode.size() && mutateRoll() < options::MutationIncrementChance)
			{
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, (bytecode.size() * sizeof(bytecode[0])) - 1);
				array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
				++mutateArray[mutateOffset];
			}
			if (bytecode.size() && mutateRoll() < options::MutationDecrementChance)
			{
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, (bytecode.size() * sizeof(bytecode[0])) - 1);
				array_view<uint8> mutateArray = { (uint8 *)bytecode.data(), bytecode.size_raw() };
				--mutateArray[mutateOffset];
			}
			if (bytecode.size() && mutateRoll() < options::MutationInsertionChance)
			{
				uint64 insertion = pNewCell->getRandom().uniform<uint64>(0, uint64(-1)); // is this range correct?
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, bytecode.size());
				bytecode.insert(mutateOffset, insertion);
			}
			if (bytecode.size() && mutateRoll() < options::MutationDeletionChance)
			{
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);
				bytecode.erase_at(mutateOffset);
			}
			if (bytecode.size() && mutateRoll() < options::MutationDuplicationChance)
			{
				uint mutateSource = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, bytecode.size());
				uint64 insertion = bytecode[mutateSource];
				bytecode.insert(mutateOffset, insertion);
			}
			if (bytecode.size() && mutateRoll() < options::MutationRangeDuplicationChance)
			{
				uint mutateSource = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);
				auto mutateOffset = pNewCell->getRandom().uniform<uint>(0, bytecode.size());

				uint mutateSize = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);
				mutateSize = min(mutateSize, bytecode.size() - mutateSource);

				array<uint64> tempBytecode;
				tempBytecode.reserve(mutateSize);
				for (uint i = 0; i < mutateSize; ++i)
				{
					tempBytecode += bytecode[i + mutateSource];
				}

				for (uint i = 0; i < mutateSize; ++i)
				{
					bytecode.insert(mutateOffset + i, tempBytecode[i]);
				}
			}
			if (bytecode.size() && mutateRoll() < options::MutationRangeDeletionChance)
			{
				uint mutateSource = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);

				uint mutateSize = pNewCell->getRandom().uniform<uint>(0, bytecode.size() - 1);
				mutateSize = min(mutateSize, bytecode.size() - mutateSource);
				for (uint i = 0; i < mutateSize; ++i)
				{
					bytecode.erase_at(mutateSource);
				}
			}
		}

		if (bytecode.size() == 0)
		{
			bytecode.push_back(0);
		}

		if (bytecode.size() >= options::MaxBytecodeSize)
		{
			bytecode.resize(options::MaxBytecodeSize);
		}

		Cell &newCell = *pNewCell;

		const auto cellCapacity = cell->getObjectCapacity();
    newCell.m_GrowthPoint = cell->m_GrowthPoint;
		*newCell.m_PhysicsInstance = *cell->m_PhysicsInstance;
    newCell.setRadius(newRadius);
		newCell.m_PhysicsInstance->m_Position = pos2;
		newCell.m_PhysicsInstance->m_ShadowPosition = pos2;
		newCell.m_PhysicsInstance->m_Direction = newDirection;
		newCell.m_uEnergy = min(cellCapacity, halfEnergy);
		newCell.m_ColorGreen = cell->m_ColorGreen;
		newCell.m_ColorRed = cell->m_ColorRed;
		newCell.m_ColorBlue = cell->m_ColorBlue;
		newCell.setArmor(0.001);
		// Slightly mutate HSV.
		auto HSV = cell->m_ColorDye;
		HSV.x += newCell.getRandom().uniform<float>(-0.05, 0.05);
		HSV.y += newCell.getRandom().uniform<float>(-0.05, 0.05);
		HSV.z += newCell.getRandom().uniform<float>(-0.05, 0.05);
		HSV.x = clamp(HSV.x, 0.0f, 6.0f);
		HSV.y = clamp(HSV.y, 0.0f, 1.0f);
		HSV.z = clamp(HSV.z, 0.0f, 1.0f);
		newCell.m_ColorDye = HSV;

		newCell.m_VMInstance->set_bytecode((array<uint64> &&)bytecode);
#if ENABLE_TRANSLATION_TABLE
		memcpy(newCell.m_VMInstance->OpTranslationTable, cell->m_VMInstance->OpTranslationTable, sizeof(newCell.m_VMInstance->OpTranslationTable));

		// VM mutations - yes, the VM itself can mutate. Muahaha.
		//static constexpr double CodeMutationIncrementChance = 0.01;
		//static constexpr double CodeMutationDecrementChance = 0.01;
		//static constexpr double CodeMutationSwapChance = 0.01;

		auto mutateRoll = [pNewCell]() -> float { return pNewCell->getRandom().uniform(0.0f, 1.0f); };

		if (mutateRoll() < options::CodeMutationIncrementChance)
		{
			uint mutateIndex = pNewCell->getRandom().uniform<uint>(0, 256);

			++newCell.m_VMInstance->OpTranslationTable[mutateIndex];
		}
		if (mutateRoll() < options::CodeMutationDecrementChance)
		{
			uint mutateIndex = pNewCell->getRandom().uniform<uint>(0, 256);

			--newCell.m_VMInstance->OpTranslationTable[mutateIndex];
		}
		if (mutateRoll() < options::CodeMutationRandomChance)
		{
			uint mutateIndex = pNewCell->getRandom().uniform<uint>(0, 256);
			uint mutateValue = pNewCell->getRandom().uniform<uint>(0, 256);

			newCell.m_VMInstance->OpTranslationTable[mutateIndex] = mutateValue;
		}
		if (mutateRoll() < options::CodeMutationSwapChance)
		{
			uint mutateSrcIndex = pNewCell->getRandom().uniform<uint>(0, 256);
			uint mutateDstIndex = pNewCell->getRandom().uniform<uint>(0, 256);

			newCell.m_VMInstance->OpTranslationTable[mutateDstIndex] = newCell.m_VMInstance->OpTranslationTable[mutateSrcIndex];
		}
#endif
	}};

	resultRegister = traits<uint16>::ones;
	return options::BaseSplitCost;
}

uint64 Instance::op_Burn(Register &resultRegister, Controller *controller)
{
	uint64 energy = m_Cell->getEnergy();
	energy = uint64(float(energy) * (1.0f - options::BurnEnergyPercentage));
	m_Cell->setEnergy(energy);

	resultRegister = 0_u16;
	return 0;
}

uint64 Instance::op_Suicide(Register &resultRegister, Controller *controller)
{
	m_Cell->setEnergy(0);

	resultRegister = traits<uint16>::ones;
	return 0;
}

uint64 Instance::op_ColorGreen(Register &resultRegister, Controller *controller)
{
	m_Cell->m_ColorGreen += 0.05f;
	vector4F vec = { m_Cell->m_ColorGreen, m_Cell->m_ColorRed, m_Cell->m_ColorBlue };
	vec = vec.normalize();
	m_Cell->m_ColorGreen = vec.x;
	m_Cell->m_ColorRed = vec.y;
	m_Cell->m_ColorBlue = vec.z;

	resultRegister = 0_u16;
	return 10;
}

uint64 Instance::op_ColorRed(Register &resultRegister, Controller *controller)
{
	m_Cell->m_ColorRed += 0.05f;
	vector4F vec = { m_Cell->m_ColorGreen, m_Cell->m_ColorRed, m_Cell->m_ColorBlue };
	vec = vec.normalize();
	m_Cell->m_ColorGreen = vec.x;
	m_Cell->m_ColorRed = vec.y;
	m_Cell->m_ColorBlue = vec.z;

	resultRegister = 0_u16;
	return 10;
}

uint64 Instance::op_ColorBlue(Register &resultRegister, Controller *controller)
{
	m_Cell->m_ColorBlue += 0.05f;
	vector4F vec = { m_Cell->m_ColorGreen, m_Cell->m_ColorRed, m_Cell->m_ColorBlue };
	vec = vec.normalize();
	m_Cell->m_ColorGreen = vec.x;
	m_Cell->m_ColorRed = vec.y;
	m_Cell->m_ColorBlue = vec.z;

	resultRegister = 0_u16;
	return 10;
}

uint64 Instance::op_Grow(Register &resultRegister, float value)
{
	Cell *cell = m_Cell;

  cell->m_GrowthPoint = value;

	resultRegister = 0_u16;
	return 0;
}

uint64 Instance::op_GetEnergy(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const float resultValue = cell->getEnergyFactor();
	resultRegister = Register(resultValue);

	return 0;
}

uint64 Instance::op_GetLightGreen(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	const float resultValue = cell->m_Simulation.getGreenEnergy(position);
	resultRegister = Register(resultValue);

	return 0;
}

uint64 Instance::op_GetLightRed(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	const float resultValue = cell->m_Simulation.getRedEnergy(position);
	resultRegister = Register(resultValue);

	return 0;
}

uint64 Instance::op_GetWaste(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	const uint16 resultValue = min((uint32)traits<uint16>::max, cell->m_Simulation.getBlueEnergy(position));
	resultRegister = Register(resultValue);

	return 0;
}

uint64 Instance::op_WasTouched(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	resultRegister = uint16(cell->m_Touched);
	cell->m_Touched = 0;
	return 0;
}

uint64 Instance::op_WasAttacked(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	resultRegister = uint16(cell->m_Attacked);
	cell->m_Attacked = 0;
	return 0;
}

uint64 Instance::op_See(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	//Cell *foundCell = controller->m_Simulation.findCell(position + (cell->m_PhysicsInstance->m_Direction * cell->m_PhysicsInstance->m_Radius * 1.75), cell->m_PhysicsInstance->m_Radius, cell);
	//
	//if (foundCell)
	//{
	//   resultRegister = traits<uint16>::ones;
	//}
	//else
	{
		resultRegister = 0_u16;
	}

	return 0;
}

uint64 Instance::op_Size(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	Cell *foundCell = controller->m_Simulation.findCell(position + (cell->m_PhysicsInstance->m_Direction * cell->m_PhysicsInstance->m_Radius * 1.75f), cell->m_PhysicsInstance->m_Radius, cell);

	if (foundCell != nullptr)
	{
		resultRegister = foundCell->getShadowRadius() / (options::MaxCellSize + 0.0000001f);
	}
	else
	{
		resultRegister = 0_u16;
	}

	return 0;
}

uint64 Instance::op_MySize(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	resultRegister = cell->getRadius() / (options::MaxCellSize + 0.0000001f);
	return 0;
}

uint64 Instance::op_Armor(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	Cell *foundCell = controller->m_Simulation.findCell(position + (cell->m_PhysicsInstance->m_Direction * cell->m_PhysicsInstance->m_Radius * 1.75), cell->m_PhysicsInstance->m_Radius, cell);

	if (foundCell)
	{
		resultRegister = min(foundCell->getArmor(), 0.9999999999999f);
	}
	else
	{
		resultRegister = 0_u16;
	}

	return 0;
}

uint64 Instance::op_MyArmor(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	resultRegister = min(cell->getArmor(), 0.9999999999999f);
	return 0;
}


uint64 Instance::op_Attack(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	Cell *foundCell = controller->m_Simulation.findCell(position + (cell->m_PhysicsInstance->m_Direction * cell->m_PhysicsInstance->m_Radius * 1.75), cell->m_PhysicsInstance->m_Radius, cell);

	if (foundCell)
	{
		// Now we need to attack!
		{
			scoped_lock<mutex> _lock(controller->m_SerializedLock);
			controller->m_SerializedTasks += {cell->m_CellID, [=]() {
				// We attack! Every attack takes down 1/4 armor.
				if (foundCell->getArmor() > 0.0f)
				{

					// how much armor will we take away?
					float sizeRatio = cell->getVolume() / foundCell->getVolume();

					foundCell->setArmor(max(foundCell->getArmor() - (0.9f * sizeRatio), 0.0f));

					// If armor is zero, this cell is going to be killed off. This will happen elsewhere, for now we will just flag it as us having killed it.
					// We will also do a check there to see if we die first.
					foundCell->setKilledBy(cell);
				}
				++foundCell->m_AttackedRemote;
			}};
		}
		resultRegister = traits<uint16>::ones;
	}
	else
	{
		resultRegister = 0_u16;
	}

	return 2;
}

uint64 Instance::op_Transfer(Register &resultRegister, Controller *controller, int16 oper1, int16 oper2)
{
	Cell *cell = m_Cell;

	const vector2F &position = cell->m_PhysicsInstance->m_Position;

	Cell *foundCell = controller->m_Simulation.findCell(position + (cell->m_PhysicsInstance->m_Direction * cell->m_PhysicsInstance->m_Radius * 1.75), cell->m_PhysicsInstance->m_Radius, cell);

	if (foundCell)
	{

		// oper 1 is the offset we insert, oper2 is the size. Size is clamped at our bytecode's size, and 0 also means 'all'.
		// It will be injected randomly into the other cell.

		uint32 uoper1 = oper1;
		uint32 uoper2 = oper2;

		const auto &bytecode = m_ByteCode;
		uoper1 = min(uint32(uoper1), bytecode.size());
		if (uoper2 == 0)
		{
			uoper2 = bytecode.size() - uoper1;
		}
		else
		{
			uoper2 = min(uoper2, bytecode.size() - uoper1);
		}
		// Special case oddness - just inject _everything_.
		if (uoper2 == 0)
		{
			uoper1 = 0;
			uoper2 = bytecode.size();
		}

		{
			scoped_lock<mutex> _lock(controller->m_SerializedLock);
			controller->m_SerializedTasks += {cell->m_CellID, [=]() {
				// Where should we insert?
				const auto &other_bytecode = foundCell->m_VMInstance->m_ByteCode;

				const uint32 targetOffset = m_Cell->getRandom().uniform<uint32>(0, other_bytecode.size());

				decltype(m_ByteCode) newBytecode;
				usize newSize = other_bytecode.size() + uoper2;
				newSize = min(newSize, options::MaxBytecodeSize);
				newBytecode.reserve(newSize);
				// This is sort of slow. Optimize later TODO.
				usize curi = 0;
				for (; curi < targetOffset; ++curi)
				{
					newBytecode.push_back(other_bytecode[curi]);
				}
				for (uint32 i = 0; i < uoper2; ++i)
				{
					if (newBytecode.size() >= newSize)
					{
						break;
					}
					newBytecode.push_back(bytecode[i + uoper1]);
				}
				for (; curi < other_bytecode.size(); ++curi)
				{
					if (newBytecode.size() >= newSize)
					{
						break;
					}
					newBytecode.push_back(other_bytecode[curi]);
				}
				foundCell->m_VMInstance->set_bytecode_live(newBytecode);
			}};
		}
		resultRegister = traits<uint16>::ones;
	}
	else
	{
		resultRegister = 0_u16;
		return 0;
	}

	return 100;
}

uint64 Instance::op_SleepTouched(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	cell->m_VMInstance->m_SleepState = SleepState::Touched;
	resultRegister = 0_u16;
	return 0;
}

uint64 Instance::op_SleepAttacked(Register &resultRegister, Controller *controller)
{
	Cell *cell = m_Cell;
	cell->m_VMInstance->m_SleepState = SleepState::Attacked;
	resultRegister = 0_u16;
	return 0;
}

#define CASE_1R2R(x) case uint16(x) | REG1_R_REG2_R
#define CASE_1V2R(x) case uint16(x) | REG1_V_REG2_R
#define CASE_1R2V(x) case uint16(x) | REG1_R_REG2_V
#define CASE_1V2V(x) case uint16(x) | REG1_V_REG2_V

#define CONTROLLER_CASE(x, func) \
   CASE_1R2R(x):                 \
   CASE_1V2R(x) :                \
   CASE_1R2V(x) :                \
   CASE_1V2V(x) :                \
      Cost = func(resultRegister, controller);   \
      break;

#define ONE_PARAM_CASE(x, func)                                                           \
   CASE_1R2R(x) :                                                                         \
   {                                                                                      \
      Cost = func(resultRegister, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]));      \
   } break;                                                                               \
   CASE_1V2R(x) :                                                                         \
   {                                                                                      \
      Cost = func(resultRegister, (Register(uint16(opUnion.Operand1))));                            \
   } break;                                                                               \
   CASE_1R2V(x) :                                                                         \
   {                                                                                      \
      Cost = func(resultRegister, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]));      \
   } break;                                                                               \
   CASE_1V2V(x) :                                                                         \
   {                                                                                      \
      Cost = func(resultRegister, (Register(uint16(opUnion.Operand1))));                            \
   } break

#define TWO_PARAM_CASE(x, func)                                                                                                                             \
   CASE_1R2R(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]), (m_Registers[uint(opUnion.Operand2) % m_Registers.size()]));      \
   } break;                                                                                                                                                 \
   CASE_1V2R(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, (Register(uint16(opUnion.Operand1))), (m_Registers[uint(opUnion.Operand2) % m_Registers.size()]));                            \
   } break;                                                                                                                                                 \
   CASE_1R2V(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]), (Register(uint16(opUnion.Operand2))));                            \
   } break;                                                                                                                                                 \
   CASE_1V2V(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, (Register(uint16(opUnion.Operand1))), (Register(uint16(opUnion.Operand2))));                                                  \
   } break

#define CONTROLLER_TWO_PARAM_CASE(x, func)                                                                                                                  \
   CASE_1R2R(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, controller, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]), (m_Registers[uint(opUnion.Operand2) % m_Registers.size()]));      \
   } break;                                                                                                                                                 \
   CASE_1V2R(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, controller, (Register(uint16(opUnion.Operand1))), (m_Registers[uint(opUnion.Operand2) % m_Registers.size()]));                            \
   } break;                                                                                                                                                 \
   CASE_1R2V(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, controller, (m_Registers[uint(opUnion.Operand1) % m_Registers.size()]), (Register(uint16(opUnion.Operand2))));                            \
   } break;                                                                                                                                                 \
   CASE_1V2V(x) :                                                                                                                                           \
   {                                                                                                                                                        \
      Cost = func(resultRegister, controller, (Register(uint16(opUnion.Operand1))), (Register(uint16(opUnion.Operand2))));                                                  \
   } break

void Instance::tick(Controller *controller, CounterType &counter)
{
	Cell * __restrict cell = m_Cell;

	if (!cell->m_Alive) [[unlikely]]
	{
		return;
	}

	bool sleep = false;
	if ((m_SleepCount > 0) & (cell->m_uEnergy != 0U)) [[likely]]
	{
		--m_SleepCount;
		++counter[uint(VM::Operation::Sleep)];
		sleep = true;
	}

	switch (m_SleepState)
	{
	case SleepState::Touched:
	{
		if (cell->m_Touched)
		{
			m_SleepState = SleepState::None;
			cell->m_Touched = 0;
		}
		else
		{
			++counter[uint(VM::Operation::Sleep_Touch)];
			sleep = true;
		}
	} break;
	case SleepState::Attacked:
	{
		if (cell->m_Attacked)
		{
			m_SleepState = SleepState::None;
			cell->m_Attacked = 0;
		}
		else
		{
			++counter[uint(VM::Operation::Sleep_Attack)];
			sleep = true;
		}
	} break;
	}

	const float costMultiplier = max(float(m_ByteCode.size()) / float(options::BaselineBytecodeSize), 1.0f);

	if (sleep) [[likely]]
	{
		uint64 energyCost = uint64(options::SleepTickEnergyLost * costMultiplier); // This is the base cost presuming a volume of '1.0'. As cells get larger,
													 // this gets higher - simulates respiration being more expensive with worse surface area to volume ratios.
		energyCost = max(1ull, uint64(float(energyCost) * cell->getSuperVolume()));

		cell->m_uEnergy -= min(energyCost, uint64(cell->m_uEnergy)); // sleeping is very efficient.

		if (cell->m_uEnergy == 0)
		{
			// Kill the cell.
			scoped_lock<mutex> _lock(controller->m_SerializedLock);
			controller->m_KillTasks += cell;
		}

		return;
	}

	uint64 energyCost = uint64(options::TickEnergyLost * costMultiplier); // This is the base cost presuming a volume of '1.0'. As cells get larger,
	// this gets higher - simulates respiration being more expensive with worse surface area to volume ratios.
	energyCost = max(1ull, uint64(float(energyCost) * cell->getSuperVolume()));

	cell->m_uEnergy -= min(energyCost, uint64(cell->m_uEnergy));

	if (cell->m_uEnergy == 0u)
	{
		// Kill the cell.
		scoped_lock<mutex> _lock(controller->m_SerializedLock);
		controller->m_KillTasks += cell;
		return;
	}

	// We execute one instruction here.
	xassert(m_ProgramCounter <= m_ByteCode.size(), "How did the PC go past bytecode end?");
	uint programCounter = m_ProgramCounter;
	++m_ProgramCounter;
	m_ProgramCounter %= m_ByteCode.size();

	uint64 operation = m_ByteCode[programCounter];

#if ENABLE_TRANSLATION_TABLE
	// handle table transformation.
	uint8 *operationArray = (uint8 *)&operation;
	for (uint i = 0; i < sizeof(operation); ++i)
	{
		operationArray[i] = OpTranslationTable[operationArray[i]];
	}
#endif

	// This part will only work on Little Endian systems. Need a better solution if we ever migrate to PPC.
	Operation &opUnion = *(Operation *)&operation;
	opUnion.OpCode %= uint16(VM::Operation::MaximumCount); // otherwise evolution is VERY difficult.
	Register &resultRegister = m_Registers[uint(opUnion.ResultRegister) % m_Registers.size()];
	// Last two bits of the fullOpcode include type information.

	static constexpr const uint16 REG1_R_REG2_R = (1 << 14) | (1 << 15);
	static constexpr const uint16 REG1_V_REG2_R = (1 << 15);
	static constexpr const uint16 REG1_R_REG2_V = (1 << 14);
	static constexpr const uint16 REG1_V_REG2_V = 0;

	uint64 Cost = 0;

	//opUnion.OpCode = OpTranslationTable[min(opUnion.OpCode, uint64(OP_MAX))];
	uint16 fullOpcode = operation & 0xFFFF;

	uint16 opcodeForTracking = min(uint16(opUnion.OpCode), uint16(VM::Operation::MaximumCount));
	++counter[opcodeForTracking];

	switch (fullOpcode) {
		ONE_PARAM_CASE(VM::Operation::Sleep, op_Sleep);

		default:
			switch (fullOpcode)
			{
			default:
				CASE_1R2R(VM::Operation::NOP) :
				CASE_1V2R(VM::Operation::NOP) :
				CASE_1R2V(VM::Operation::NOP) :
				CASE_1V2V(VM::Operation::NOP) :
					// Do nothing!
					break;

				ONE_PARAM_CASE(VM::Operation::Copy, op_Copy);
				ONE_PARAM_CASE(VM::Operation::Load, op_Load);
				ONE_PARAM_CASE(VM::Operation::Store, op_Store);
				ONE_PARAM_CASE(VM::Operation::Load_Store, op_LoadStore);

				ONE_PARAM_CASE(VM::Operation::Jump, op_Jump);
				TWO_PARAM_CASE(VM::Operation::Jump_Z, op_JumpZ);
				TWO_PARAM_CASE(VM::Operation::Jump_NZ, op_JumpNZ);
				TWO_PARAM_CASE(VM::Operation::Jump_GZ, op_JumpGZ);
				TWO_PARAM_CASE(VM::Operation::Jump_LZ, op_JumpLZ);
				TWO_PARAM_CASE(VM::Operation::Jump_GEZ, op_JumpGEZ);
				TWO_PARAM_CASE(VM::Operation::Jump_LEZ, op_JumpLEZ);

				TWO_PARAM_CASE(VM::Operation::Add_Integer, op_Add);
				TWO_PARAM_CASE(VM::Operation::Subtract_Integer, op_Subtract);
				TWO_PARAM_CASE(VM::Operation::Multiply_Integer, op_Multiply);
				TWO_PARAM_CASE(VM::Operation::Divide_Integer, op_Divide);
				TWO_PARAM_CASE(VM::Operation::Modulo_Integer, op_Modulo);

				TWO_PARAM_CASE(VM::Operation::Add_Float, op_AddF);
				TWO_PARAM_CASE(VM::Operation::Subtract_Float, op_SubtractF);
				TWO_PARAM_CASE(VM::Operation::Multiply_Float, op_MultiplyF);
				TWO_PARAM_CASE(VM::Operation::Divide_Float, op_DivideF);
				TWO_PARAM_CASE(VM::Operation::Modulo_Float, op_ModuloF);

				TWO_PARAM_CASE(VM::Operation::LogicalAND, op_LAND);
				TWO_PARAM_CASE(VM::Operation::LogicalNAND, op_LNAND);
				TWO_PARAM_CASE(VM::Operation::LogicalOR, op_LOR);
				TWO_PARAM_CASE(VM::Operation::LogicalNOR, op_LNOR);
				ONE_PARAM_CASE(VM::Operation::LogicalNEGATE, op_LNEGATE);
				TWO_PARAM_CASE(VM::Operation::LogicalXOR, op_LXOR);

				ONE_PARAM_CASE(VM::Operation::Move, op_Move);
				ONE_PARAM_CASE(VM::Operation::Rotate, op_Rotate);

				CONTROLLER_CASE(VM::Operation::Split, op_Split);
				CONTROLLER_CASE(VM::Operation::Burn, op_Burn);
				CONTROLLER_CASE(VM::Operation::Suicide, op_Suicide);
				CONTROLLER_CASE(VM::Operation::Color_Green, op_ColorGreen);
				CONTROLLER_CASE(VM::Operation::Color_Red, op_ColorRed);
				CONTROLLER_CASE(VM::Operation::Color_Blue, op_ColorBlue);
		    ONE_PARAM_CASE(VM::Operation::Grow, op_Grow);
				CONTROLLER_CASE(VM::Operation::GetEnergy, op_GetEnergy);
				CONTROLLER_CASE(VM::Operation::GetLight_Green, op_GetLightGreen);
				CONTROLLER_CASE(VM::Operation::GetLight_Red, op_GetLightRed);
				CONTROLLER_CASE(VM::Operation::GetWaste, op_GetWaste);
				CONTROLLER_CASE(VM::Operation::Attack, op_Attack);
				CONTROLLER_TWO_PARAM_CASE(VM::Operation::Transfer, op_Transfer);

				CONTROLLER_CASE(VM::Operation::WasTouched, op_WasTouched);
				CONTROLLER_CASE(VM::Operation::WasAttacked, op_WasAttacked);
				CONTROLLER_CASE(VM::Operation::See, op_See);
				CONTROLLER_CASE(VM::Operation::Size, op_Size);
				CONTROLLER_CASE(VM::Operation::MySize, op_MySize);
				CONTROLLER_CASE(VM::Operation::Armor, op_Armor);
				CONTROLLER_CASE(VM::Operation::MyArmor, op_MyArmor);
				CONTROLLER_CASE(VM::Operation::Sleep_Touch, op_SleepTouched);
				CONTROLLER_CASE(VM::Operation::Sleep_Attack, op_SleepAttacked);
			}
			break;
	}

	if (Cost != 0)
	{
		Cost = max(1ull, uint64(float(Cost) * cell->getVolume()));
	}

	Cost = min(Cost, uint64(cell->m_uEnergy));

	cell->m_uEnergy -= Cost;

	if ((cell->m_uEnergy == 0u) | (Cost == uint16(-1)))
	{
		// Kill the cell.
		scoped_lock<mutex> _lock(controller->m_SerializedLock);
		controller->m_KillTasks += cell;
	}

}

void Instance::unserialize(Stream &inStream, Cell *cell)
{
	inStream.read(m_Registers);
	inStream.read(m_ProgramCounter);
#if ENABLE_TRANSLATION_TABLE
	inStream.read(OpTranslationTable);
#endif
	inStream.read(m_SleepState);
	inStream.read(m_SleepCount);
	xtd::array<uint64>::size_type bytecodeLen = 0;
	inStream.read(bytecodeLen);
	m_ByteCode.resize(bytecodeLen);
	inStream.readRaw(m_ByteCode.data(), m_ByteCode.size_raw());
}

void Instance::serialize(Stream &outStream) const
{
	outStream.write(m_Registers);
	outStream.write(m_ProgramCounter);
#if ENABLE_TRANSLATION_TABLE
	outStream.write(OpTranslationTable);
#endif
	outStream.write(m_SleepState);
	outStream.write(m_SleepCount);
	outStream.write(m_ByteCode.size());
	outStream.writeRaw(m_ByteCode.data(), m_ByteCode.size_raw());
}
