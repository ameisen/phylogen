#include "Phylogen.hpp"
#include "Simulation.hpp"

#include <noise.h>

namespace phylo::nouns
{
#include "Words/nouns.txt.hpp"
}

namespace phylo::adjectives
{
#include "Words/adjectives.txt.hpp"
}

using namespace phylo;

static constexpr usize WideArraySize = 5000000ull;

static constexpr float MedianSimCellSize = 5.0f;
static constexpr float MedianLightCellSize = 5.0f;
static constexpr float WorldRadius = options::WorldRadius;

static constexpr float MulFactor = 0.01f;
#define DYNAMIC_LIGHTS 0

string getRandomName()
{
	const int64 timeValue = int64(clock::get_current_time() - clock::time_point(0));
	random::source<random::engine::xorshift_plus> rand{ uint64(timeValue) };

	const usize adjectiveCount = sizeof(adjectives::words) / sizeof(const char *);
	const usize nounCount = sizeof(nouns::words) / sizeof(const char *);

	const char *adj = adjectives::words[rand.uniform<usize>(0, adjectiveCount - 1)];
	const char *noun = nouns::words[rand.uniform<usize>(0, nounCount - 1)];

	return string(adj) + " " + noun;
}

void Simulation::GridProperties::init() 
{
	m_GridElementsPositions.resize(m_GridElementsEdge * m_GridElementsEdge);

	for (uint i = 0; i < m_GridElementsPositions.size(); ++i)
	{
		uint _x = i % m_GridElementsEdge;
		uint _y = i / m_GridElementsEdge;

		vector2F offset = { float(_x), float(_y) };
		offset = (((offset / float(m_GridElementsEdge)) * 2.0f - vector2F{ 1.0f }) * WorldRadius) + vector2F{ m_GridElementSizeHalf };

		m_GridElementsPositions[i] = offset;
	}
}

Cell *Simulation::getNewCellPtr() 
{
	uptr nextCell = *(uptr * )m_NextFreeCell;
	Cell *ret = (Cell * )m_NextFreeCell;
	m_NextFreeCell = (uint8 * )nextCell;
	xassert(nextCell != 0, "out of cells to allocate");
	return ret;
}

void Simulation::freeCellPtr(Cell *cell) 
{
	*(uptr * )cell = uptr(m_NextFreeCell);
	m_NextFreeCell = (uint8 * )cell;
}

Simulation::Simulation(event &startProcessing, event &waitThreadProcessing, const loadInitializer &init) :
	System(),
	m_ThreadPool("Simulation", [this](usize) {pool_update(); }, false),
	m_ThreadPool2("Simulation2", [this](usize) {pool_update2(); }, false),
	m_ThreadPool3("Simulation3", [this](usize) {pool_updatelite(); }, false),
	m_ThreadPool4("Simulation4", [this](usize) {pool_update_waste(); }, false),
	m_PhysicsController(*this),
	m_VMController(*this),
	m_RenderController(*this),
	m_HashName(init.m_HashName),
	m_HashSeed(xtd::security::hash::fnv<uint64>(init.m_HashName))
{
	m_CellStore = new uint8[sizeof(Cell) * MaxNumCells];
	m_NextFreeCell = &m_CellStore[0];

	for (uint i = 0; i < MaxNumCells; ++i)
	{
		uptr *curCellPtr = (uptr *)&m_CellStore[i * sizeof(Cell)];

		*curCellPtr = uptr(&m_CellStore[(i + 1) * sizeof(Cell)]);
	}
	*(uptr *)&m_CellStore[(MaxNumCells - 1) * sizeof(Cell)] = uptr(0);


	// Initialize the store.
	m_LightmapZ = init.m_LightmapZ;
	m_CurProcessRow = init.m_CurProcessRow;
	m_uCurrentFrame = init.m_uCurrentFrame;

	// Generate sim grid
	{
		m_SimGrid.m_GridElementsEdge = init.m_SimGridElementsEdge;
		m_SimGrid.m_GridElementSize = float((float(WorldRadius) * 2.0f) / float(m_LightGrid.m_GridElementsEdge));
		m_SimGrid.m_GridElementSizeHalf = m_LightGrid.m_GridElementSize * 0.5f;
		m_SimGrid.m_InvGridElementSize = 1.0f / m_LightGrid.m_GridElementSize;
		m_SimGrid.init();
	}

	{
		// Generate light array.
		m_LightGrid.m_GridElementsEdge = init.m_LightGridElementsEdge;
		m_LightGrid.m_GridElementSize = float((float(WorldRadius) * 2.0f) / float(m_LightGrid.m_GridElementsEdge));
		m_LightGrid.m_GridElementSizeHalf = m_LightGrid.m_GridElementSize * 0.5f;
		m_LightGrid.m_InvGridElementSize = 1.0f / m_LightGrid.m_GridElementSize;
		m_LightGrid.init();

		m_NoisePipeline.setSeed(init.m_NoiseSeed);
		m_pNoiseCache = m_NoisePipeline.createCache();
		m_NoiseSrc = m_NoisePipeline.getElement(m_PerlinModule.addToPipe(m_NoisePipeline));

		// Initialize light values
		for (uint i = 0; i < m_LightGrid.m_GridElements.size(); ++i)
		{
			uint8 &intensity = m_LightGrid.m_GridElements[i];
			vector2F position = m_LightGrid.m_GridElementsPositions[i];
			position *= MulFactor;

			float value = clamp(float(m_NoiseSrc->getValue(position.x, position.y, m_LightmapZ, m_pNoiseCache) + 1.0f) * 0.5f, 0.0f, 1.0f);
			value = std::sqrtf(value);
			//value = value * value;

			intensity = uint8(value * 255.5);
		}
	}

	{
		// Initialize waste array
		m_WasteGrid.m_GridElementsEdge = init.m_WasteGridElementsEdge;
		m_WasteGrid.m_GridElementSize = float((float(WorldRadius) * 2.0f) / float(m_WasteGrid.m_GridElementsEdge));
		m_WasteGrid.m_GridElementSizeHalf = m_WasteGrid.m_GridElementSize * 0.5f;
		m_WasteGrid.m_InvGridElementSize = 1.0f / m_WasteGrid.m_GridElementSize;
		m_WasteGrid.init();

		// Initialize waste values
		for (uint i = 0; i < m_WasteGrid.m_GridElements.size(); ++i)
		{
			uint32 &intensity = m_WasteGrid.m_GridElements[i];
			intensity = 0;
		}
	}

	m_Cells.reserve(WideArraySize);

	// Kick off the sim thread.
	if (!m_SimThread.started())
	{
		event waitForThreadStart;
		m_SimThread = [this, &startProcessing, &waitThreadProcessing, &waitForThreadStart] {
			waitForThreadStart.set();
			if (&startProcessing != nullptr)
			{
				startProcessing.join();
			}
			if (&waitThreadProcessing != nullptr)
			{
				waitThreadProcessing.set();
			}

			sim_loop();
		};
		m_SimThread.set_name("Sim Thread");
		m_SimThread.start();

		waitForThreadStart.join();
	}

	//
}

