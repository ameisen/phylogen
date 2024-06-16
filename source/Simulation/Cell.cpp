#include "phylogen.hpp"
#include "Simulation/Simulation.hpp"
#include "Physics/PhysicsController.hpp"
#include "VM/VMInstance.hpp"

using namespace phylo;

namespace
{
	static color hue_to_rgb(const vector4F &hsv)
	{
		// TODO license -  http://www.java2s.com/Code/Java/2D-Graphics-GUI/HSVtoRGB.htm
		// H is given on [0->6] or -1. S and V are given on [0->1].
		// RGB are each returned on [0->1].
		float m, n, f;
		int i;

		color rgb;
		rgb.a = 1.0f;

		if (hsv.x == -1)
		{
			rgb[0] = rgb[1] = rgb[2] = hsv.z;
			return rgb;
		}
		i = (int)(floorf(hsv.x));
		f = hsv.x - i;
		if (i % 2 == 0)
		{
			f = 1 - f; // if i is even
		}
		m = hsv.z * (1 - hsv.y);
		n = hsv.z * (1 - hsv.y * f);
		switch (i)
		{
		case 6:
		case 0:
			rgb[0] = hsv.z;
			rgb[1] = n;
			rgb[2] = m;
			break;
		case 1:
			rgb[0] = n;
			rgb[1] = hsv.z;
			rgb[2] = m;
			break;
		case 2:
			rgb[0] = m;
			rgb[1] = hsv.z;
			rgb[2] = n;
			break;
		case 3:
			rgb[0] = m;
			rgb[1] = n;
			rgb[2] = hsv.z;
			break;
		case 4:
			rgb[0] = n;
			rgb[1] = m;
			rgb[2] = hsv.z;
			break;
		case 5:
			rgb[0] = hsv.z;
			rgb[1] = m;
			rgb[2] = n;
			break;
		}

		return rgb;
	}
}

Cell::Cell(const Cell * parent, Simulation &simulation, const vector2F &position, bool initialize) :
	m_Simulation(simulation),
	m_RenderInstance(simulation.m_RenderController.insert(this)),
	m_PhysicsInstance(simulation.m_PhysicsController.insert(this)),
	m_VMInstance(simulation.m_VMController.insert(this))
{
	uint64 seed[2] = { 0, 0 };

	random::source<random::engine::xorshift_plus> randomg{ m_Simulation.get_hash_seed() };
	auto &random = parent ? parent->getRandom() : randomg;

	for (usize i = 0; i < 2; ++i)
	{
		while (seed[i] == 0)
		{
			seed[i] = random.uniform<uint64>(0, xtd::traits<uint64>::max);
		}
	}

	m_Random.seed(seed[0], seed[1]);
	m_CellID = m_Random.uniform<uint64>(0, xtd::traits<uint64>::max);

	m_VMInstance->m_Cell = this;

	auto getTransformTemp = [](const vector4F &vec) -> matrix4F
	{
		matrix4F objTrans = matrix4F::Identity;
		objTrans[3] = vec;
		return objTrans;
	};

	setRadius(1.0f); // This also happens to set our initial energy.

	Renderer::InstanceData newInstance =
	{
	   getTransformTemp({ float(position.x), float(position.y), 0.0f, 1.0f }),
	   { 1.0f, 1.0f, 1.0f, 1.0f },
	   { 1.0f, 0.0f, 0.0f, 1.0f },
	   { 0.0f, 0.0f, 0.0f, 0.0f },
	   float(m_PhysicsInstance->m_Radius), 0.0f,
	   m_RenderInstance->m_RenderData.CellPtr,
	};
	m_RenderInstance->m_RenderData = newInstance;

	m_PhysicsInstance->m_Position = position;

	if (initialize)
	{
		array<uint64> bytecode = {
			//VM::Instance::OperationBuilder(VM::Instance::OP_SLEEP, 0, VM::Instance::OperandType::Value, uint16(256)),
			VM::Instance::OperationBuilder(VM::Operation::Grow, 0, VM::Instance::OperandType::Value, 0.01f),
			VM::Instance::OperationBuilder(VM::Operation::Sleep, 0, VM::Instance::OperandType::Value, uint16(256)),
			VM::Instance::OperationBuilder(VM::Operation::Split, 0, VM::Instance::OperandType::Value, 0.9f),
		};
		m_VMInstance->set_bytecode(bytecode);
	}
}

