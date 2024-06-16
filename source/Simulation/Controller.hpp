#pragma once

#include <memory>
#include <limits>

namespace phylo
{
   namespace Physics {
      struct Instance;
   }

   namespace VM {
      struct Instance;
   }

   namespace Render {
      struct Instance;
   }

   class Cell;

   namespace ComponentControllerCommon {
      void update_cell_instance(Cell *cell, Physics::Instance *newInstance);
      void update_cell_instance(Cell *cell, VM::Instance *newInstance);
      void update_cell_instance(Cell *cell, Render::Instance *newInstance);
   };

   // To use this, your TInstance type must have a variable named m_Cell which is of type Cell *.
   template <typename TInstance, bool Ordered = false>
   class ComponentController
   {
   protected:
      // until we have a real wide_array implementation, we need to presize it.
      static constexpr usize WideArraySize = 2'000'000ull;

      wide_array<TInstance>    				      m_Instances;

      ComponentController()
      {
         m_Instances.reserve(WideArraySize);
      }

      ComponentController(ComponentController&&) = default;

   public:
      bool contains_cell(Cell *cell) const 
      {
         for (const TInstance &instance : m_Instances)
         {
            if (instance.m_Cell == cell) [[unlikely]]
            {
               return true;
            }
         }
         return false;
      }

   public:
      ComponentController(const ComponentController&) = delete;

      virtual ~ComponentController() = default;

      ComponentController& operator=(const ComponentController&) = delete;
      ComponentController& operator=(ComponentController&&) = default;

      TInstance *insert(Cell *cell) 
      {
         xassert(m_Instances.size() < WideArraySize, "Wide Array Overflow in ComponentController");

         auto& instance = m_Instances.emplace_back();            // Push a new instance onto the array.
         instance.m_Cell = cell;

         return &instance;
      }

      void remove(TInstance *instance) 
      {
         // Get the cell mapping for the instance.
         Cell *oldCell = instance->m_Cell;

         // Get the offset for this instance in the array.
         usize objOffset = usize((uptr(instance) - uptr(m_Instances.data())) / sizeof(TInstance));

         xassert(objOffset <= WideArraySize, "offset out of range");
         xassert(objOffset < m_Instances.size(), "Out of range instance being removed");

         // It's much faster to remove an instance from an unordered array than an ordered one, unfortunately.
         if (instance == &m_Instances.back())
         {
            instance->m_Cell = nullptr;
            // Pop the last element of the instance array (this one).
            m_Instances.pop_back();
         }
         else {
            if constexpr (Ordered)
            {
               // Otherwise, we need to actually shuffle everything to fill this slot :(.
               for (usize i = objOffset + 1; i < m_Instances.size(); ++i)
               {
                  TInstance& __restrict sourceInstance = m_Instances[i];
                  TInstance& __restrict destInstance = m_Instances[i - 1];
                  Cell* thisCell = sourceInstance.m_Cell;

                  destInstance = std::move(sourceInstance);

                  ComponentControllerCommon::update_cell_instance(thisCell, &destInstance);
               }
               m_Instances.back().m_Cell = nullptr;
               m_Instances.pop_back();
            }
            else
            {
               TInstance& instanceToMove = m_Instances.back();    // Get the last instance.
               Cell* cell = instanceToMove.m_Cell;            // Get the cell that the last instance represents.

               xassert(cell != nullptr, "Null cell?!");

               instanceToMove.m_Cell = nullptr;                   // Erase the instance from the cell map.

               *instance = std::move(m_Instances.back());   // Copy the instance's data to the new instance location.
               m_Instances.pop_back();                            // Pop the last element of the instance array.

               instance->m_Cell = cell;                        // Update the cell map information for this instance.
               ComponentControllerCommon::update_cell_instance(cell, instance);                   // Notify the cell object to update its instance handles.

               xassert(objOffset < m_Instances.size(), "Size / Offset mismatch");
            }
         }
      }
   };

   template <typename TInstance>
   class SparseComponentController
   {
   protected:
      // until we have a real wide_array implementation, we need to presize it.
      static constexpr usize WideArraySize = 5'000'000ull;

      wide_array<TInstance>    				      m_Instances;
      xtd::array<uint>                          m_FreeIndices;

      SparseComponentController()
      {
         m_Instances.reserve(WideArraySize);
         m_FreeIndices.reserve(WideArraySize);
      }

      SparseComponentController(SparseComponentController&&) = default;

   public:
      bool contains_cell(Cell *cell) const 
      {
         for (const TInstance &instance : m_Instances)
         {
            if (instance.m_Cell == cell) [[unlikely]]
            {
               return true;
            }
         }
         return false;
      }

      virtual void removedInstance(TInstance *instance) = 0;
      virtual void insertedInstance(TInstance *instance) = 0;

   public:
      SparseComponentController(const SparseComponentController&) = delete;

      virtual ~SparseComponentController() = default;

      SparseComponentController& operator=(const SparseComponentController&) = delete;
      SparseComponentController& operator=(SparseComponentController&&) = default;

      TInstance *insert(Cell *cell) 
      {
         xassert(m_Instances.size() < WideArraySize, "Wide Array Overflow in ComponentController");

         TInstance* instance;

         if (m_FreeIndices.size())
         {
            instance = std::construct_at<TInstance>(&m_Instances[m_FreeIndices.back()]);
            m_FreeIndices.pop_back();
         }
         else
         {
            // Push a new instance onto the array.
            instance = &m_Instances.emplace_back();
         }

         // Set the cell mapping for that instance.
         instance->m_Cell = cell;
         instance->m_Valid = true;

         insertedInstance(instance);

         return instance;
      }

      void remove(TInstance *instance) 
      {
         xassert(instance->m_Valid, "How are we removing an invalid instance?");

         removedInstance(instance);

         instance->m_Cell = nullptr;          // Erase instance from the cell map.
         usize objOffset = usize((uptr(instance) - uptr(m_Instances.data())) / sizeof(TInstance));
         xassert(objOffset <= WideArraySize, "offset out of range");
         xassert(objOffset < m_Instances.size(), "Out of range instance being removed");
         using index_t = decltype(m_FreeIndices)::value_type;
         xassert(objOffset <= std::numeric_limits<index_t>::max(), "offset out of range");
         m_FreeIndices.push_back(index_t(objOffset));

         instance->m_Valid = false;
         std::destroy_at(instance);
      }
   };
}
