#pragma once

namespace phylo
{
   class System
   {
   public:
      virtual ~System() = default;

      virtual void halt() = 0;
   };
}