Simulation::Simulation(const string &hashName, event &startProcessing, event &waitThreadProcessing) : Simulation(
	startProcessing, waitThreadProcessing,
	{
	   hashName,
	   1_u64,
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianSimCellSize)) + 0.5f)),
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianLightCellSize)) + 0.5f)),
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianLightCellSize)) + 0.5f)),
	   -1000000.0f,
	   0_u32,
	   static_cast<int>(xtd::security::hash::fnv<uint32>(hashName))
	}
)
{
}

Simulation::Simulation(const string &hashName) : Simulation(
	*(event * )nullptr, *(event * )nullptr,
	{
	   hashName,
	   1,
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianSimCellSize)) + 0.5f)),
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianLightCellSize)) + 0.5f)),
	   (uint32(((float(WorldRadius) * 2.0f) / float(MedianLightCellSize)) + 0.5f)),
	   -1000000.0f,
	   0,
	   static_cast<int>(xtd::security::hash::fnv<uint32>(hashName))
	}
)
{
}

namespace phylo
{
	extern Simulation *g_pSimulation;
}

Simulation::~Simulation()
{
	g_pSimulation = nullptr;
	delete[] m_CellStore;
	m_NoisePipeline.freeCache(m_pNoiseCache);
}

void Simulation::pool_update() 
{
	const uint numCells = m_Cells.size();

	for (;;)
	{
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numCells);
		for (; uIdx < finalIdx; ++uIdx)
		{
			Cell *cell = m_Cells[uIdx];
			cell->update();
		}
		if (finalIdx == numCells)
		{
			return;
		}
	}
}

void Simulation::pool_update2() 
{
	// We alternate one row every frame because, well, it's really slow otherwise.

	const auto lightGridElementsEdge = m_LightGrid.m_GridElementsEdge;
	const auto curProcessRow = m_CurProcessRow;
	const bool flashlight = m_Flashlight;

	static constexpr uint readAhead = 8;

	for (;;)
	{
		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, lightGridElementsEdge);
		for (; uIdx < finalIdx; ++uIdx)
		{
			uint uIndex = uIdx + (lightGridElementsEdge * curProcessRow);

			uint8 &intensity = m_LightGrid.m_GridElements[uIndex];

			vector2F &position = m_LightGrid.m_GridElementsPositions[uIndex];

			float value = clamp(float(m_NoiseSrc->getValue(position.x * MulFactor, position.y * MulFactor, m_LightmapZ, m_pNoiseCache) + 1.0f) * 0.5f, 0.0f, 1.0f);
			value = sqrtf(value);
			//value = value * value;

			if (flashlight)
			{
				float distance = position.distance_sq(m_FlashlightPos);
				const float flashlightDistanceSq = 10000.0f;
				if (distance < 10000.0f)
				{
					value += 1.0f - (distance / flashlightDistanceSq);
					value = clamp(value, 0.0f, 1.0f);
				}
			}

			intensity = uint8(value * 255.5f);
		}
		if (finalIdx == lightGridElementsEdge)
		{
			return;
		}
	}
}

void Simulation::pool_update_waste() 
{
	const auto numWasteGridElements = m_WasteGrid.m_GridElements.size();
	const auto wasteGridElementsEdge = m_WasteGrid.m_GridElementsEdge;

	for (;;)
	{
		static constexpr uint readAhead = 64;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numWasteGridElements);
		for (; uIdx < finalIdx; ++uIdx)
		{
			uint32 amount = m_WasteGrid.m_AtomicGridElements[uIdx];
			amount = min(amount, m_WasteGrid.m_GridElements[uIdx]);
			m_WasteGrid.m_GridElements[uIdx] -= amount;
			m_WasteGrid.m_AtomicGridElements[uIdx] = 0;
		}
		if (finalIdx == numWasteGridElements)
		{
			return;
		}
	}
}

