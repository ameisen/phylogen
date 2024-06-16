#include "phylogen.hpp"
#include "PhysicsController.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;
using namespace phylo::Physics;

// until we have a real wide_array implementation, we need to presize it.
static constexpr const usize WideArraySize = 5'000'000ull;

namespace
{
	static vector2F ClampPosition(const vector2F& __restrict position, float radius)
	{
		vector2F out = position;

		if (out.length() > options::WorldRadius - radius) [[unlikely]]
		{
			out = out.normalize(options::WorldRadius - radius);
		}

		return out;
	}
}

Controller::Controller(Simulation& simulation) :
	m_Simulation(simulation),
	m_ThreadPool("Physics", [this](usize threadID) {pool_update(threadID); }, false),
	m_ThreadPool2("Physics 2", [this](usize threadID) {pool_update2(threadID); }, false)
{
	// Calculate the grid width/height. Should be the same.
	m_GridElementsEdge = uint32(((double(options::WorldRadius) * 2.0) / double(options::MedianCellSize)) + 0.5);
	m_GridElementSize = float((double(options::WorldRadius) * 2.0) / double(m_GridElementsEdge));
	m_GridElementSizeHalf = m_GridElementSize * 0.5f;
	m_InvGridElementSize = 1.0f / m_GridElementSize;
	m_GridElements.resize((m_GridElementsEdge * m_GridElementsEdge) + 1); // we stick new elements in the last one.
}

Controller::~Controller() = default;

vector2F Controller::GetGridOffset(uint gridElement) const __restrict
{
	uint32 x, y;
	xtd::morton2d<uint>(gridElement).get_offsets(x, y);

	return{
	   ((float(x) * m_GridElementSize) + (m_GridElementSize * 0.5f)) - options::WorldRadius,
	   ((float(y) * m_GridElementSize) + (m_GridElementSize * 0.5f)) - options::WorldRadius
	};
}

vector2F Controller::GetGridXRange(uint gridElement) const __restrict
{
	uint32 x, y;
	xtd::morton2d<uint>(gridElement).get_offsets(x, y);

	float xPos = (float(x) * m_GridElementSize) - options::WorldRadius;

	return{
	   xPos,
	   xPos + m_GridElementSize
	};
}

vector2F Controller::GetGridXRangeFromX(uint gridX) const __restrict
{
	float xPos = (float(gridX) * m_GridElementSize) - options::WorldRadius;

	return{
	   xPos,
	   xPos + m_GridElementSize
	};
}

vector2F Controller::GetGridYRangeFromY(uint gridY) const __restrict
{
	float yPos = (float(gridY) * m_GridElementSize) - options::WorldRadius;

	return{
	   yPos,
	   yPos + m_GridElementSize
	};
}

uint32 Controller::GetInstanceOffset(const instance_t& __restrict instance) const __restrict
{
	uint32 xcoord = uint32((instance.m_Position.x + options::WorldRadius) * m_InvGridElementSize);
	uint32 ycoord = uint32((instance.m_Position.y + options::WorldRadius) * m_InvGridElementSize);

	return xtd::morton2d<uint>(xcoord, ycoord);

	//uint32 coord = (ycoord * m_GridElementsEdge) + xcoord;

	//return coord;
}

// Error - this is returning an out of range index.
uint32 Controller::GetPositionOffset(const vector2F& __restrict position) const __restrict
{
	uint32 xcoord = uint32((position.x + options::WorldRadius) * m_InvGridElementSize);
	uint32 ycoord = uint32((position.y + options::WorldRadius) * m_InvGridElementSize);

	return xtd::morton2d<uint>(xcoord, ycoord);

	//uint32 coord = (ycoord * m_GridElementsEdge) + xcoord;

	//return coord;
}

uint32 Controller::GetInstanceOffset(uint x, uint y) const __restrict
{
	return xtd::morton2d<uint>(x, y);

	//uint32 coord = (y * m_GridElementsEdge) + x;

	//return coord;
}

