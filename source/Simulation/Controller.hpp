#pragma once

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
   // To use this, your INSTANCE type must have a variable named m_Cell which is of type Cell *.
   template <typename INSTANCE, bool Ordered = false>
   class ComponentController
   {
   protected:
      // until we have a real wide_array implementation, we need to presize it.
      static constexpr usize WideArraySize = 2000000ull;

      wide_array<INSTANCE>    				      m_Instances;

      ComponentController()
      {
         m_Instances.reserve(WideArraySize);
      }

   public:
      bool contains_cell(Cell *cell) const 
      {
         for (const INSTANCE &instance : m_Instances)
         {
            if (instance.m_Cell == cell)
            {
               return true;
            }
         }
         return false;
      }

   public:
      virtual ~ComponentController()
      {
      }

      INSTANCE *insert(Cell *cell) 
      {
         xassert(m_Instances.size() < WideArraySize, "Wide Array Overflow in ComponentController");

         m_Instances.emplace_back();            // Push a new instance onto the array.
         auto *instance = &m_Instances.back();  // Get a pointer to the instance.
         instance->m_Cell = cell;

         return instance;
      }

      void remove(INSTANCE *instance) 
      {
         Cell *oldCell = instance->m_Cell;   // Get the cell mapping for the instance.

                                                // Get the offset for this instance in the array.
         uint objOffset = uint((uptr(instance) - uptr(m_Instances.data())) / sizeof(INSTANCE));

         xassert(objOffset < m_Instances.size(), "Out of range instance being removed");

         // It's much faster to remove an instance from an unordered array than an ordered one, unfortunately.
         if (instance == &m_Instances.back())
         {
            instance->m_Cell = nullptr;
            m_Instances.pop_back();             // Pop the last element of the instance array (this one).
         }
         else if (Ordered)
         {
            // Otherwise, we need to actually shuffle everything to fill this slot :(.
            for (uint i = objOffset + 1; i < m_Instances.size(); ++i)
            {
               Cell *thisCell = m_Instances[i].m_Cell;
               m_Instances[i - 1].m_Cell = thisCell;

               m_Instances[i - 1] = (INSTANCE &&)m_Instances[i];

               ComponentControllerCommon::update_cell_instance(thisCell, &m_Instances[i - 1]);
            }
            m_Instances.back().m_Cell = nullptr;
            m_Instances.pop_back();
         }
         else
         {
            INSTANCE *instanceToMove = &m_Instances.back();    // Get the last instance.
            Cell *cell = instanceToMove->m_Cell;            // Get the cell that the last instance represents.

            xassert(cell != nullptr, "Null cell?!");

            instanceToMove->m_Cell = nullptr;                   // Erase the instance from the cell map.

            *instance = (INSTANCE &&)m_Instances.back();   // Copy the instance's data to the new instance location.
            m_Instances.pop_back();                            // Pop the last element of the instance array.

            instance->m_Cell = cell;                        // Update the cell map information for this instance.
            ComponentControllerCommon::update_cell_instance(cell, instance);                   // Notify the cell object to update its instance handles.

            xassert(objOffset < m_Instances.size(), "Size / Offset mismatch");
         }
      }
   };

   template <typename INSTANCE>
   class SparseComponentController
   {
   protected:
      // until we have a real wide_array implementation, we need to presize it.
      static constexpr usize WideArraySize = 5000000ull;

      wide_array<INSTANCE>    				      m_Instances;
      xtd::array<uint>                         m_FreeIndices;

      SparseComponentController()
      {
         m_Instances.reserve(WideArraySize);
         m_FreeIndices.reserve(WideArraySize);
      }

   public:
      bool contains_cell(Cell *cell) const 
      {
         for (const INSTANCE &instance : m_Instances)
         {
            if (instance.m_Cell == cell)
            {
               return true;
            }
         }
         return false;
      }

      virtual void removedInstance(INSTANCE *instance)  = 0;
      virtual void insertedInstance(INSTANCE *instance)  = 0;

   public:
      virtual ~SparseComponentController()
      {
      }

      INSTANCE *insert(Cell *cell) 
      {
         xassert(m_Instances.size() < WideArraySize, "Wide Array Overflow in ComponentController");

         if (m_FreeIndices.size())
         {
            auto *instance = &m_Instances[m_FreeIndices.back()];
            new (instance) INSTANCE;
            m_FreeIndices.pop_back();
            instance->m_Cell = cell;            // Set the cell mapping for that instance.

            instance->m_Valid = true;
            insertedInstance(instance);
            return instance;
         }
         else
         {
            m_Instances.emplace_back();            // Push a new instance onto the array.
            auto *instance = &m_Instances.back();  // Get a pointer to the instance.
            instance->m_Cell = cell;            // Set the cell mapping for that instance.

            instance->m_Valid = true;
            insertedInstance(instance);
            return instance;
         }
      }

      void remove(INSTANCE *instance) 
      {
         xassert(instance->m_Valid, "How are we removing an invalid instance?");

         removedInstance(instance);

         instance->m_Cell = nullptr;          // Erase instance from the cell map.
         uint objOffset = uint((uptr(instance) - uptr(m_Instances.data())) / sizeof(INSTANCE));
         xassert(objOffset < m_Instances.size(), "Out of range instance being removed");
         m_FreeIndices.push_back(objOffset);

         instance->m_Valid = false;
         instance->~INSTANCE();
      }
   };
}