void Simulation::pool_updatelite() 
{
	const uint numCells = m_Cells.size();

	for (;;)
	{
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numCells);
		for (; uIdx < finalIdx; ++uIdx)
		{
			Cell *cell = m_Cells[uIdx];
			cell->update_lite();
		}
		if (finalIdx == numCells)
		{
			return;
		}
	}
}

uint32 Simulation::GetInstanceOffset(const vector2F &position) const 
{
	const float invGridElementSize = m_SimGrid.m_InvGridElementSize;

	vector2F adjustedPosition = (position + WorldRadius) * invGridElementSize;
	uint32 xcoord = static_cast<uint32>(adjustedPosition.x);
	uint32 ycoord = static_cast<uint32>(adjustedPosition.y);

	uint32 coord = (ycoord * m_SimGrid.m_GridElementsEdge) + xcoord;

	return coord;
}

uint32 Simulation::GetLightInstanceOffset(const vector2F &position) const 
{
	const float invGridElementSize = m_LightGrid.m_InvGridElementSize;

	vector2F adjustedPosition = (position + WorldRadius) * invGridElementSize;
	uint32 xcoord = static_cast<uint32>(adjustedPosition.x);
	uint32 ycoord = static_cast<uint32>(adjustedPosition.y);

	uint32 coord = (ycoord * m_LightGrid.m_GridElementsEdge) + xcoord;

	return coord;
}

uint32 Simulation::GetWasteInstanceOffset(const vector2F &position) const 
{
	const float invGridElementSize = m_WasteGrid.m_InvGridElementSize;

	vector2F adjustedPosition = (position + WorldRadius) * invGridElementSize;
	uint32 xcoord = static_cast<uint32>(adjustedPosition.x);
	uint32 ycoord = static_cast<uint32>(adjustedPosition.y);

	uint32 coord = (ycoord * m_WasteGrid.m_GridElementsEdge) + xcoord;

	return coord;
}

template <uint elements>
class AverageTime
{
	uint CurrentTimeOffset = 0;
	array<int64, elements> Times;

public:
	AverageTime()
	{
		memset(Times.data(), 0, elements * sizeof(int64));
	}

	AverageTime &operator = (const clock::time_span &span) 
	{
		uint idx = CurrentTimeOffset++;
		CurrentTimeOffset %= elements;

		Times[idx] = int64(span);

		return *this;
	}

	string to_string() const 
	{
		int64 totalTime = 0;
		for (int64 time : Times)
		{
			totalTime += time;
		}
		totalTime /= elements;

		return clock::time_span(totalTime).to_string();
	}

	operator clock::time_span() const 
	{
		int64 totalTime = 0;
		for (int64 time : Times)
		{
			totalTime += time;
		}
		totalTime /= elements;

		return clock::time_span(totalTime);
	}
};

void Simulation::spawn_initial_cell()
{
	uint cellIdx = m_Cells.size();

	// Find the most energetic position near the center of the map.
	float centerPercent = 0.2f;

	uint32 gridElements = m_LightGrid.m_GridElementsEdge;
	uint32 start = uint32(round(gridElements * (0.5f - (centerPercent * 0.5f))));
	uint32 end = start + uint32(round((gridElements * centerPercent)));

	uint8 bestValue = 0;
	uint32 bestIndex = -1;

	for (uint32 y = start; y <= end; ++y)
	{
		for (uint32 x = start; x <= end; ++x)
		{
			uint32 index = (y * gridElements) + x;

			uint8 value = m_LightGrid.m_GridElements[index];

			if (bestValue < value)
			{
				bestValue = value;
				bestIndex = index;
			}
		}
	}

	if (bestIndex == -1)
	{
		bestIndex = 0;
	}

	Cell *cell = getNewCellPtr();
	new (cell) Cell(nullptr, *this, m_LightGrid.m_GridElementsPositions[bestIndex], true);
	cell->m_CellIdx = cellIdx;
	cell->setEnergy(cell->getObjectCapacity());
	m_Cells += cell;
	++m_TotalCells;

	// Move the camera to here.
	m_pRenderer->set_screen_position(m_LightGrid.m_GridElementsPositions[bestIndex]);
}