Cell::~Cell()
{
	m_Simulation.m_VMController.remove(m_VMInstance);
	m_Simulation.m_PhysicsController.remove(m_PhysicsInstance);
	m_Simulation.m_RenderController.remove(m_RenderInstance);

	xassert(!m_Simulation.m_VMController.contains_cell(this), "VM controller still contains cell");
	xassert(!m_Simulation.m_PhysicsController.contains_cell(this), "Physics controller still contains cell");
	xassert(!m_Simulation.m_RenderController.contains_cell(this), "Render controller still contains cell");
}

float Cell::getRadius() const
{
	return m_PhysicsInstance->m_Radius;
}

float Cell::getShadowRadius() const
{
	return m_PhysicsInstance->m_ShadowRadius;
}

void Cell::setRadius(float radius)
{
	// All of these values are not mathmatically accurate, because frankly it works better without it being such.

	m_PhysicsInstance->m_Radius = radius;
	m_PhysicsInstance->m_ShadowRadius = radius;
	//m_fVolume = (4.0 / 3.0) * Pi * (radius * radius * radius);
	const float radiusSquared = radius * radius;
	const float radiusCubed = radiusSquared * radius;
	m_fVolume = radiusCubed;
	m_fInvVolume = 1.0f / m_fVolume;

	m_fSuperVolume = radiusCubed * radius;
	m_fInvSuperVolume = 1.0f / m_fSuperVolume;

	// We also calculate surface area. This is not simple 2D area - since we use volume internally, this is the surface
	// area of the presumed sphere that the 2d slice represents.
	// NIX THE ABOVE
	// For a variety of reasons, planar circle area is more stable.
	//m_fArea = 4.0 * Pi * (radius * radius);
	//m_fArea = Pi * (radius * radius);
	m_fArea = radiusSquared;
	m_fInvArea = 1.0f / m_fArea;

	// Just a presumed value for now. Take the volume, multiply it by 100000.0, and then round it.
	m_uObjectCapacity = uint64(round(m_fVolume * 200000));
	// Adjust for time mult.
	m_uObjectCapacity = uint64(round(m_uObjectCapacity * options::CapacityMultiplier * options::TimeMultiplier));
	if (getUsedObjectCapacity() > getObjectCapacity())
	{
		uint64 capacityRemainder = getUsedObjectCapacity() - getObjectCapacity();

		xassert(capacityRemainder <= getUsedObjectCapacity(), "capacityRemainder is somehow larger than the sum of all stores");

		// Reduce Energy
		uint64 energyToRemove = min(uint64(m_uEnergy), capacityRemainder);
		m_uEnergy -= energyToRemove;
	}
}

void Cell::setColorHash1(const vector4F &colorHash)
{
	m_ColorHash1 = colorHash;
}