void Controller::removedInstance(instance_t* __restrict instance) __restrict
{
	m_GridElements[instance->m_GridArrayIndex].removeElement(instance); // It was in another sub-array.
	instance->m_GridArrayIndex = (m_GridElements.size() - 1);
}

void Controller::insertedInstance(instance_t* __restrict instance) __restrict
{
	instance->m_GridArrayIndex = m_GridElements.size() - 1;
	m_GridElements.back().addElement(instance);
}

void Controller::pool_update(usize) __restrict
{
	const uint numInstances = m_Instances.size();

	for (;;)
	{
		// How many instances each thread is going to consume per loop.
		// If this is too low, cache locality goes down, and it spends too much time
		// on a concurrent access to m_ThreadPoolIndex.
		// If this is too high, parallelism suffers.
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numInstances);
		for (; uIdx < finalIdx; ++uIdx)
		{
			Instance& __restrict instance = m_Instances[uIdx];

			// If the instance is invalid, just skip it.
			// This happens when an instance is removed from the global list, but no new instance has populated it.
			// This happens because the instance list is stable - once an element is in, it stays at exactly that address.
			if (!instance.m_Valid) [[unlikely]]
			{
				continue;
			}

			const float speedSq = instance.m_Velocity.length_sq();
			const float radiusAdj = instance.m_Radius * 10.0f;

			xassert(speedSq == speedSq, "nan");

			xassert(instance.m_Velocity == instance.m_Velocity, "nan");
			xassert(radiusAdj == radiusAdj, "nan");
			xassert(!isinf(instance.m_Velocity.x), "nan");
			xassert(!isinf(radiusAdj), "nan");


			xassert(instance.m_Velocity == instance.m_Velocity, "nan");
			xassert(radiusAdj == radiusAdj, "nan");
			xassert(!isinf(instance.m_Velocity.x), "nan");
			xassert(!isinf(radiusAdj), "nan");

			// If the speed of the cell is greater than the radius of the cell, clamp it, otherwise it will just jump over collisions.
			float velocityAdjCheck = (radiusAdj * (instance.m_Radius * instance.m_Radius * instance.m_Radius)) / 0.001f;
			if (speedSq > (velocityAdjCheck * velocityAdjCheck))
			{
				instance.m_Velocity = instance.m_Velocity.normalize(velocityAdjCheck);
			}

			// Apply velocity using stupid math.
			auto adjustedVelocity = (instance.m_Velocity / (instance.m_Radius * instance.m_Radius * instance.m_Radius)) * 0.001f;

			// Apply velocity using stupid math.
			instance.m_Position += adjustedVelocity;

			// Make sure the cell stays within the world radius.
			instance.m_Position = ClampPosition(instance.m_Position, instance.m_Radius);

			xassert(instance.m_Position == instance.m_Position, "nan");

			// Get the instance's grid index.
			uint32 GridIndex = GetInstanceOffset(instance);

			if (GridIndex != instance.m_GridArrayIndex) // If the grid index has changed, handle that.
			{
				// These operations enforce strict ordering on the grid elements, so that determinism is maintained.
				m_GridElements[instance.m_GridArrayIndex].removeElement(&instance); // It was in another sub-array.
				m_GridElements[GridIndex].addElement(&instance);

				instance.m_GridArrayIndex = GridIndex; // Set the new index.
			}

			// Drag
			instance.m_Velocity *= 0.9f;

			instance.m_ShadowRadius = instance.m_Radius;
			instance.m_ShadowPosition = instance.m_Position;
			instance.m_ShadowVelocity = instance.m_Velocity;
		}
		if (finalIdx == numInstances)
		{
			return;
		}
	}
}

