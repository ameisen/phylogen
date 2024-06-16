// PRECOMPILED HEADER

#pragma once

#include <xtd/xtd>
#include <bit>

using namespace xtd;
using namespace xtd::math;

template <typename T>
static void memzero(T &val)
{
   memset(&val, 0, sizeof(val));
}

#include "SimOptions.hpp"

namespace phylo
{
   struct Task// final : xtd::trait_simple
   {
      uint64            CellID;
      function<void()>  Function;

	  Task(uint64 cell_id, function<void()>&& functor) :
		  CellID(cell_id),
         Function(std::move(functor))
      {}

	  Task(const Task &) = default;
	  Task(Task &&) = default;

	  Task & operator = (const Task &) = default;
	  Task & operator = (Task &&) = default;

      auto operator <=> (const Task &task) const 
      {
         return CellID <=> task.CellID;
      }

      operator uint64 () const 
      {
         return CellID;
      }
   };
}

//namespace std
//{
//	template<>
//	void swap(phylo::Task& lhs, phylo::Task& rhs)
//	{
//		// ... blah
//	}
//}

class Stream
{
   static constexpr const usize cuSizeExpandAlign = 0x1000ULL;

   bool reading = false;

   array<uint8>	   m_StreamData;
   array_view<uint8> m_StreamView;
   usize				   m_uCurrentOffset;

   void expandTo(usize uSize) 
   {
      uSize = (uSize + (cuSizeExpandAlign - 1U)) & ~(cuSizeExpandAlign - 1U);
      if (uSize > m_StreamData.size())
      {
         m_StreamData.resize(typename decltype(m_StreamData)::size_type(uSize));
      }
   }

public:
   Stream() : m_StreamView(nullptr), m_uCurrentOffset(0ULL)
   {
      m_StreamData.reserve(128ULL * 1024ULL * 1024ULL);
   }
   Stream(const array_view<uint8> &rData) : reading(true), m_StreamView(rData), m_uCurrentOffset(0ULL) {
   }
   Stream(Stream &&stream) = default;
   ~Stream() = default;

   void truncate() 
   {
      m_StreamData.resize(typename decltype(m_StreamData)::size_type(m_uCurrentOffset));
   }

   void reset() 
   {
      m_uCurrentOffset = 0ULL;
      if (!reading) {
         m_StreamData.resize(cuSizeExpandAlign);
      }
   }

   array_view<uint8> getRawData() const 
   {
      if (reading)
      {
         return m_StreamView;
      }
      else
      {
         return m_StreamData;
      }
   }

   void writeString(const xtd::string &rString) 
   {
      const uint16 uSize = uint16(rString.size());
      write(uSize);
      writeRaw(rString.data(), rString.size());
   }

   void writeRaw(const void *pData, usize uLength) 
   {
      const size_t uSizeNeeded = m_uCurrentOffset + uLength;
      expandTo(uSizeNeeded);

      std::memcpy(m_StreamData.data() + m_uCurrentOffset, pData, uLength);
      m_uCurrentOffset += uLength;
   }

   template <typename T>
   void write(const T &rVal) 
   {
      const usize uSizeNeeded = m_uCurrentOffset + sizeof(T);
      expandTo(uSizeNeeded);

      std::memcpy(m_StreamData.data() + m_uCurrentOffset, &rVal, sizeof(T));
      m_uCurrentOffset += sizeof(T);
   }

   template <typename T>
   void write(const std::atomic<T>& rVal)
   {
      const usize uSizeNeeded = m_uCurrentOffset + sizeof(T);
      expandTo(uSizeNeeded);

      T temp = rVal.load();
      std::memcpy(m_StreamData.data() + m_uCurrentOffset, &temp, sizeof(T));
      m_uCurrentOffset += sizeof(T);
   }

   xtd::string readString()
   {
      uint16 uSize;
      read(uSize);

      array<char> ret(uSize + 1, 0);
      readRaw(ret.data(), uSize);
      return { ret.data() };
   }

   void readRaw(void *pData, usize uLength) 
   {
      std::memcpy(pData, m_StreamView.data() + m_uCurrentOffset, uLength);
      m_uCurrentOffset += uLength;
   }

   template <typename T>
   void read(T &rVal) 
   {
      std::memcpy(&rVal, m_StreamView.data() + m_uCurrentOffset, sizeof(T));
      m_uCurrentOffset += sizeof(T);
   }

   template <typename T>
   void read(std::atomic<T>& rVal)
   {
      T temp;
      std::memcpy(&temp, m_StreamView.data() + m_uCurrentOffset, sizeof(T));
      rVal.store(temp);
      m_uCurrentOffset += sizeof(T);
   }
};