void Cell::update()
{
	const auto * __restrict physicsInstance = m_PhysicsInstance;

	const vector2F & __restrict position = physicsInstance->m_Position;
	//float energyAtPosition = pow(float(m_Simulation.m_GridElements[energyOffset]) / 255.0f, 1.0f / 1.0f);

	// Copy physics info to render info.
	auto &renderData = m_RenderInstance->m_RenderData;
	const auto physicsDirection = physicsInstance->m_Direction;
	renderData.Radius = float(physicsInstance->m_Radius);
	renderData.Transform[0] = vector4F(physicsDirection * vector2F(-1.0f, -1.0f), 0.0f, 0.0f);
	renderData.Transform[1] = vector4F(vector2F(-physicsDirection.y, physicsDirection.x), 0.0f, 0.0f);
	renderData.Transform[3] = vector4F(physicsInstance->m_Position, 0.0f, 1.0f);
	//vector2F velocity = physicsInstance->m_Velocity;
	//velocity.x = clamp(velocity.x, -1.0f, 1.0f);
	//velocity.y = clamp(velocity.y, -1.0f, 1.0f);
	//renderData.TrendVelocity = vector4D(velocity, 0.0f, 0.0f);

	if (m_Alive)
	{
		// TODO : Make work without sticking state into the VM instance.
		//if (m_Simulation.get_renderer()->get_checkbox_state(m_VMInstance->m_LastInstruction))
		//{
		//	m_SelectBrightness = 1.0f;
		//}

    // TODO maybe move this to another update function before physics?
    if (m_MoveState)
    {
      const uint64 energy_cost = uint64((float(options::BaseMoveCost) * m_MoveSpeed + 1.0f) + 0.5f);
      if (energy_cost <= m_uEnergy)
      {
        m_PhysicsInstance->m_Velocity += physicsInstance->m_Direction * m_MoveSpeed * 20.0f;
      }
      m_uEnergy -= energy_cost;
    }

    const float energyFactor = getEnergyFactor();
    if (energyFactor > m_GrowthPoint)
    {
      // Prevent growth from too much touching.
      if (physicsInstance->m_TouchedThisFrame > 3)
      {
        // Do naught
      }
      else
      {

        //if ((double(m_uEnergy) / double(m_uObjectCapacity)) > GrowthThreshold) // removed extra 'touched this frame' check. > 2 is mutex of < 3
        {
          // We want the volume to increase by a fixed amount.
          static const float VolumeGrowth = options::GrowthRate * options::GrowthRateMultiplier;
          float fNewVolume = m_fVolume * (1.0f + VolumeGrowth);

          // Derive a radius from the new volume.
          // cubic_root(volume / ((4/3) * pi))
          //double fNewRadius = pow(fNewVolume / ((4.0 / 3.0) * Pi), 1.0 / 3.0);
          float fNewRadius = pow(fNewVolume, 1.0f / 3.0f);
          fNewRadius = min(fNewRadius, (float)options::MaxCellSize);
          if (physicsInstance->m_Radius != fNewRadius)
          {
            // At this point, we are certainly enlarging.

            // We also reduce energy by growth amount.
            uint64 energyCost = uint64(round((VolumeGrowth / options::GrowthRateMultiplier) * options::BaseGrowCost));
            energyCost = max(energyCost, 1_u64);

            if (energyCost < m_uEnergy)
            {
              setRadius(fNewRadius);
              m_uEnergy -= energyCost;
            }
          }
        }
      }
    }

		if (m_uEnergy != 0)
		{
			if (m_Armor == 0.0f)
			{
				scoped_lock _lock(m_Simulation.m_DestroySerializedLock);
				m_Simulation.m_DestroyTasks += this;
			}
			else
			{
				m_Armor += options::ArmorRegrowthRate;
				m_Armor = clamp(m_Armor, 0.0f, 1.0f);

        const uint energyOffset = m_Simulation.GetLightInstanceOffset(position);

				const float energyAtPosition = m_Simulation.getGreenEnergy(energyOffset);

				float green = max(0.0f, m_ColorGreen);
				float red = max(0.0f, m_ColorRed);
        float blue = max(0.0f, m_ColorBlue);
        if (blue)
				{
					green = 0.0f;
					red = 0.0f;
				}
        if (green)
        {
          red = powf(red, 1.0f / red);
          blue = powf(blue, 1.0f / blue);
        }
        if (red)
        {
          green = powf(green, 1.0f / green);
          blue = powf(blue, 1.0f / blue);
        }

				uint wasteOffset = m_Simulation.GetWasteInstanceOffset(position);
				uint64 wasteAtPosition = m_Simulation.getBlueEnergy(wasteOffset);

				constexpr uint64 maxWasteConsumePerTick = 500;

				float redEnergy = m_Simulation.getRedEnergy(energyOffset);
				//m_RenderInstance->m_RenderData.TrendVelocity.x = clamp(m_RenderInstance->m_RenderData.TrendVelocity.x, -0.0000000000001f, 0.0000000000001f);
				//m_RenderInstance->m_RenderData.TrendVelocity.y = clamp(m_RenderInstance->m_RenderData.TrendVelocity.y, -0.0000000000001f, 0.0000000000001f);

				// TODO optionize
				uint64 wasteEfficiencyLoss = wasteAtPosition / 20000;

				// Handle tasks like energy.
				const float areaEnergyMult = m_fArea * options::BaseEnergyMultiplier;
				uint64 newEnergy = uint64(round(green * areaEnergyMult * energyAtPosition * 1.2f)); // hack to make green better
				newEnergy += uint64(round(red * areaEnergyMult * redEnergy));
				newEnergy -= min(newEnergy, uint64(wasteEfficiencyLoss));

				newEnergy = min(getFreeObjectCapacity() + 1, newEnergy);

				uint64 newEnergySet = (getEnergy() + newEnergy);

				if ((wasteAtPosition != 0) & (newEnergySet < getObjectCapacity()) & (blue > 0.0f))
				{
					uint64 remaining = getObjectCapacity() - newEnergySet;
					// we cannot consume it all, however.
					wasteAtPosition = min(maxWasteConsumePerTick, uint64((double(wasteAtPosition) * blue) + 0.5));
					wasteAtPosition = min(remaining, uint64(wasteAtPosition));
					if (wasteAtPosition)
					{
						m_Simulation.decreaseBlueEnergy(wasteOffset, static_cast<uint32>(wasteAtPosition)); // also arbitrary factor
						newEnergySet += wasteAtPosition / 2u;
					}
				}

				if (newEnergySet != 0)
				{
					newEnergySet--; // basic respiration cost.
				}

				setEnergy(newEnergySet);

				// If we were touched by a lot, we are probably crowded out. Penalize.
				if (physicsInstance->m_TouchedThisFrame)
				{
					if (physicsInstance->m_TouchedThisFrame/* > 2*/)
					{
						float modifier = (float(physicsInstance->m_TouchedThisFrame) / 1000.0f) * options::CrowdingPenalty; // why is the 'max' necessary? This can never be negative.
						uint64 reducer = uint64(getObjectCapacity() * modifier);
						reducer = min(getEnergy(), reducer);
						setEnergy(getEnergy() - reducer);
						// TODO reenable crowding.
					}
					m_TickCollided = true;
				}
				else
				{
					m_TickCollided = false;
				}

				m_Touched += physicsInstance->m_TouchedThisFrame;
				m_Attacked = m_AttackedRemote;

				//if ((double(m_uEnergy) / double(m_uObjectCapacity)) > GrowthThreshold) // removed extra 'touched this frame' check. > 2 is mutex of < 3
				//{
				//   // We want the volume to increase by a fixed amount.
				//   static const double VolumeGrowth = options::GrowthRate * options::GrowthRateMultiplier;
				//   double fNewVolume = m_fVolume * (1.0 + VolumeGrowth);
				//
				//   // Derive a radius from the new volume.
				//   // cubic_root(volume / ((4/3) * pi))
				//   //double fNewRadius = pow(fNewVolume / ((4.0 / 3.0) * Pi), 1.0 / 3.0);
				//   double fNewRadius = pow(fNewVolume, 1.0 / 3.0);
				//   fNewRadius = min(fNewRadius, options::MaxCellSize);
				//   if (physicsInstance->m_Radius != fNewRadius)
				//   {
				//      setRadius(fNewRadius);
				//
				//      // We also reduce energy by growth amount.
				//      // TODO keep track of energy for global energy state.
				//      uint64 newEnergy = uint64(max(double(getEnergy()) * (1.0 - ((VolumeGrowth / options::GrowthRateMultiplier) * 10)), 0.0));
				//      uint64 energyDiff = getEnergy() - newEnergy;
				//      setEnergy(newEnergy);
				//
				//      uint64 newWaste = min(getFreeObjectCapacity(), energyDiff);
				//      setWaste(getWaste() + newWaste);
				//   }
				//}

				m_VMInstance->mutate();
			}
		}
	}
	else
	{
		if (m_Armor == 0.0f)
		{
			scoped_lock _lock(m_Simulation.m_DestroySerializedLock);
			m_Simulation.m_DestroyTasks += this;
		}
		else
		{
			m_Integrity -= 0.0005f;

			if (m_Integrity <= 0.0f)
			{
				scoped_lock _lock(m_Simulation.m_DestroySerializedLock);
				m_Simulation.m_DestroyTasks += this;
			}
		}
	}

	float shade = sqrtf(float(getEnergy()) / float(getObjectCapacity()));

	float health = shade;

	static const color plantGreen = { pow(0.46184f, 2.2f), pow(0.661448f, 2.2f), pow(0.0704501f, 2.2f) };
	static const color plantRed = { pow(0.896282f, 2.2f), pow(0.168297f, 2.2f), pow(0.313112f, 2.2f) };
	static const color plantBlue = { pow(0.195695f, 2.2f), pow(0.289628f, 2.2f), pow(0.696673f, 2.2f) };

	color baseColor = plantGreen * float(m_ColorGreen) + plantRed * float(m_ColorRed) + plantBlue * float(m_ColorBlue);

	m_RenderInstance->m_RenderData.Color2 = { 1.0f - baseColor.r, 1.0f - baseColor.g, 1.0f - baseColor.b, 0.0f };
	m_RenderInstance->m_RenderData.Color2 *= 2.0f;

	m_RenderInstance->m_RenderData.Color2.w = float(xtd::sqrt(health));

	m_RenderInstance->m_RenderData.Armor_NucleusScale.x = float(m_Armor);
	//m_RenderInstance->m_RenderData.Armor_NucleusScale.y = float(physicsDirection.x);
	//m_RenderInstance->m_RenderData.Armor_NucleusScale.z = float(physicsDirection.y);
	auto lerp = [](float x, float y, float s) -> float
	{
		return x*(1.0f - s) + y*s;
	};
	m_RenderInstance->m_RenderData.Armor_NucleusScale.y = float(lerp(0.75f, 1.5f, xtd::min((float(m_VMInstance->m_ByteCode.size()) / float(options::BaselineBytecodeSize * 5)), 1.0f)));

	//baseColor *= float(shade);
	baseColor.a = float(m_Integrity);

	switch (m_Simulation.get_renderer()->getRenderMode())
	{
	case Renderer::RenderMode::HashCode:
		baseColor = hue_to_rgb(m_ColorHash1); break;
	case Renderer::RenderMode::Dye:
		baseColor = hue_to_rgb(m_ColorDye); break;
	}

	baseColor += {m_SelectBrightness, m_SelectBrightness, m_SelectBrightness, m_SelectBrightness};

	m_SelectBrightness -= SelectBrightnessDecay;
	m_SelectBrightness = max(m_SelectBrightness, 0.0f);
	m_RenderInstance->m_RenderData.Color1 = baseColor;
}