void Simulation::sim_loop() 
{
	m_KickoffEvent.join();

	// This creates the first cell.
	spawn_initial_cell();

	clock::time_span tickTime = 0_msec; // This controls the speed of execution.

	Renderer::UIData uiData;
	uiData.speedState = uint(m_SpeedState);
	uiData.NumCells = m_Cells.size();
	uiData.CurTick = m_uCurrentFrame;

	m_pRenderer->update_from_sim(m_Illumination, m_uCurrentFrame, m_RenderController.getRawData(), m_LightGrid.m_GridElements, m_LightGrid.m_GridElementsEdge, m_WasteGrid.m_GridElements, m_WasteGrid.m_GridElementsEdge, uiData, m_VMController.m_ExecutionCounter);
	m_pRenderer->update_hash_name(m_HashName);

	clock::time_point lastExecuteTime = clock::get_current_time();
	while (m_SimThreadRun)
	{
		m_TotalParallelTime = clock::time_span(0ull);
		m_TotalSerialTime = clock::time_span(0ull);

		// TODO REMOVE DEBUG
		//if (m_Cells.size() == 500)
		//{
		//   // do nothing! This is our test point.
		//   m_pRenderer->update_from_sim(uiData);
		//   m_pRenderer->update_from_sim(m_uCurrentFrame, m_RenderController.getRawData(), m_GridElements, m_GridElementsEdge, uiData, m_VMController.m_ExecutionCounter);
		//   system::yield();
		//   continue;
		//}

		clock::time_point thisTime = clock::get_current_time();
		clock::time_span sinceLastTime = thisTime - lastExecuteTime;

		if (!m_SimThreadRun)
		{
			return;
		}

		// Check for speed settings.
		{
			SpeedState originalSpeedState = m_SpeedState;

			scoped_lock _lock(m_CallbackLock);
			for (auto &callback : m_Callbacks)
			{
				callback(this);
			}
			m_Callbacks.clear();

			if (m_SpeedState != originalSpeedState)
			{
				switch (m_SpeedState)
				{
				case SpeedState::Slow:
					tickTime = 16_msec; break;
				case SpeedState::Medium:
					tickTime = 4_msec; break;
				case SpeedState::Fast:
					tickTime = 1_msec; break;
				case SpeedState::Ludicrous:
					tickTime = 0_msec; break;
				default:
					__assume(0);
				}

				uiData.speedState = uint(m_SpeedState);
			}
		}

		if (((sinceLastTime < tickTime) | (m_SpeedState == SpeedState::Pause)) & (!m_Step))
		{
			m_ThreadPoolIndex = 0ull;
			m_ThreadPool3.kickoff();
			// HACK
			m_pRenderer->update_from_sim(m_Illumination, m_uCurrentFrame, m_RenderController.getRawData(), m_LightGrid.m_GridElements, m_LightGrid.m_GridElementsEdge, m_WasteGrid.m_GridElements, m_WasteGrid.m_GridElementsEdge, uiData, m_VMController.m_ExecutionCounter);
			m_pRenderer->update_from_sim(uiData);
			m_pRenderer->force_update();
			system::yield();
			continue;
		}

		m_Step = false;

		//while (sinceLastTime >= tickTime)
		{
			if (!m_SimThreadRun)
			{
				break;
			}
			// need to use modulus, but type doesn't support it.
			while ((double(xtd::time(tickTime)) != 0.0) & (sinceLastTime >= tickTime))
			{
				sinceLastTime -= tickTime;
			}
			{
				scoped_lock _lock(m_ClickLock);
				for (const auto &click : m_Clicks)
				{
					m_Flashlight = click.State;
					m_FlashlightPos = click.Position;
				}
				m_Clicks.clear();
			}

			// scoped_lock isn't fair, this gives the save system a chance.
			while (m_SimulationLockAtomic)
			{
				system::yield();
			}
			scoped_lock _lock(m_SimulationLock);

			if (m_Cells.size() == 0)
			{
				// If there are no cells left, create a new starter cell.
				// also clear waste
				for (auto &value : m_WasteGrid.m_GridElements)
				{
					value = 0;
				}
				spawn_initial_cell();
			}

			static AverageTime<50> totalTime;
			static AverageTime<50> vmTime;
			static AverageTime<50> physicsTime;
			static AverageTime<50> renderTime;
			static AverageTime<50> updateTime;
			static AverageTime<50> postTime;
			static AverageTime<50> lightTime;

			clock::time_point totalTimeStart = clock::get_current_time();

#if DYNAMIC_LIGHTS
			{
				clock::time_point thisTime = clock::get_current_time();
				clock::time_point subTime = thisTime;
				m_ThreadPoolIndex = 0ull;
				m_ThreadPool2.kickoff();
				++m_CurProcessRow;
				m_CurProcessRow %= m_LightGrid.m_GridElementsEdge;
				if (m_CurProcessRow == 0)
				{
					m_LightmapZ += options::LightMapChangeRate * float(m_LightGrid.m_GridElementsEdge);
				}
				m_TotalSerialTime += clock::get_current_time() - subTime;

				subTime = clock::get_current_time();
				// TODO put into waste time
				m_ThreadPoolIndex = 0ull;
				m_ThreadPool4.kickoff();
				m_TotalParallelTime += clock::get_current_time() - subTime;

				subTime = clock::get_current_time();
				double adjustedTick = double(m_uCurrentFrame) / (options::LightPeriodTicks / (xtd::pi<float>));
				m_Illumination = sin((xtd::pi<float> / 2.0) * cos(adjustedTick));
				m_Illumination = (m_Illumination + 1.0) / 2.0;

				m_TotalSerialTime += clock::get_current_time() - subTime;
				lightTime = clock::get_current_time() - thisTime;
			}
#endif

			{
				clock::time_point thisTime = clock::get_current_time();
				m_VMController.update();
				vmTime = clock::get_current_time() - thisTime;
			}
			{
				clock::time_point thisTime = clock::get_current_time();
				m_PhysicsController.update();
				physicsTime = clock::get_current_time() - thisTime;
			}

			m_RenderController.update();

			{
				clock::time_point thisTime = clock::get_current_time();
				// Update the cells (copies data between components)
				m_ThreadPoolIndex = 0ull;
				m_ThreadPool.kickoff();
				m_TotalParallelTime += clock::get_current_time() - thisTime;
				updateTime = clock::get_current_time() - thisTime;
			}

			bool renderUpdate = false;
			{
				clock::time_point thisTime = clock::get_current_time();
				uiData.NumCells = m_Cells.size();
				uiData.CurTick = m_uCurrentFrame;
				uiData.TotalCells = m_TotalCells.load();
				if (m_pRenderer && m_pRenderer->is_frame_ready())
				{
					m_pRenderer->update_from_sim(m_Illumination, m_uCurrentFrame, m_RenderController.getRawData(), m_LightGrid.m_GridElements, m_LightGrid.m_GridElementsEdge, m_WasteGrid.m_GridElements, m_WasteGrid.m_GridElementsEdge, uiData, m_VMController.m_ExecutionCounter);
					renderUpdate = true;
				}
				m_TotalSerialTime += clock::get_current_time() - thisTime;
				renderTime = clock::get_current_time() - thisTime;
			}

			{
				clock::time_point thisTime = clock::get_current_time();
				m_VMController.post_update();

				clock::time_point subTime = clock::get_current_time();
				if (m_DestroyTasks.size())
				{
					if (options::Deterministic)
					{
						std::sort(m_DestroyTasks.data(), m_DestroyTasks.data() + m_DestroyTasks.size(), [](const Cell * a, const Cell * b) { return a->getCellID() < b->getCellID(); });
					}
					for (Cell *cell : m_DestroyTasks)
					{
						destroyCell(*cell);
					}
					m_DestroyTasks.clear();
				}
				m_TotalSerialTime += clock::get_current_time() - subTime;
				postTime = clock::get_current_time() - thisTime;
			}

			totalTime = clock::get_current_time() - totalTimeStart;

			//if (m_uCurrentFrame == 200000)
			//{
			//	m_SpeedState = SpeedState::Pause;
			//}

			if ((m_LoadedFrame != m_uCurrentFrame) & ((m_uCurrentFrame % 200000) == 0))
			{
				Autosave();
			}

			++m_uCurrentFrame;

			uiData.totalTime = totalTime;
			uiData.vmTime = vmTime;

			uiData.physicsTime = physicsTime;
			uiData.updateTime = updateTime;
			uiData.renderUpdateTime = renderTime;
			uiData.postTime = postTime;
			uiData.lightTime = lightTime;
			uiData.serialTime = m_TotalSerialTime;
			uiData.parallelTime = m_TotalParallelTime;
		}
		lastExecuteTime = thisTime;
	}
}

