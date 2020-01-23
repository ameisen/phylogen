#pragma once

#include "Renderer/Renderer.hpp"

namespace phylo
{
   class Cell;
   namespace Render
   {
      class Controller;
      struct Instance
      {
         Renderer::InstanceData      m_RenderData;

         Cell *get_Cell() const  {
            return (Cell * )m_RenderData.CellPtr;
         }
         void set_Cell(Cell *cell)  {
            m_RenderData.CellPtr = cell;
         }

         __declspec(property(get = get_Cell, put = set_Cell)) Cell *m_Cell;

         Instance() : m_RenderData() {}
         Instance(const Instance &instance) : m_RenderData(instance.m_RenderData){}

         void unserialize(Stream &inStream, Cell *cell) ;
         void serialize(Stream &outStream) const ;
      };
   }
}