void Cell::update_lite()
{
	// TODO : Make work without sticking state into the VM instance.
	//if (m_Alive & m_Simulation.get_renderer()->get_checkbox_state(m_VMInstance->m_LastInstruction))
	//{
	//	m_SelectBrightness = 1.0f;
	//}

	float shade = sqrtf(float(getEnergy()) / float(getObjectCapacity()));

	float health = shade;

	static const color plantGreen = { pow(0.46184f, 2.2f), pow(0.661448f, 2.2f), pow(0.0704501f, 2.2f) };
	static const color plantRed = { pow(0.896282f, 2.2f), pow(0.168297f, 2.2f), pow(0.313112f, 2.2f) };
	static const color plantBlue = { pow(0.195695f, 2.2f), pow(0.289628f, 2.2f), pow(0.696673f, 2.2f) };

	color baseColor = plantGreen * float(m_ColorGreen) + plantRed * float(m_ColorRed) + plantBlue * float(m_ColorBlue);

	m_RenderInstance->m_RenderData.Color2 = { 1.0f - baseColor.r, 1.0f - baseColor.g, 1.0f - baseColor.b, 0.0f };
	m_RenderInstance->m_RenderData.Color2 *= 2.0f;

	m_RenderInstance->m_RenderData.Color2.w = float(xtd::sqrt(health));

	m_RenderInstance->m_RenderData.Armor_NucleusScale.x = float(m_Armor);
	const auto lerp = [](float x, float y, float s) -> float
	{
		return x*(1.0f - s) + y*s;
	};
	m_RenderInstance->m_RenderData.Armor_NucleusScale.y = float(lerp(0.75f, 1.5f, xtd::min((float(m_VMInstance->m_ByteCode.size()) / float(options::BaselineBytecodeSize * 5)), 1.0f)));

	switch (m_Simulation.get_renderer()->getRenderMode())
	{
	case Renderer::RenderMode::HashCode:
		baseColor = hue_to_rgb(m_ColorHash1);
		break;
	case Renderer::RenderMode::Dye:
		baseColor = hue_to_rgb(m_ColorDye);
		break;
	default: [[likely]] {
			break;
		}
	}

	baseColor.a = float(m_Integrity);

	baseColor += {m_SelectBrightness, m_SelectBrightness, m_SelectBrightness, m_SelectBrightness};

	m_SelectBrightness -= SelectBrightnessDecay;
	m_SelectBrightness = max(m_SelectBrightness, 0.0f);
	m_RenderInstance->m_RenderData.Color1 = baseColor;
}