Cell &Simulation::getNewCell(const Cell *parent) 
{
	uint cellIdx = m_Cells.size();
	Cell *cell = getNewCellPtr();
	new (cell) Cell(parent, *this, vector2F());
	cell->m_CellIdx = cellIdx;
	cell->setEnergy(cell->getObjectCapacity());
	m_Cells += cell;
	++m_TotalCells;

	return *cell;
}

void Simulation::killCell(Cell &cell) 
{
	cell.m_Alive = false;

	// when the cell dies, it needs to contribute to the waste field.
	uint wasteOffset = GetWasteInstanceOffset(cell.m_PhysicsInstance->m_Position);
	uint64 contributeEnergy = cell.getEnergy();
	contributeEnergy += uint64((double(cell.getObjectCapacity()) * 0.01) + 0.5);

	uint64 totalEnergy = contributeEnergy + m_WasteGrid.m_GridElements[wasteOffset];
	totalEnergy = min(totalEnergy, (uint64)traits<uint32>::max);
	m_WasteGrid.m_GridElements[wasteOffset] = uint32(totalEnergy);
}

void Simulation::beEatenCell(Cell &cell) 
{
}

void Simulation::destroyCell(Cell &cell) 
{
	cell.m_Alive = false;

	uint idx = cell.m_CellIdx;

	//delete &cell; // deletion logic.
	cell.~Cell();
	freeCellPtr(&cell);

	m_Cells[idx] = m_Cells.back();
	m_Cells[idx]->m_CellIdx = idx;

	m_Cells.pop_back();
}

Cell *Simulation::findCell(const vector2F &position, float radius, Cell *filter) const 
{
	return m_PhysicsController.findCell(position, radius, filter);
}

float Simulation::getGreenEnergy(const vector2F &position) const 
{
	return getGreenEnergy(GetLightInstanceOffset(position));
}

float Simulation::getRedEnergy(const vector2F &position) const 
{
	return getRedEnergy(GetLightInstanceOffset(position));
}

uint32 Simulation::getBlueEnergy(const vector2F &position) const 
{
	return getBlueEnergy(GetWasteInstanceOffset(position));
}

float Simulation::getGreenEnergy(uint offset) const 
{
	return m_Illumination;
}

float Simulation::getRedEnergy(uint offset) const 
{
	float energyAtPosition = float(m_LightGrid.m_GridElements[offset]) / 255.0f;

	//float redEnergy = energyAtPosition * 5.0f;
	//if (redEnergy > 1.0f)
	//{
	//	redEnergy = 1.0f - (redEnergy - 1.0f);
	//}
	//redEnergy = clamp(redEnergy, 0.0f, 1.0f);

	return energyAtPosition;
}

uint32 Simulation::getBlueEnergy(uint offset) const 
{
	uint32 energyAtPosition = m_WasteGrid.m_GridElements[offset];

	return energyAtPosition;
}

