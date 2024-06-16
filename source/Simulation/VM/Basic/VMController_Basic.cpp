#include "phylogen.hpp"
#include "VMController_Basic.hpp"
#include "Simulation/Simulation.hpp"

using namespace phylo;
using namespace phylo::VM;
using namespace phylo::VM::Basic;

// until we have a real wide_array implementation, we need to presize it.
static constexpr usize WideArraySize = 5'000'000ull;

ControllerImpl::ControllerImpl(Simulation &simulation) : m_Simulation(simulation),
m_ThreadPool("VM", [this](usize idx) {pool_update(idx); }, 0),
m_ThreadPool2("VM2", [this](usize idx) {pool_update2(idx); }, 1)
{
	memset(m_ExecutionCounter.data(), 0, m_ExecutionCounter.size_raw());
}

ControllerImpl::~ControllerImpl() = default;

void ControllerImpl::pool_update(usize threadID)
{
	const uint numInstances = m_Instances.size();

	instance_t::CounterType counter;
	memset(&counter, 0, sizeof(counter));

	for (;;)
	{
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numInstances);
		for (; uIdx < finalIdx; ++uIdx)
		{
			Instance &instance = m_Instances[uIdx];
			instance.tick(this, counter);
		}
		if (finalIdx == numInstances)
		{
			for (usize i = 0; i < counter.size(); ++i)
			{
				m_ExecutionCounter[i] += counter[i];
			}
			return;
		}
	}
}

void ControllerImpl::pool_update2(usize threadID)
{
	const uint numTasks = m_UnserializedTasks.size();

	for (;;)
	{
		static constexpr uint readAhead = 16;

		uint uIdx = m_ThreadPoolIndex.fetch_add(readAhead);
		uint finalIdx = std::min(uIdx + readAhead, numTasks);
		for (; uIdx < finalIdx; ++uIdx)
		{
			auto &task = m_UnserializedTasks[uIdx];
			task();
		}
		if (finalIdx == numTasks)
		{
			return;
		}
	}
}

void ControllerImpl::update()
{
	clock::time_point subTime = clock::get_current_time();
	memset(m_ExecutionCounter.data(), 0, m_ExecutionCounter.size_raw());
	m_Simulation.m_TotalSerialTime += clock::get_current_time() - subTime;

	subTime = clock::get_current_time();
	m_ThreadPoolIndex = 0ull;
	m_ThreadPool.kickoff();
	m_Simulation.m_TotalParallelTime += clock::get_current_time() - subTime;
}

#include <algorithm>

void ControllerImpl::post_update()
{
	while ((m_SerializedTasks.size() != 0) | (m_UnserializedTasks.size() != 0))
	{
		if (m_UnserializedTasks.size() != 0)
		{
			clock::time_point subTime = clock::get_current_time();
			m_ThreadPoolIndex = 0ull;
			m_ThreadPool2.kickoff();
			m_Simulation.m_TotalParallelTime += clock::get_current_time() - subTime;
			m_UnserializedTasks.clear();
		}

		{
			clock::time_point subTime = clock::get_current_time();
			if (m_SerializedTasks.size() != 0)
			{
				if (options::Deterministic)
				{
					m_SerializedTasks.sort();
				}
				for (auto & task : m_SerializedTasks)
				{
					task.Function();
				}
				m_SerializedTasks.clear();
			}
			m_Simulation.m_TotalSerialTime += clock::get_current_time() - subTime;
		}
	}

	clock::time_point subTime = clock::get_current_time();
	if (m_KillTasks.size())
	{
		if (options::Deterministic)
		{
			std::sort(m_KillTasks.data(), m_KillTasks.data() + m_KillTasks.size(), [](const Cell * a, const Cell * b) { return a->getCellID() < b->getCellID(); });
		}
		for (Cell *cell : m_KillTasks)
		{
			m_Simulation.killCell(*cell);
		}
		m_KillTasks.clear();
	}
	m_Simulation.m_TotalSerialTime += clock::get_current_time() - subTime;
}