void Cell::unserialize(Stream &inStream)
{
	inStream.read(m_Random);
	inStream.read(m_CellID);
	inStream.read(m_fVolume);
	inStream.read(m_fInvVolume);
	inStream.read(m_fSuperVolume);
	inStream.read(m_fInvSuperVolume);
	inStream.read(m_fArea);
	inStream.read(m_fInvArea);
	inStream.read(m_ColorGreen);
	inStream.read(m_ColorRed);
	inStream.read(m_ColorBlue);
	inStream.read(m_Touched);
	inStream.read(m_Attacked);
	inStream.read(m_AttackedRemote);
	inStream.read(m_uObjectCapacity);
	inStream.read(m_uEnergy);
	inStream.read(m_Integrity);
	inStream.read(m_Armor);
	inStream.read(m_CellIdx);
	inStream.read(m_TickCollided);
	inStream.read(m_Alive);
	inStream.read(m_ColorHash1);
	inStream.read(m_ColorDye);

	xassert(m_PhysicsInstance, "Cell doesn't have a physics instance.");
	xassert(m_VMInstance, "Cell doesn't have a VM instance.");
	xassert(m_RenderInstance, "Cell doesn't have a Render instance.");

	m_PhysicsInstance->unserialize(inStream, this);
	m_VMInstance->unserialize(inStream, this);
	m_RenderInstance->unserialize(inStream, this);
}