void Simulation::decreaseBlueEnergy(uint offset, uint32 amount) 
{
	((atomic<uint32> &)m_WasteGrid.m_AtomicGridElements[offset]) += amount;
}

void Simulation::on_click(const vector2F &pos, bool state) 
{
	scoped_lock _lock(m_ClickLock);
	m_Clicks.push_back({ pos, state });
}

void Simulation::halt() 
{
	m_SimThreadRun = false;
	if (m_SimThread.started())
	{
		m_SimThread.join();
	}
}

#define NOMINMAX 1
#include <Windows.h>
#include <Commdlg.h>
#include <Shlobj.h>
#include <Shlwapi.h>

#define LOCAL_PATH_ONLY 1

namespace phylo
{
	extern Simulation *g_pSimulation;
	extern Renderer *g_pRenderer;
}

void Simulation::openInstructionStats() 
{
	m_pRenderer->openInstructionStats();
}

void Simulation::openSettings() 
{
	// This really needs to be set on the renderer.
	m_pRenderer->openSettings();
}

void Simulation::newSim(const string &hashName) 
{
	{
		m_SimulationLockAtomic = 1;
		scoped_lock _lock(m_SimulationLock);
		m_SimulationLockAtomic = 0;
		// Check if they want to save first.
		const int result = MessageBoxW(HWND(m_pRenderer ? m_pRenderer->get_window()->_get_platform_handle() : nullptr), L"Would you like to save first?", L"Save?", MB_YESNOCANCEL);
		switch (result)
		{
		case IDYES:
			onSave();
			break;
		case IDNO:
			break;
		case IDCANCEL:
			return;
		}
	}

	event startNewSimulation;
	event threadKickedOff;
	Simulation *newSimulation = nullptr;

	{
		m_SimulationLockAtomic = 1;
		scoped_lock _lock(m_SimulationLock);
		m_SimulationLockAtomic = 0;

		newSimulation = new Simulation(hashName, startNewSimulation, threadKickedOff);

		m_SimThreadRun = false;

		newSimulation->m_SpeedState = m_SpeedState;
		newSimulation->set_renderer(phylo::g_pRenderer);
	}
	m_SimThread.join();
	delete this; // destroy this simulation.

	phylo::g_pSimulation = newSimulation;
	phylo::g_pRenderer->setSimulation(newSimulation);
	phylo::g_pRenderer->resetFrame();

	startNewSimulation.set();
	threadKickedOff.join();
	newSimulation->kickoff();
}

template <typename T>
struct scoped_false
{
	T &value;
	scoped_false(T &val) : value(val) {}
	~scoped_false() { value = false; }
};

template <typename T>
struct scoped_decrement
{
	T &value;
	scoped_decrement(T &val) : value(val) {}
	~scoped_decrement() { --value; }
};