void Controller::pool_update2(usize) __restrict
{
	// Really stupid collision detection. Much improvement obviously needed.

	const uint numInstances = m_Instances.size();

	for (;;)
	{
		// How many instances each thread is going to consume per loop.
		// If this is too low, cache locality goes down, and it spends too much time
		// on a concurrent access to m_ThreadPoolIndex.
		// If this is too high, parallelism suffers.
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numInstances);
		for (; uIdx < finalIdx; ++uIdx)
		{
			Instance& __restrict instance = m_Instances[uIdx];
			if (!instance.m_Valid)
			{
				continue;
			}
			xassert(instance.m_Radius > 0.0f, "radius is 0");

			uint touchedThisFrame = 0;
			vector2F velocity = vector2F(0.0f, 0.0f);
			const vector2F instanceVelocity = instance.m_Velocity;
			const float instanceSpeedSquared = instanceVelocity.length_sq();
			const bool instanceSpeedNZero = instanceSpeedSquared > 0.00000001f;
			xassert(instanceVelocity == instanceVelocity, "nan");
			const float instanceRadius = instance.m_Radius;
			const float instanceMass = instanceRadius * instanceRadius * instanceRadius;

			vector2F thisPosition = instance.m_Position;
			xassert(thisPosition == thisPosition, "nan");

			const auto cellId = instance.m_Cell->getCellID();

			// Precalculate a range for AABB tests.
			const vector2F xRange = { thisPosition.x - instance.m_Radius, thisPosition.x + instance.m_Radius };
			const vector2F yRange = { thisPosition.y - instance.m_Radius, thisPosition.y + instance.m_Radius };

			// AABB test function.
			const auto testRange = [](const vector2F& __restrict range1, const vector2F& __restrict range2) -> uint
			{
				return (uint(range1.x <= range2.y) & uint(range2.x <= range1.y));
			};

			const auto& gridElements = m_GridElements[instance.m_GridArrayIndex];

			{
				const auto testCommand = [&](const instance_t* testInstance)
				{
					// AABB is actually slower most of the time.
					//const vector2F testXRange = { testInstance->m_Position.x - testInstance->m_Radius, testInstance->m_Position.x + testInstance->m_Radius };
					//const vector2F testYRange = { testInstance->m_Position.y - testInstance->m_Radius, testInstance->m_Position.y + testInstance->m_Radius };
					//if (!(testRange(xRange, testXRange) & testRange(yRange, testYRange)))
					//{
					//   return;
					//}

					// Check if the two circles overlap.
					vector2F subDistance = (thisPosition - testInstance->m_Position);
					xassert(subDistance == subDistance, "nan");
					float distSq = subDistance.dot(subDistance);
					float radiusSq = (instance.m_Radius + testInstance->m_Radius);
					radiusSq *= radiusSq;
					if (distSq < radiusSq)
					{
						// We are intersecting/overlapping in some fashion.
						// Generate a force to separate them.

						float overlapScale = sqrtf(1.0f - (distSq / radiusSq));
						//overlapScale *= overlapScale * overlapScale;
						// Use this as a force to apply an impulse away.

						const auto getRandomDirection = [&instance]() -> vector2F {
							float radians = instance.m_Cell->getRandom().uniform<float>(0.0f, 2.0f * xtd::pi<float>);
							return { cos(radians), sin(radians) };
							};

						const float directionScale = overlapScale * 10.0f * instance.m_Radius;

						const bool subDistanceNZero = (distSq != 0.0f);

						const vector2F subDistanceNormalized = subDistance.normalize();

						vector2F directionAway = (subDistanceNZero) ?
							(subDistanceNormalized * directionScale) :
							(getRandomDirection() * directionScale);

						const float testInstanceRadius = testInstance->m_Radius;
						//const float testInstanceMass = testInstanceRadius * testInstanceRadius * testInstanceRadius;
						const float invMassRatio = (testInstanceRadius / instance.m_Radius);
						const float massRatio = (instance.m_Radius / testInstanceRadius);

						velocity += directionAway;// *massRatio;
						xassert(velocity == velocity, "nan");

						if (overlapScale > 0.5f) {
							++touchedThisFrame;
						}

						const auto testInstanceVelocity = testInstance->m_ShadowVelocity;
						const float testInstanceSpeedSquared = testInstanceVelocity.length_sq();
						const bool testInstanceSpeedNZero = testInstanceSpeedSquared > 0.00000001f;

						if (subDistanceNZero)
						{
							constexpr float elasticity = 0.1f;
							if (testInstanceSpeedNZero)
							{
								// Not accurate, but close enough.
								const float normalVelocityDot = testInstanceVelocity.normalize().dot(subDistanceNormalized);
								if (normalVelocityDot > 0.0f)
								{
									const vector2F relativizedSpeed = testInstanceVelocity - instanceVelocity;
									const vector2F targetVelocity = subDistanceNormalized * (relativizedSpeed.length()) * elasticity;

									//velocity -= workComponentVelocity * invMassRatio * 20000.0;

									// This math is wrong. Collisions can only be fully elastic if both objects are the same mass; otherwise,
									// the more massive object will only transfer as much energy as is an inverse ratio to their mass. This prevents 
									// the tiny little cells from shooting everywhere.
									// TODO.
									// Thus - we are _trying_ to get ourselves to 'targetVelocity' (and vice-versa below). However, we need to scale
									// the energy by the mass ratio - 1 m/s delta to an object that's twice as massive as you takes 2 m/s from you.

									const vector2F velocityDifference = targetVelocity;
									// How much energy do they actually have to spare?
									vector2F instanceEnergyRel = relativizedSpeed * massRatio;
									if ((velocityDifference.x * instanceEnergyRel.x) < 0.0f)
									{
										instanceEnergyRel.x = 0.0f;
									}
									if ((velocityDifference.y * instanceEnergyRel.y) < 0.0f)
									{
										instanceEnergyRel.y = 0.0f;
									}
									const vector2F sign = { velocityDifference.x >= 0 ? 1.0f : -1.0f, velocityDifference.y >= 0 ? 1.0f : -1.0f };
									const vector2F velocityOffset = {
										sign.x * xtd::min(xtd::abs(velocityDifference.x), xtd::abs(instanceEnergyRel.x)),
										sign.y * xtd::min(xtd::abs(velocityDifference.y), xtd::abs(instanceEnergyRel.y))
									};

									velocity += velocityOffset;

									xassert(velocity == velocity, "nan");
								}
							}
							if (instanceSpeedNZero)
							{
								// Not accurate, but close enough.
								const float normalVelocityDot = instanceVelocity.normalize().dot(-subDistanceNormalized);
								if (normalVelocityDot > 0.0f)
								{
									const vector2F relativizedSpeed = instanceVelocity - testInstanceVelocity;
									const vector2F targetVelocity = subDistanceNormalized * (relativizedSpeed.length()) * elasticity;

									//velocity -= workComponentVelocity * invMassRatio * 20000.0;

									// This math is wrong. Collisions can only be fully elastic if both objects are the same mass; otherwise,
									// the more massive object will only transfer as much energy as is an inverse ratio to their mass. This prevents 
									// the tiny little cells from shooting everywhere.
									// TODO.
									// Thus - we are _trying_ to get ourselves to 'targetVelocity' (and vice-versa below). However, we need to scale
									// the energy by the mass ratio - 1 m/s delta to an object that's twice as massive as you takes 2 m/s from you.

									const vector2F velocityDifference = targetVelocity;
									// How much energy do they actually have to spare?
									vector2F instanceEnergyRel = relativizedSpeed * massRatio;
									if ((velocityDifference.x * instanceEnergyRel.x) < 0.0f)
									{
										instanceEnergyRel.x = 0.0f;
									}
									if ((velocityDifference.y * instanceEnergyRel.y) < 0.0f)
									{
										instanceEnergyRel.y = 0.0f;
									}
									const vector2F sign = { velocityDifference.x >= 0 ? 1.0f : -1.0f, velocityDifference.y >= 0 ? 1.0f : -1.0f };
									const vector2F velocityOffset = {
										sign.x * xtd::min(xtd::abs(velocityDifference.x), xtd::abs(instanceEnergyRel.x)),
										sign.y * xtd::min(xtd::abs(velocityDifference.y), xtd::abs(instanceEnergyRel.y))
									};

									velocity += velocityOffset * invMassRatio;

									xassert(velocity == velocity, "nan");
								}
							}
						}
					}
				};

				// Test against the current grid instance. Splitting the test into two loops allows us to
				// avoid requiring a condition check for the current instance.

				for (uint elem = 0; elem < instance.m_GridIndex; ++elem)
				{
					const auto* __restrict testInstance = gridElements.m_Elements[elem];

					testCommand(testInstance);
				}

				const uint sz = gridElements.m_ElementCount;
				for (uint elem = instance.m_GridIndex + 1; elem < sz; ++elem)
				{
					const auto* __restrict testInstance = gridElements.m_Elements[elem];

					testCommand(testInstance);
				}

				// Extract X and Y.
				//uint32 x = instance.m_GridArrayIndex % m_GridElementsEdge;
				//uint32 y = instance.m_GridArrayIndex / m_GridElementsEdge;

				uint32 x, y;
				xtd::morton2d<uint>(instance.m_GridArrayIndex).get_offsets(x, y);

				// Build a set of grid arrays to scan.
				uint GridSize = 0; // two by default.
				xtd::array<const InstanceSubArray<GridArraySize>*, 8> GridArray;

				uint ym1 = testRange(yRange, GetGridYRangeFromY(y - 1));
				uint yp1 = testRange(yRange, GetGridYRangeFromY(y + 1));

				// Figure out which tiles the cell actually touches, so we can only check cells in this tiles.

				if (x != 0 && testRange(xRange, GetGridXRangeFromX(x - 1)))
				{
					// We can insert elements behind.
					GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y)];
					if ((y != 0) & ym1)
					{
						GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y - 1)];
					}
					if ((y != m_GridElementsEdge - 1) & yp1)
					{
						GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y + 1)];
					}
				}
				if (x != m_GridElementsEdge - 1 && testRange(xRange, GetGridXRangeFromX(x + 1)))
				{
					// We can insert elements ahead.
					GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y)];
					if ((y != 0) & ym1)
					{
						GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y - 1)];
					}
					if ((y != m_GridElementsEdge - 1) & yp1)
					{
						GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y + 1)];
					}
				}
				if ((y != 0) & ym1)
				{
					GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x, y - 1)];
				}
				if ((y != m_GridElementsEdge - 1) & yp1)
				{
					GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x, y + 1)];
				}

				// Check for overlaps in each of those tiles.

				for (uint i = 0; i < GridSize; ++i)
				{
					const auto* __restrict element = GridArray[i];

					const uint sz = element->m_ElementCount;
					for (uint elem = 0; elem < sz; ++elem)
					{
						const auto* __restrict testInstance = element->m_Elements[elem];

						testCommand(testInstance);
					}
				}

				instance.m_TouchedThisFrame = touchedThisFrame;
				instance.m_Velocity += velocity;
			}
		}
		if (finalIdx == numInstances)
		{
			return;
		}
	}
}

