#pragma once

#include "System.hpp"
#include "Simulation\VM\VMInstance.hpp"

#include <d3d11.h>
#include <d3d11_3.h>

#include <shared_mutex>

namespace phylo
{
   // A discrete, marshallable Renderer object that encapsulates all state.
   class Simulation;
   class Cell;
   class Renderer final : public System
   {
   public: // SimThread uses this to store render component data.
      struct InstanceData
      {
         matrix4F Transform;
         vector4F Color1;
         vector4F Color2;
         vector4F TrendVelocity;
         float    Radius;
         float    TimeOffset;
         Cell*    CellPtr;
         vector4F Armor_NucleusScale;
      };

      struct UIData
      {
         uint64		      TotalCells = 0;
         uint64			  CurTick = 0;
         clock::time_span totalTime;
         clock::time_span vmTime;
         clock::time_span physicsTime;
         clock::time_span updateTime;
         clock::time_span renderUpdateTime;
         clock::time_span postTime;
         clock::time_span lightTime;
         clock::time_span parallelTime;
         clock::time_span serialTime;
         uint NumCells = 0;
         uint speedState = 4;
      };

      enum class RenderMode : uint32
      {
         Normal,
         HashCode,
         Dye
      };
   private:

      Simulation* m_Simulation;
      atomic<Simulation* > m_NewSimulation;
      mutex          m_RenderLock;

      window* m_pWindow = nullptr;
      vector2I       m_CurrentResolution;

      thread         m_DrawThread;
      atomic<bool>   m_DrawThreadRun = true;
      mutable atomic<bool>	 m_SignalFrameReady = true; // This is used by the Simulation thread to determine if now is a good time to pass data.

      ID3D11Device* m_pDevice = nullptr;
      ID3D11Device3* m_pDevice3 = nullptr;
      IDXGISwapChain* m_pSwapChain = nullptr;
      ID3D11DeviceContext* m_pContext = nullptr;

      array<IUnknown* >          m_RenderResources;
      array<IUnknown* >          m_RenderTargets;

      D3D11_VIEWPORT             m_Viewport;

      vector2D                   m_CameraOffset;
      vector2D                   m_DirtyCameraOffset;
      int                        m_Zoom = 0;
      double                     m_DirtyZoomScalar = 1.0;
      double                     m_ZoomScalar = 1.0;
      mutex                      m_CameraLock;

      matrix4F                   m_ViewProjection;

      mutex                      m_FramePendingLock;
      uint64                     m_CurrentFrame = 0;
      atomic<uint64>             m_PendingFrame = 0;
      wide_array<InstanceData>   m_PendingArray;
      wide_array<InstanceData>   m_CurrentInstanceData;

      array<uint8>               m_PendingLightData;
      array<uint8>               m_CurrentLightData;
      uint                       m_LightEdge;
      float						 m_Illumination;

      array<float>               m_PendingWasteData;
      array<float>              m_CurrentWasteData;
      uint                       m_WasteEdge;

      UIData                     m_UIData;
      string					 m_HashName;
      clock::time_span           m_RenderCPUTime;

      atomic<bool>               m_SettingsOpenRequested = { false };
      atomic<bool>               m_InstructionStatsOpenRequested = { false };

      array<uint32, VM::NumOperations + 1>   m_InstructionTracker;

      std::shared_mutex          m_InstructionCheckboxLock;
      bool                       m_InstructionCheckboxes[traits<uint16>::max];

      atomic<bool>               m_ForceUpdate = { false };

      RenderMode m_RenderMode = RenderMode::Normal;

      struct _RenderState
      {
         matrix4F Projection;
         matrix4F View;
         matrix4F VP;
         vector4F Time;
         vector4F Color;
         vector4F Scale_ColorLerp_MoveForward = { 1.0f, 0.0f, 0.0f, 0.0f };
         vector4F WorldRadius_Lit_NuclearScaleLerp;
      } m_RenderState;

      struct _State
      {
         friend class Renderer;
      private:
         _State() = default;

         ID3D11DepthStencilState* m_NoDepthStencilState = nullptr;
         ID3D11BlendState* m_TransparentBlendState = nullptr;
      } State;

      // Shaders - a container struct only.
      struct _Shader
      {
         friend class Renderer;
      private:
         _Shader() = default;

         ID3D11VertexShader* m_pCircleVertexShader = nullptr;
         ID3D11PixelShader* m_pCirclePixelShader = nullptr;

         ID3D11VertexShader* m_pCircleJitterVertexShader = nullptr;
         ID3D11PixelShader* m_pCircleJitterPixelShader = nullptr;

