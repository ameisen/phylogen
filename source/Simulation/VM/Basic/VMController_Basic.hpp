#pragma once

#include "../VMInstance.hpp"
#include "ThreadPool.hpp"
#include "Simulation/Controller.hpp"

namespace phylo
{
	class Simulation;
	namespace VM::Basic
	{
		class ControllerImpl final : public ComponentController<VM::Instance>
		{
		protected:
			friend struct Instance;
			typedef VM::Instance instance_t;
			using CounterType = array<atomic<uint32>, VM::NumOperations + 1>;
		private:

			static constexpr bool SINGLE_THREADED = false;

			Simulation     &m_Simulation;

			ThreadPool                 m_ThreadPool;
			ThreadPool                 m_ThreadPool2;
			atomic<uint>              m_ThreadPoolIndex;

			mutex                    m_SerializedLock;
			array<Task>              m_SerializedTasks;
			array<Cell * >              m_KillTasks;
			mutex                    m_UnserializedLock;
			array<function<void()>>  m_UnserializedTasks;

			void pool_update(usize threadID) ;
			void pool_update2(usize threadID) ;

		public:
			ControllerImpl() = delete;
			ControllerImpl(Simulation &simulation);
			~ControllerImpl();

			void update() ;
			void post_update() ;

			CounterType m_ExecutionCounter;
		};
	}
}
