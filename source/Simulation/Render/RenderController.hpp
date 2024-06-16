#pragma once

#include "RenderInstance.hpp"
#include "ThreadPool.hpp"
#include "Simulation/Controller.hpp"

namespace phylo
{
   class Simulation;
   namespace Render
   {
      class Controller final : public ComponentController<Render::Instance, true>
      {
         using instance_t = Render::Instance;

         Simulation     &m_Simulation;

      public:
         Controller() = delete;
         explicit Controller(Simulation &simulation);
         ~Controller() override;

         void update()  ;

         const wide_array<Renderer::InstanceData> &getRawData() const 
         {
            return *reinterpret_cast<const wide_array<Renderer::InstanceData>*>(&m_Instances); // This is one of the hackiest things here.
         }
      };
   }
}
