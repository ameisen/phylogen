#include "phylogen.hpp"

namespace phylo
{
   namespace options
   {
      float LightMapChangeRate = 0.00005;

      int BaselineBytecodeSize = 1024;
      float BurnEnergyPercentage = 0.05;
      float BaseEnergyMultiplier = (195.0f) * TimeMultiplier;
      float MutationSubstitutionChance = 0.0025;
      float MutationIncrementChance = 0.0025;
      float MutationDecrementChance = 0.0025;
      float MutationInsertionChance = 0.0045;
      float MutationDeletionChance = 0.0025;
      float MutationDuplicationChance = 0.0045;
      float MutationRangeDuplicationChance = 0.0045;
      float MutationRangeDeletionChance = 0.0045;

      float CodeMutationIncrementChance = 0.01;
      float CodeMutationDecrementChance = 0.01;
      float CodeMutationRandomChance = 0.01;
      float CodeMutationSwapChance = 0.01;

      float ArmorRegrowthRate = 0.0001;

      float LiveMutationChance = 0.00001;

      float CrowdingPenalty = 0.001;

      int TickEnergyLost = int((100u) * TimeMultiplier);
      int SleepTickEnergyLost = int((25u) * TimeMultiplier);

      int BaseMoveCost = int((75u) * TimeMultiplier);
      int BaseRotateCost = int((10u) * TimeMultiplier);
      int BaseSplitCost = int((10u) * TimeMultiplier);
      int BaseGrowCost = int((100000u) * TimeMultiplier);
   }

   const options_delta defaultOptions;
}