void Controller::update()
{
	clock::time_point subTime = clock::get_current_time();
	m_ThreadPoolIndex = 0ull;
	m_ThreadPool.kickoff();
	m_ThreadPoolIndex = 0ull;
	m_ThreadPool2.kickoff();
	m_Simulation.m_TotalParallelTime += clock::get_current_time() - subTime;

	// handle deltas.
	// TODO this _can_ be threaded, but it needs to be threaded per instance.
	/*
	if (m_VelocityDeltas.size())
	{
	   m_VelocityDeltas.sort();
	   for (const auto &delta : m_VelocityDeltas)
	   {
		  delta.m_Target->m_Velocity += delta.Delta;
	   }
	   m_VelocityDeltas.clear();
	}
	*/
}

Cell* Controller::findCell(const vector2F& __restrict position, float radius, const Cell* __restrict filter) const __restrict
{
	const vector2F thisPosition = ClampPosition(position, radius);
	float testRadius = radius;

	const vector2F xRange = { thisPosition.x - testRadius, thisPosition.x + testRadius };
	const vector2F yRange = { thisPosition.y - testRadius, thisPosition.y + testRadius };

	uint32 GridIndex = GetPositionOffset(thisPosition);

	const auto testRange = [](const vector2F& __restrict range1, const vector2F& __restrict range2) -> uint
	{
		return (uint(range1.x <= range2.y) & uint(range2.x <= range1.y));
	};

	const auto& __restrict gridElements = m_GridElements[GridIndex];

	const auto testCommand = [&](const instance_t* testInstanceNR) -> bool
	{
		// AABB is actually slower most of the time.
		//const vector2F testXRange = { testInstance->m_Position.x - testInstance->m_Radius, testInstance->m_Position.x + testInstance->m_Radius };
		//const vector2F testYRange = { testInstance->m_Position.y - testInstance->m_Radius, testInstance->m_Position.y + testInstance->m_Radius };
		//if (!(testRange(xRange, testXRange) & testRange(yRange, testYRange)))
		//{
		//   return;
		//}

		if (testInstanceNR->m_Cell == filter)
		{
			return false;
		}

		const instance_t* __restrict testInstance = testInstanceNR;

		vector2F subDistance = (thisPosition - testInstance->m_ShadowPosition);
		float distSq = subDistance.dot(subDistance);
		float radiusSq = (testRadius + testInstance->m_ShadowRadius);
		radiusSq *= radiusSq;
		return (distSq < radiusSq);
	};

	uint sz = gridElements.m_ElementCount;
	for (uint elem = 0; elem < sz; ++elem)
	{
		const auto* __restrict testInstance = gridElements.m_Elements[elem];

		if (testCommand(testInstance))
		{
			return testInstance->m_Cell;
		}
	}

	// Extract X and Y.
	//uint32 x = GridIndex % m_GridElementsEdge;
	//uint32 y = GridIndex / m_GridElementsEdge;

	uint32 x, y;
	xtd::morton2d<uint>(GridIndex).get_offsets(x, y);

	// Build a set of grid arrays to scan.
	uint GridSize = 0; // two by default.
	xtd::array<const InstanceSubArray<GridArraySize>*, 8> GridArray;

	uint ym1 = testRange(yRange, GetGridYRangeFromY(y - 1));
	uint yp1 = testRange(yRange, GetGridYRangeFromY(y + 1));

	if (x != 0 && testRange(xRange, GetGridXRangeFromX(x - 1)))
	{
		// We can insert elements behind.
		GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y)];
		if ((y != 0) & ym1)
		{
			GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y - 1)];
		}
		if ((y != m_GridElementsEdge - 1) & yp1)
		{
			GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x - 1, y + 1)];
		}
	}
	if (x != m_GridElementsEdge - 1 && testRange(xRange, GetGridXRangeFromX(x + 1)))
	{
		// We can insert elements ahead.
		GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y)];
		if ((y != 0) & ym1)
		{
			GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y - 1)];
		}
		if ((y != m_GridElementsEdge - 1) & yp1)
		{
			GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x + 1, y + 1)];
		}
	}
	if ((y != 0) & ym1)
	{
		GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x, y - 1)];
	}
	if ((y != m_GridElementsEdge - 1) & yp1)
	{
		GridArray[GridSize++] = &m_GridElements[GetInstanceOffset(x, y + 1)];
	}

	for (uint i = 0; i < GridSize; ++i)
	{
		const auto* __restrict element = GridArray[i];

		uint sz = element->m_ElementCount;
		for (uint elem = 0; elem < sz; ++elem)
		{
			const auto* __restrict testInstance = element->m_Elements[elem];

			if (testCommand(testInstance))
			{
				return testInstance->m_Cell;
			}
		}
	}

	return nullptr;
}
