#pragma once

#include <xtd/xtd>
#include <Windows.h>

namespace phylo
{
   class ThreadPool final
   {
      static constexpr const bool SINGLE_THREADED = false;

      function<void(usize)>   m_PoolFunc;
      array<thread>           m_PoolThreads;
      semaphore               m_PoolEvent;
      atomic<bool>            m_PoolAlive = true;
      semaphore               m_PoolFinishEvent;

      void ThreadPoolFunc(usize threadID, usize numThreads) 
      {
		    if (numThreads > 1)
		    {
			    SetThreadAffinityMask(GetCurrentThread(), 1ULL << threadID);
			    Sleep(2);
		    }

         for (;;)
         {
            m_PoolEvent.join();

            if (!m_PoolAlive)
            {
               return;
            }

            m_PoolFunc(threadID);

            m_PoolFinishEvent.release(1);
         }
      }

   public:
      ThreadPool(const string_view &poolName, function<void(usize)>&& poolFunc, bool singleThreaded = SINGLE_THREADED ? 1 : 0) :
         m_PoolFunc(std::move(poolFunc)),
         m_PoolEvent(singleThreaded ? 1 : system::get_system_information().logical_core_count, 0),
         m_PoolFinishEvent(singleThreaded ? 1 : system::get_system_information().logical_core_count, 0)
      {
         const uint numThreads = singleThreaded ? 1 : system::get_system_information().logical_core_count;
         m_PoolThreads.resize(numThreads);
         usize threadNum = 0;
         for (thread &_thread : m_PoolThreads)
         {
            _thread = [=, this] {ThreadPoolFunc(threadNum, numThreads); };
            _thread.set_name(poolName + " thread " + string::from(threadNum));
            _thread.start();

            ++threadNum;
         }
      }

      ~ThreadPool()
      {
         m_PoolAlive = false;
         m_PoolEvent.release(m_PoolThreads.size());
        
         for (thread &_thread : m_PoolThreads)
         {
            _thread.join();
         }
      }

      void kickoff() 
      {
         m_PoolEvent.release(m_PoolThreads.size());

         // Then we have to wait for them all to return.
         for (usize i = 0; i < m_PoolThreads.size(); ++i)
         {
            m_PoolFinishEvent.join();
         }
      }

      usize getThreadCount() const 
      {
         return m_PoolThreads.size();
      }
   };
}
