#include "phylogen.hpp"

namespace phylo
{
   namespace options
   {
      float LightMapChangeRate = 0.00005f;

      int BaselineBytecodeSize = 1024;
      float BurnEnergyPercentage = 0.05f;
      float BaseEnergyMultiplier = (195.0f) * TimeMultiplier;
      float MutationSubstitutionChance = 0.0025f;
      float MutationIncrementChance = 0.0025f;
      float MutationDecrementChance = 0.0025f;
      float MutationInsertionChance = 0.0045f;
      float MutationDeletionChance = 0.0025f;
      float MutationDuplicationChance = 0.0045f;
      float MutationRangeDuplicationChance = 0.0045f;
      float MutationRangeDeletionChance = 0.0045f;

      float CodeMutationIncrementChance = 0.01f;
      float CodeMutationDecrementChance = 0.01f;
      float CodeMutationRandomChance = 0.01f;
      float CodeMutationSwapChance = 0.01f;

      float ArmorRegrowthRate = 0.0001f;

      float LiveMutationChance = 0.00001f;

      float CrowdingPenalty = 0.001f;

      int TickEnergyLost = int((100u) * TimeMultiplier);
      int SleepTickEnergyLost = int((25u) * TimeMultiplier);

      int BaseMoveCost = int((75u) * TimeMultiplier);
      int BaseRotateCost = int((10u) * TimeMultiplier);
      int BaseSplitCost = int((10u) * TimeMultiplier);
      int BaseGrowCost = int((100000u) * TimeMultiplier);
   }

   const options_delta defaultOptions;
}