void Simulation::onLoad() 
{
	event startNewSimulation;
	event threadKickedOff;
	Simulation *newSimulation = nullptr;

	m_Loading = true;
	scoped_false<decltype(m_Loading)> _saveFalse{ m_Loading };

	{
		m_SimulationLockAtomic = 1;
		scoped_lock _lock(m_SimulationLock);
		m_SimulationLockAtomic = 0;
		{
			// Check if they want to save first.
			const int result = MessageBoxW(HWND(m_pRenderer ? m_pRenderer->get_window()->_get_platform_handle() : nullptr), L"Would you like to save first?", L"Save?", MB_YESNOCANCEL);
			switch (result)
			{
			case IDYES:
				onSave();
				break;
			case IDNO:
				break;
			case IDCANCEL:
				return;
			}
		}

		OPENFILENAMEW dlgOpen;
		memset(&dlgOpen, 0, sizeof(dlgOpen));
		dlgOpen.lStructSize = sizeof(dlgOpen);
		dlgOpen.hwndOwner = nullptr;
		wchar_t path[MAX_PATH] = L"";
		wchar_t prepath[MAX_PATH] = L"";
		dlgOpen.lpstrFile = path;
		dlgOpen.lpstrInitialDir = prepath;
		dlgOpen.lpstrFilter = L"Simulations (*.esim)\0*.esim\0";
		dlgOpen.nMaxFile = MAX_PATH;
		dlgOpen.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		dlgOpen.lpstrDefExt = L"esim";
		dlgOpen.hwndOwner = HWND(m_pRenderer ? m_pRenderer->get_window()->_get_platform_handle() : nullptr);

#if LOCAL_PATH_ONLY
		extern void ResetWorkingDirectory();
		ResetWorkingDirectory();
		GetCurrentDirectoryW(MAX_PATH, prepath);
#else
		SHGetSpecialFolderPathW(
			nullptr,
			path,
			CSIDL_LOCAL_APPDATA,
			TRUE
		);
		PathAppendW(path, L"phylogen");
		CreateDirectoryW(path, nullptr);
#endif

		bool bOpen = GetOpenFileNameW(&dlgOpen) != 0;
		extern void ResetWorkingDirectory();
		ResetWorkingDirectory();

		decltype(m_WasteGrid.m_GridElements) newWasteGrid;

		if (!bOpen)
		{
			// Determine if they cancelled or it was an error.
			DWORD extError = CommDlgExtendedError();
			if (extError != 0)
			{
				throw "Unable to open that file";
			}
			else
			{
				return;
			}
		}

		if (PathFileExistsW(path) == FALSE)
		{
			return;
		}

		{
			// Read in the file.
			try
			{
				io::file inFile(string{ path }, io::file::Flags::Sequential | io::file::Flags::Read);
				array_view<uint8> view = { (uint8 * )inFile.at(0), inFile.get_size() };
				Stream inStream(view);

				uint32 compressed;
				uint64 uncompressedSize;

				inStream.read(compressed);
				inStream.read(uncompressedSize);

				loadInitializer loadInit;
				xtd::array<Cell *>::size_type numCells;

				auto unserialize = [&](Stream &inStream)
				{
					options_delta curOptions;
					inStream.read(curOptions);
					curOptions.apply();

					loadInit.m_HashName = inStream.readString();
					uint64 totalCells = 0;
					inStream.read<uint64>(totalCells);
					inStream.read(loadInit.m_uCurrentFrame);
					inStream.read(loadInit.m_SimGridElementsEdge);
					inStream.read(loadInit.m_LightGridElementsEdge);
					inStream.read(loadInit.m_WasteGridElementsEdge);
					inStream.read(loadInit.m_LightmapZ);
					inStream.read(loadInit.m_CurProcessRow);
					inStream.read(loadInit.m_NoiseSeed); // This isn't the current adjusted seed.
					decltype(m_WasteGrid.m_GridElements.size()) elementCount;
					inStream.read(elementCount);
					newWasteGrid.resize(elementCount);
					for (size_t i = 0; i < elementCount; ++i)
					{
						inStream.read(newWasteGrid[i]);
					}
					inStream.read(numCells);

					// new Simulation
					newSimulation = new Simulation(startNewSimulation, threadKickedOff, loadInit);
					newSimulation->m_WasteGrid.m_GridElements = std::move(newWasteGrid);
					for (xtd::array<Cell * >::size_type i = 0; i < numCells; ++i)
					{
						Cell &newCell = newSimulation->getNewCell(nullptr);
						newCell.unserialize(inStream);
					}
					newSimulation->m_TotalCells = totalCells;
				};

				if (compressed)
				{
					array<uint8> uncompressedData(uncompressedSize);
					compression::bulk_decompress<compression::zlib>(
					{ (uint8 * )inFile.at(sizeof(uint32) + sizeof(uint64)), inFile.get_size() - (sizeof(uint32) + sizeof(uint64))
					}, uncompressedData);

					//compression::streaming_store StreamStore(0x4000, 32);
					//
					//array_view<uint8> compressedView = { inStream.getRawData().data() + sizeof(uint32) + sizeof(uint64), inStream.getRawData().size_raw() - (sizeof(uint32) + sizeof(uint64)) };
					//
					//compression::stream<compression::zlib> compressStream(compressedView);
					//
					//compression::streaming_array streamArray(StreamStore, compressStream, uncompressedSize);
					//
					//array_view<uint8> uncompressedView = { (uint8 *)streamArray.get_ptr(), uncompressedSize };
					//Stream uncompressedStream(uncompressedView);

					//unserialize(uncompressedStream);
					array_view<uint8> uncompressedView = { (uint8 * )uncompressedData.data(), uncompressedData.size_raw() };
					Stream uncompressedStream(uncompressedView);
					unserialize(uncompressedStream);
				}
				else
				{
					unserialize(inStream);
				}
			}
			catch (...)
			{
				delete newSimulation;
				throw "Unable to load";
			}

			m_SimThreadRun = false;
		}
		newSimulation->m_SpeedState = m_SpeedState;

		newSimulation->set_renderer(phylo::g_pRenderer);

		newSimulation->m_LoadedFrame = newSimulation->m_uCurrentFrame;
	}
	m_SimThread.join();
	delete this; // destroy this simulation.

	phylo::g_pSimulation = newSimulation;
	phylo::g_pRenderer->setSimulation(newSimulation);
	phylo::g_pRenderer->resetFrame();

	startNewSimulation.set();
	threadKickedOff.join();
	newSimulation->kickoff();
}

void Simulation::onSave() 
{
	scoped_lock _lock(SaveLock);

	++m_Saving;
	scoped_decrement<decltype(m_Saving)> _saveFalse{ m_Saving };

	Stream outStream;
	thread saveGatherThread;

	wchar_t path[MAX_PATH] = L"";

	{
		m_SimulationLockAtomic = 1;
		scoped_lock _lock(m_SimulationLock);
		m_SimulationLockAtomic = 0;

		startSave(saveGatherThread, outStream);

		OPENFILENAMEW dlgOpen;
		memset(&dlgOpen, 0, sizeof(dlgOpen));
		dlgOpen.lStructSize = sizeof(dlgOpen);
		dlgOpen.hwndOwner = nullptr;
		dlgOpen.lpstrFile = path;
		dlgOpen.lpstrFilter = L"Simulations (*.esim)\0*.esim\0";
		dlgOpen.nMaxFile = MAX_PATH;
		dlgOpen.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_CREATEPROMPT;
		dlgOpen.lpstrDefExt = L"esim";
		dlgOpen.hwndOwner = HWND(m_pRenderer ? m_pRenderer->get_window()->_get_platform_handle() : nullptr);

#if LOCAL_PATH_ONLY
		extern void ResetWorkingDirectory();
		ResetWorkingDirectory();
		GetCurrentDirectoryW(MAX_PATH, path);
#else
		SHGetSpecialFolderPathW(
			nullptr,
			path,
			CSIDL_LOCAL_APPDATA,
			TRUE
		);
		PathAppendW(path, L"phylogen");
		CreateDirectoryW(path, nullptr);
#endif

		wchar_t filename[MAX_PATH] = { L'\0' };
		const time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		wchar_t timeBuffer[32];
		swprintf(timeBuffer, 32, L"%I64u", t);
		lstrcatW(filename, timeBuffer);
		lstrcatW(filename, L".esim");
		PathAppendW(path, filename);

		bool bSave = GetSaveFileNameW(&dlgOpen) != 0;
		extern void ResetWorkingDirectory();
		ResetWorkingDirectory();
		if (!bSave)
		{
			// Determine if they cancelled or it was an error.
			DWORD extError = CommDlgExtendedError();
			if (extError != 0)
			{
				throw "Unable to save to that location";
			}
			else
			{
				return;
			}
		}

		saveGatherThread.join();
	}

	doSave(outStream, string{ path });
}

