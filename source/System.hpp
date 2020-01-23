#pragma once

namespace phylo
{
   class System
   {
   public:
      virtual ~System() {}

      virtual void halt() = 0;
   };
}
