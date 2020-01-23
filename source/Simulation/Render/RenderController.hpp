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
		 typedef Render::Instance instance_t;

         Simulation     &m_Simulation;

      public:
         Controller() = delete;
         Controller(Simulation &simulation);
         ~Controller();

         void update()  ;

         const wide_array<Renderer::InstanceData> &getRawData() const 
         {
            return *(const wide_array<Renderer::InstanceData> *)&m_Instances; // This is one of the hackiest things here. But it works. I don't feel like explaining why to myself.
         }
      };
   }
}