void Simulation::Autosave() 
{
	scoped_lock _lock(SaveLock);

	++m_Saving;
	scoped_decrement<decltype(m_Saving)> _saveFalse{ m_Saving };

	Stream outStream;
	thread saveGatherThread;

	wchar_t path[MAX_PATH] = L"";

	{
		m_SimulationLockAtomic = 1;
		scoped_lock _lock(m_SimulationLock);
		m_SimulationLockAtomic = 0;

		startSave(saveGatherThread, outStream);

#if LOCAL_PATH_ONLY
		extern void ResetWorkingDirectory();
		ResetWorkingDirectory();
		GetCurrentDirectoryW(MAX_PATH, path);
#else
		SHGetSpecialFolderPathW(
			nullptr,
			path,
			CSIDL_LOCAL_APPDATA,
			TRUE
		);
		PathAppendW(path, L"phylogen");
		CreateDirectoryW(path, nullptr);
#endif

		wchar_t filename[MAX_PATH] = { L'\0' };
		lstrcatW(filename, L"autosave.esim");
		PathAppendW(path, filename);

		saveGatherThread.join();
	}

	doSave(outStream, string{ path });
}

void Simulation::startSave(thread &saveGatherThread, Stream &outStream) 
{
	saveGatherThread = [this, &outStream]()
	{
		options_delta curOptions;
		outStream.write(curOptions);

		outStream.writeString(m_HashName);
		outStream.write(m_TotalCells.load());
		outStream.write(m_uCurrentFrame);
		outStream.write(m_SimGrid.m_GridElementsEdge);
		outStream.write(m_LightGrid.m_GridElementsEdge);
		outStream.write(m_WasteGrid.m_GridElementsEdge);
		outStream.write(m_LightmapZ);
		outStream.write(m_CurProcessRow);
		outStream.write(m_NoisePipeline.getSeed()); // This isn't the current adjusted seed.
		// Also need to write out the waste stuff.
		outStream.write(m_WasteGrid.m_GridElements.size());
		for (const auto elem : m_WasteGrid.m_GridElements)
		{
			outStream.write(elem);
		}

		outStream.write(m_Cells.size());
		for (const Cell *cell : m_Cells)
		{
			// Save cell data
			cell->serialize(outStream);
		}
	};
	saveGatherThread.start();
}

void Simulation::doSave(Stream &outStream, const string_view &dest) 
{
	event streamMoved;
	thread remoteThread{ [=, &outStream, &dest, &streamMoved]() {
		++m_Saving;
		scoped_decrement<decltype(m_Saving)> _saveFalse{ m_Saving };

		Stream outStream = std::move(outStream);
		string pathStr = std::move(dest);
		streamMoved.set();
		scoped_lock _lock(SaveLock);
		// If we are here, it's time to actually save the file. Write out 'outstream'.
		try
		{
			array<uint8> compressedBuffer(outStream.getRawData().size_raw() - 1);

			try
			{
				array_view<uint8> compressedData = compression::bulk_compress<xtd::compression::zlib>(outStream.getRawData(), compressedBuffer); // zlib is streamable

				io::file outFile(pathStr, io::file::Flags::New | io::file::Flags::Sequential | io::file::Flags::Write, compressedData.size_raw() + sizeof(uint64) + sizeof(uint32));

				uint64 sz = outStream.getRawData().size_raw();
				uint32 compressed = 1;
				outFile.write(0, &compressed, sizeof(compressed));
				outFile.write(sizeof(uint32), &sz, sizeof(sz));
				outFile.write(sizeof(uint64) + sizeof(uint32), compressedData.data(), compressedData.size_raw());
			}
			catch (...)
			{
				io::file outFile(pathStr, io::file::Flags::New | io::file::Flags::Sequential | io::file::Flags::Write, outStream.getRawData().size_raw() + sizeof(uint64) + sizeof(uint32));

				uint64 sz = 0;
				uint32 compressed = 0;
				outFile.write(0, &compressed, sizeof(compressed));
				outFile.write(sizeof(uint32), &sz, sizeof(sz));
				outFile.write(sizeof(uint64) + sizeof(uint32), outStream.getRawData().data(), outStream.getRawData().size_raw());
			}
		}
		catch (...)
		{
			MessageBoxW(nullptr, L"Failed to save.", L"Failed", MB_OK | MB_ICONWARNING);
		}
	} };
	remoteThread.start();
	remoteThread.reset();
	streamMoved.join();
}
