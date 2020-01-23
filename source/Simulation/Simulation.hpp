#pragma once

#include "System.hpp"
#include "Renderer/Renderer.hpp" // Need the renderer component struct.
#include "Physics/PhysicsController.hpp"
#include "VM/VMController.hpp"
#include "Render/RenderController.hpp"
#include "Cell.hpp"

#include <Noise.h>

namespace phylo
{
   // A discrete, marshallable Simulation object that encapsulates all state.
   class Simulation final : public System
   {
      static constexpr bool SINGLE_THREADED = false;

      friend class Cell;

   public:
      enum class SpeedState : uint
      {
         Pause = 0,
         Slow,
         Medium,
         Fast,
         Ludicrous,
      };
   private:
      uint64         m_uCurrentFrame = 1ull;

      ThreadPool                 m_ThreadPool;
      ThreadPool                 m_ThreadPool2;
      ThreadPool                 m_ThreadPool3;
      ThreadPool                 m_ThreadPool4;
      atomic<uint>              m_ThreadPoolIndex;

      thread         m_SimThread;
      atomic<bool>   m_SimThreadRun = true;

      // This should probably be wrapped in a mutex.
      Renderer       *m_pRenderer = nullptr;

      wide_array<Cell * >                  m_Cells;
	  atomic<uint64>					   m_TotalCells{ 0 };

      mutex m_ClickLock;
      struct Click
      {
         vector2F Position;
         bool State;
      };
      array<Click>   m_Clicks;
      mutex          m_DestroySerializedLock;
      array<Cell * >    m_DestroyTasks;
      atomic<uint>   m_SimulationLockAtomic = { 0 };
      mutex          m_SimulationLock;
      uint64         m_LoadedFrame = traits<uint64>::ones;
      atomic<uint>   m_Saving = { false };
      atomic<bool>   m_Loading = { false };

      Physics::Controller        m_PhysicsController;
      VM::Controller             m_VMController;
      Render::Controller         m_RenderController;

   public:
	  clock::time_span m_TotalParallelTime;
	  clock::time_span m_TotalSerialTime;
   private:

      event                      m_KickoffEvent;

      struct GridProperties
      {
         float                   m_GridElementSize;
         float                   m_GridElementSizeHalf;
         float                   m_InvGridElementSize;
         uint32                  m_GridElementsEdge;
         array<vector2F>          m_GridElementsPositions;
         void init() ;
      };

      template <typename T>
      struct Grid : public GridProperties
      {
         array<T>                m_GridElements;
         Grid() = default;
         Grid(const GridProperties &props) : GridProperties(props) {}

         void init() 
         {
            m_GridElements.resize(this->m_GridElementsEdge * this->m_GridElementsEdge); // we stick new elements in the last one.
            GridProperties::init();
         }
      };

      template <typename T>
      struct AtomicGrid final : public Grid<T>
      {
         array<T>        m_AtomicGridElements; // TODO array<atomic<T>> doesn't work
         AtomicGrid() = default;
         AtomicGrid(const GridProperties &props) : AtomicGrid(props) {}

         void init() 
         {
            m_AtomicGridElements.resize(this->m_GridElementsEdge * this->m_GridElementsEdge); // we stick new elements in the last one.
            Grid<T>::init();
         }
      };

      GridProperties          m_SimGrid;
      Grid<uint8>             m_LightGrid;
      AtomicGrid<uint32>      m_WasteGrid;
      float                   m_LightmapZ = 0.0f;
      uint                     m_CurProcessRow = 0;
      noisepp::Cache           *m_pNoiseCache = nullptr;
      noisepp::Pipeline3D      m_NoisePipeline;
      noisepp::PerlinModule    m_PerlinModule;
      noisepp::PipelineElement3D *m_NoiseSrc;
      uint32 GetInstanceOffset(const vector2F &position) const ;
      uint32 GetLightInstanceOffset(const vector2F &position) const ;
      uint32 GetWasteInstanceOffset(const vector2F &position) const ;

      bool                     m_Flashlight = false;
      vector2F                 m_FlashlightPos;
	  float					   m_Illumination = 0.0f;

      static constexpr uint MaxNumCells = 100000;
      uint8                   *m_NextFreeCell = nullptr;
      uint8                   *m_CellStore = nullptr;

      void pool_update() ;
      void pool_update2() ;
      void pool_update_waste() ;
      void pool_updatelite() ;

      void spawn_initial_cell() ;

      SpeedState m_SpeedState = SpeedState::Ludicrous;
      bool m_Step = false;

      mutex                   m_CallbackLock;
      array<function<void(Simulation * )>> m_Callbacks;

      const string               m_HashName;
      uint64                     m_HashSeed;

      struct loadInitializer
      {
         string                  m_HashName;
         uint64                   m_uCurrentFrame;
         uint32                   m_SimGridElementsEdge;
         uint32                   m_LightGridElementsEdge;
         uint32                   m_WasteGridElementsEdge;
         float                    m_LightmapZ;
         uint                     m_CurProcessRow;
         int                      m_NoiseSeed;
         uint64                   g_CellID;
      };
      
      void startSave(thread & saveGatherThread, Stream & outStream) ;
      void doSave(Stream & outStream, const string_view & dest) ;

   public:
      Simulation(event &startProcessing, event &waitThreadProcessing, const loadInitializer &init);
      Simulation(const string &hashName, event &startProcessing, event &waitThreadProcessing);
      Simulation(const string &hashName);
      virtual ~Simulation();

      uint64 get_hash_seed()  const { return m_HashSeed; }

      bool is_saving()  const { return m_Saving != 0; }
      bool is_loading()  const { return m_Loading; }

	  uint64 get_current_frame() const
	  {
		  return m_uCurrentFrame;
	  }

      void kickoff() 
      {
         m_KickoffEvent.set();
      }

      mutex &getSimulationLock() 
      {
         return m_SimulationLock;
      }

      void set_renderer(Renderer *renderer) 
      {
         m_pRenderer = renderer;
      }

      Renderer *get_renderer() const 
      {
         return m_pRenderer;
      }

      void sim_loop() ;

      void on_click(const vector2F &pos, bool state) ;

      uint get_num_cells() const 
      {
         return m_Cells.size();
      }

      void SetSpeedState(SpeedState state) 
      {
         m_SpeedState = state;
      }

      void SetStep() 
      {
         m_Step = true;
      }

      void pushCallback(function<void(Simulation * )> func) 
      {
         scoped_lock<mutex> _lock(m_CallbackLock);
         m_Callbacks += func;
      }

      float getGreenEnergy(const vector2F &position) const ;
      float getRedEnergy(const vector2F &position) const ;
      uint32 getBlueEnergy(const vector2F &position) const ;

      float getGreenEnergy(uint offset) const ;
      float getRedEnergy(uint offset) const ;
      uint32 getBlueEnergy(uint offset) const ;

      void decreaseBlueEnergy(uint offset, uint32 amount) ;

      // Internal Public functions
      Cell &getNewCell(const Cell *parent) ;
      void killCell(Cell &cell) ;
      void beEatenCell(Cell &cell) ;
      void destroyCell(Cell &cell) ;
      Cell *findCell(const vector2F &position, float radius, Cell *filter) const ;

      virtual void halt() ;

      Cell *getNewCellPtr() ;
      void freeCellPtr(Cell *cell) ;

      void openInstructionStats() ;
      void openSettings() ;
      void newSim(const string &hashName) ;
      void onLoad() ;
      mutex SaveLock;
      void onSave() ;
      void Autosave() ;
   };
}
