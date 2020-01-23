#pragma once

#include "Simulation.hpp"
#include "Renderer/Renderer.hpp"
#include "Physics/PhysicsInstance.hpp"
#include "VM/VMInstance.hpp"
#include "Render/RenderInstance.hpp"

namespace phylo
{
   class Simulation;
   class Cell
   {
      friend struct VM::Instance;
      friend class Simulation;

      random::source<random::engine::xorshift_plus> m_Random;
      usize m_CellID;
	  usize m_NumChildren = 0;

      Simulation                 &m_Simulation;
      Render::Instance           *m_RenderInstance = nullptr;
      Physics::Instance          *m_PhysicsInstance = nullptr;
      VM::Instance               *m_VMInstance = nullptr;

      float                     m_fVolume = 1.0f;
      float                     m_fInvVolume = 1.0f;
      float                     m_fSuperVolume = 1.0f;
      float                     m_fInvSuperVolume = 1.0f;
      float                     m_fArea = 1.0f;
      float                     m_fInvArea = 1.0f;

      float                     m_ColorGreen = 0.0f;
      float                     m_ColorRed = 1.0f;
      float                     m_ColorBlue = 0.0f;

      Cell                       *m_KilledBy = nullptr;

      uint64                     m_Touched = 0;
      uint64                     m_Attacked;
      atomic<uint64>             m_AttackedRemote = 0;

      // This would likely be slightly faster as a double, but because we want to be able to support a closed system,
      // we need to guarantee that there is no imprecision.
      class min_t
      {
         uint64 m_Value;

      public:
         min_t(uint64 value) : m_Value(value) {}
         min_t &operator = (uint64 value) 
		 {
            m_Value = value;
            return *this;
         }

         min_t &operator -= (uint64 value) 
         {
            m_Value -= min(m_Value, value);
            return *this;
         }
         const min_t operator - (uint64 value) const 
         {
            return{ m_Value - min(m_Value, value) };
         }
         operator uint64 &()  { return m_Value; }
         operator const uint64 &() const  { return m_Value; }
      };
      uint64                       m_uObjectCapacity = 0;

      // These are stored in the 'object' slots of the cell.
      min_t                        m_uEnergy = 0;
      float                       m_Integrity = 1.0f;
      float                       m_Armor = 1.0f;
      float                       m_SelectBrightness = 0.0f;
      static constexpr float SelectBrightnessDecay = 0.003f;

      vector4F m_ColorHash1;
      vector4F m_ColorDye = { 0.0f, 1.0f, 1.0f };

      uint m_CellIdx = 0;
      bool m_TickCollided = false;
      bool m_Alive = true;

      float m_GrowthPoint = 0.0f;
      bool m_MoveState = false; // Are we moving?
      float m_MoveSpeed = 0.0f;

   public:
      Cell(const Cell * parent, Simulation &simulation, const vector2F &position, bool initialize = false);
      ~Cell();

	  usize getNumChildren() const
	  {
		  return m_NumChildren;
	  }

      usize getCellID() const 
      {
         return m_CellID;
      }

      float getRadius() const ;
      float getShadowRadius() const ;
      void setRadius(float radius) ;

      const random::source<random::engine::xorshift_plus> &getRandom() const 
      {
         return m_Random;
      }

      bool collidedThisTick() const 
      {
         return m_TickCollided;
      }

      uint64 getObjectCapacity() const 
      {
         return m_uObjectCapacity;
      }

      uint64 getUsedObjectCapacity() const 
      {
         return m_uEnergy;
      }

      uint64 getFreeObjectCapacity() const 
      {
         return getObjectCapacity() - getUsedObjectCapacity();
      }

      float getEnergyFactor() const 
      {
         return float(m_uEnergy) / float(getObjectCapacity());
      }

      uint64 getEnergy() const 
      {
         return m_uEnergy;
      }

      void setEnergy(uint64 energy) 
      {
         xassert((energy) <= m_uObjectCapacity, "Energy cannot be larger than capacity");
         // This should only be used for debugging or initial cell drop purposes. It can throw off a simulation fast.
         m_uEnergy = energy;
      }

      float getVolume() const 
      {
         return m_fVolume;
      }

      float getSuperVolume() const 
      {
         return m_fSuperVolume;
      }

      float getInverseVolume() const 
      {
         return m_fInvVolume;
      }

      float getArea() const 
      {
         return m_fArea;
      }

      float getInverseArea() const 
      {
         return m_fInvArea;
      }

      float getArmor() const 
      {
         return m_Armor;
      }

      void setArmor(float armor) 
      {
         m_Armor = armor;
      }

      void setKilledBy(Cell *cell) 
      {
         m_KilledBy = cell;
      }

      Cell *getKilledBy() const 
      {
         return m_KilledBy;
      }

      void setColorHash1(const vector4F &colorHash) ;

      void update() ;

      void update_lite() ;

      void update_instance(Physics::Instance *newInstance) 
      {
         m_PhysicsInstance = newInstance;
      }

      void update_instance(VM::Instance *newInstance) 
      {
         m_VMInstance = newInstance;
      }

      void update_instance(Render::Instance *newInstance) 
      {
         m_RenderInstance = newInstance;
      }

      void unserialize(Stream &in) ;
      void serialize(Stream &out) const ;
   };
}
