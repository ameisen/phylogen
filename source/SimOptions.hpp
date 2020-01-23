#pragma once

namespace phylo
{
   namespace options
   {
      static constexpr bool Deterministic = true;

      static constexpr float TimeMultiplier = 1.0;
      static_assert(TimeMultiplier == 1.0, "Must currently be 1.0");
      static constexpr float CapacityMultiplier = 150.0;
      static constexpr float GrowthRate = 0.005;
      static constexpr float GrowthRateMultiplier = 2.0;
	  static constexpr float LightPeriodTicks = 50000.0f;
      extern float LightMapChangeRate;

      static constexpr usize MaxBytecodeSize = 30000;

      extern int BaselineBytecodeSize;
      extern float BurnEnergyPercentage;
      extern float BaseEnergyMultiplier;
      extern float MutationSubstitutionChance;
      extern float MutationIncrementChance;
      extern float MutationDecrementChance;
      extern float MutationInsertionChance;
      extern float MutationDeletionChance;
      extern float MutationDuplicationChance;
      extern float MutationRangeDuplicationChance;
      extern float MutationRangeDeletionChance;

      extern float CodeMutationIncrementChance;
      extern float CodeMutationDecrementChance;
      extern float CodeMutationRandomChance;
      extern float CodeMutationSwapChance;

      extern float ArmorRegrowthRate;

      extern float LiveMutationChance;

      extern float CrowdingPenalty;

      extern int TickEnergyLost;
      extern int SleepTickEnergyLost;

      extern int BaseMoveCost;
      extern int BaseRotateCost;
      extern int BaseSplitCost;
      extern int BaseGrowCost;

      static constexpr float MinCellSize = 1.0;
      static constexpr float MaxCellSize = 2.5;
      static constexpr float MedianCellSize = MaxCellSize;

      static constexpr float StartWorldRadius = 128.0;
      static constexpr float WorldRadius = StartWorldRadius * MedianCellSize;
   }

   struct options_delta
   {
      float LightMapChangeRate = options::LightMapChangeRate;

      int BaselineBytecodeSize = options::BaselineBytecodeSize;
      float BurnEnergyPercentage = options::BurnEnergyPercentage;
      float BaseEnergyMultiplier = options::BaseEnergyMultiplier;
      float MutationSubstitutionChance = options::MutationSubstitutionChance;
      float MutationIncrementChance = options::MutationIncrementChance;
      float MutationDecrementChance = options::MutationDecrementChance;
      float MutationInsertionChance = options::MutationInsertionChance;
      float MutationDeletionChance = options::MutationDeletionChance;
      float MutationDuplicationChance = options::MutationDuplicationChance;
      float MutationRangeDuplicationChance = options::MutationRangeDuplicationChance;
      float MutationRangeDeletionChance = options::MutationRangeDeletionChance;

      float CodeMutationIncrementChance = options::CodeMutationIncrementChance;
      float CodeMutationDecrementChance = options::CodeMutationDecrementChance;
      float CodeMutationRandomChance = options::CodeMutationRandomChance;
      float CodeMutationSwapChance = options::CodeMutationSwapChance;

      float ArmorRegrowthRate = options::ArmorRegrowthRate;

      float LiveMutationChance = options::LiveMutationChance;

      float CrowdingPenalty = options::CrowdingPenalty;

      int TickEnergyLost = options::TickEnergyLost;
      int SleepTickEnergyLost = options::SleepTickEnergyLost;

      int BaseMoveCost = options::BaseMoveCost;
      int BaseRotateCost = options::BaseRotateCost;
      int BaseSplitCost = options::BaseSplitCost;
      int BaseGrowCost = options::BaseGrowCost;

      bool operator == (const options_delta &delta) const
      {
         return memcmp(this, &delta, sizeof(options_delta)) == 0;
      }

      bool operator != (const options_delta &delta) const
      {
         return memcmp(this, &delta, sizeof(options_delta)) != 0;
      }

      void apply()
      {
         options::LightMapChangeRate = LightMapChangeRate;
         options::BaselineBytecodeSize = BaselineBytecodeSize;
         options::BurnEnergyPercentage = BurnEnergyPercentage;
         options::BaseEnergyMultiplier = BaseEnergyMultiplier;
         options::MutationSubstitutionChance = MutationSubstitutionChance;
         options::MutationIncrementChance = MutationIncrementChance;
         options::MutationDecrementChance = MutationDecrementChance;
         options::MutationInsertionChance = MutationInsertionChance;
         options::MutationDeletionChance = MutationDeletionChance;
         options::MutationDuplicationChance = MutationDuplicationChance;
         options::MutationRangeDuplicationChance = MutationRangeDuplicationChance;
         options::MutationRangeDeletionChance = MutationRangeDeletionChance;
         options::CodeMutationIncrementChance = CodeMutationIncrementChance;
         options::CodeMutationDecrementChance = CodeMutationDecrementChance;
         options::CodeMutationRandomChance = CodeMutationRandomChance;
         options::CodeMutationSwapChance = CodeMutationSwapChance;
         options::ArmorRegrowthRate = ArmorRegrowthRate;
         options::LiveMutationChance = LiveMutationChance;
         options::CrowdingPenalty = CrowdingPenalty;
         options::TickEnergyLost = TickEnergyLost;
         options::SleepTickEnergyLost = SleepTickEnergyLost;
         options::BaseMoveCost = BaseMoveCost;
         options::BaseRotateCost = BaseRotateCost;
         options::BaseSplitCost = BaseSplitCost;
         options::BaseGrowCost = BaseGrowCost;
      }
   };

   extern const options_delta defaultOptions;
}