         ID3D11VertexShader* m_pCircleConstColorVertexShader = nullptr;
         ID3D11PixelShader* m_pCircleConstColorPixelShader = nullptr;

         ID3D11VertexShader* m_pCircleMultiplyVertexShader = nullptr;
         ID3D11PixelShader* m_pCircleMultiplyPixelShader = nullptr;

         ID3D11VertexShader* m_pCircleBackdropVertexShader = nullptr;
         ID3D11PixelShader* m_pCircleBackdropPixelShader = nullptr;
      } Shader;

      // Buffers - a container struct only.
      struct _Buffer
      {
         friend class Renderer;
      private:
         _Buffer() = default;

         ID3D11Buffer* m_pSquareVertexBuffer = nullptr;
         ID3D11InputLayout* m_pSquareInputLayout = nullptr;
         uint                       m_uSquareStride = 0;
         uint                       m_uSquareVertices = 0;

         ID3D11Buffer* m_pRenderStateBuffer = nullptr;

         ID3D11Buffer* m_pInstanceBuffer = nullptr;
         ID3D11ShaderResourceView* m_pShaderResourceView = nullptr;
         uint                       m_uInstanceBufferElements = 0;

         ID3D11Texture2D* m_pLightTexture = nullptr;
         ID3D11ShaderResourceView* m_pLightTextureView = nullptr;
         ID3D11SamplerState* m_pLightSamplerState = nullptr;

         ID3D11Texture2D* m_pWasteTexture = nullptr;
         ID3D11ShaderResourceView* m_pWasteTextureView = nullptr;
         ID3D11SamplerState* m_pWasteSamplerState = nullptr;
      } Buffer;

      // Targets - render targets
      struct _Target
      {
         friend class Renderer;
      private:
         _Target() = default;

         ID3D11Texture2D* m_pBackbufferTexture = nullptr;

         ID3D11Texture2D* m_pRenderTargetTexture = nullptr;
         ID3D11RenderTargetView* m_pRenderTargetView = nullptr;
         ID3D11Texture2D* m_pDepthTargetTexture = nullptr;
         ID3D11DepthStencilView* m_pDepthTargetView = nullptr;
      } Target;

      void rebuild();
      void release_all();
      void init_all();

      void render_loop();
      void render_frame(clock::time_point timeAtRenderStart);
      void render_circles(usize instanceCount, const InstanceData* instanceData);
      void render_circles_jitter(usize instanceCount, const InstanceData* instanceData);
      void render_circles_backdrop(usize instanceCount, const InstanceData* instanceData);
      void render_circles_constcolor(usize instanceCount, const InstanceData* instanceData);
      void render_circles_multiply(usize instanceCount, const InstanceData* instanceData);
      void init_resources();

      void clear_targets();
      void init_targets();

      void render_ui();

   public:
      Renderer(Simulation* simulation, window* outputWindow);
      virtual ~Renderer() override;

      RenderMode getRenderMode() const
      {
         return m_RenderMode;
      }

      virtual void halt();

      bool get_checkbox_state(uint16 instruction)
      {
         //std::shared_lock<decltype(m_InstructionCheckboxLock)> _lock(m_InstructionCheckboxLock);
         return m_InstructionCheckboxes[instruction];
      }

      window* get_window() const { return m_pWindow; }

      void move_screen(const vector2D& mouseMove);
      void set_screen_position(const vector2D& position);
      void adjust_zoom(int zoomDelta);
      vector2D unproject(const vector2D& screenPos);

      void update_from_sim(float light, uint64 frame, const wide_array<InstanceData>& instanceData, const array<uint8>& lightData, uint lightEdge, const array<uint32>& wasteData, uint wasteEdge, Renderer::UIData& uiData, const array<atomic<uint32>, VM::NumOperations + 1>& VMInstructionTrackerArray);
      void update_from_sim(const Renderer::UIData& uiData);
      void update_hash_name(const string_view& hash_name);

      void setSimulation(Simulation* simulation) {
         scoped_lock _lock(m_RenderLock);
         m_NewSimulation = simulation;
      }

      bool is_frame_ready() const
      {
         return m_SignalFrameReady.exchange(false);
      }

      void openInstructionStats();
      void openSettings();

      void force_update()
      {
         m_ForceUpdate = true;
      }

      void resetFrame()
      {
         scoped_lock         _lock(m_FramePendingLock);
         m_CurrentFrame = 0;
         m_PendingFrame = 0;
         m_InstructionStatsOpenRequested = false;
         m_SettingsOpenRequested = false;
      }
   };
}