void Cell::serialize(Stream &outStream) const
{
	outStream.write(m_Random);
	outStream.write(m_CellID);
	outStream.write(m_fVolume);
	outStream.write(m_fInvVolume);
	outStream.write(m_fSuperVolume);
	outStream.write(m_fInvSuperVolume);
	outStream.write(m_fArea);
	outStream.write(m_fInvArea);
	outStream.write(m_ColorGreen);
	outStream.write(m_ColorRed);
	outStream.write(m_ColorBlue);
	outStream.write(m_Touched);
	outStream.write(m_Attacked);
	outStream.write(m_AttackedRemote);
	outStream.write(m_uObjectCapacity);
	outStream.write(m_uEnergy);
	outStream.write(m_Integrity);
	outStream.write(m_Armor);
	outStream.write(m_CellIdx);
	outStream.write(m_TickCollided);
	outStream.write(m_Alive);
	outStream.write(m_ColorHash1);
	outStream.write(m_ColorDye);

	xassert(m_PhysicsInstance, "Cell doesn't have a physics instance.");
	xassert(m_VMInstance, "Cell doesn't have a VM instance.");
	xassert(m_RenderInstance, "Cell doesn't have a Render instance.");

	m_PhysicsInstance->serialize(outStream);
	m_VMInstance->serialize(outStream);
	m_RenderInstance->serialize(outStream);
}
