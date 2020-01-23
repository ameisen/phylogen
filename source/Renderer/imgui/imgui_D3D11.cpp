#include "phylogen.hpp"
#ifndef NOMINMAX
#  define NOMINMAX 1
#endif
#include <Windows.h>
#include "imgui.h"

#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

void    ImGui_ImplDX11_InvalidateDeviceObjects();
bool    ImGui_ImplDX11_CreateDeviceObjects();
bool    ImGui_ImplDX11_Init(void*hwnd, ID3D11Device*device, ID3D11DeviceContext*device_context);

static INT64                    g_Time = 0;
static INT64                    g_TicksPerSecond = 0;

static HWND                     g_hWnd = 0;
static ID3D11Device*          g_pd3dDevice = nullptr;
static ID3D11DeviceContext*   g_pd3dDeviceContext = nullptr;
static ID3D11Buffer*          g_pVB = nullptr;
static ID3D11Buffer*          g_pIB = nullptr;
static ID3D11VertexShader*    g_pVertexShader = nullptr;
static ID3D11InputLayout*     g_pInputLayout = nullptr;
static ID3D11Buffer*          g_pVertexConstantBuffer = nullptr;
static ID3D11PixelShader*     g_pPixelShader = nullptr;
static ID3D11SamplerState*    g_pFontSampler = nullptr;
static ID3D11ShaderResourceView*g_pFontTextureView = nullptr;
static ID3D11RasterizerState* g_pRasterizerState = nullptr;
static ID3D11BlendState*      g_pBlendState = nullptr;
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;

struct VERTEX_CONSTANT_BUFFER
{
   float        mvp[4][4];
};

uptr imgui_wndproc_hook(void *hWnd, uint msg, uint64 wParam, uint64 lParam)
{
   ImGuiIO& io = ImGui::GetIO();

   switch (msg)
   {
   case WM_LBUTTONDOWN:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[0] = true;
      return true;
   case WM_LBUTTONUP:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[0] = false;
      return true;
   case WM_RBUTTONDOWN:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[1] = true;
      return true;
   case WM_RBUTTONUP:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[1] = false;
      return true;
   case WM_MBUTTONDOWN:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[2] = true;
      return true;
   case WM_MBUTTONUP:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseDown[2] = false;
      return true;
   case WM_MOUSEWHEEL:
      if (!io.WantCaptureMouse)
         return false;
      io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
      return true;
   case WM_MOUSEMOVE:
      io.MousePos.x = (signed short)(lParam);
      io.MousePos.y = (signed short)(lParam >> 16);
      return (!!io.WantCaptureMouse);
   case WM_KEYDOWN:
      if (!io.WantCaptureKeyboard)
         return false;
      if (wParam < 256)
         io.KeysDown[wParam] = 1;
      return true;
   case WM_KEYUP:
      //if (!io.WantCaptureKeyboard)
      //   return false;
      if (wParam < 256)
         io.KeysDown[wParam] = 0;
      return true;
   case WM_CHAR:
      if (!io.WantTextInput)
         return false;
      // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
      if (wParam > 0 && wParam < 0x10000)
         io.AddInputCharacter((unsigned short)wParam);
      return true;
   }

   return 0;
}

void ImGui_ImplDX11_RenderDrawLists(ImDrawData* draw_data)
{
   // Create and grow vertex/index buffers if needed
   if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
   {
      if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }
      g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
      D3D11_BUFFER_DESC desc;
      memset(&desc, 0, sizeof(D3D11_BUFFER_DESC));
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.ByteWidth = g_VertexBufferSize * sizeof(ImDrawVert);
      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags = 0;
      if (g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pVB) < 0)
         return;
   }
   if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
   {
      if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }
      g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
      D3D11_BUFFER_DESC bufferDesc;
      memset(&bufferDesc, 0, sizeof(D3D11_BUFFER_DESC));
      bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
      bufferDesc.ByteWidth = g_IndexBufferSize * sizeof(ImDrawIdx);
      bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
      bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      if (g_pd3dDevice->CreateBuffer(&bufferDesc, nullptr, &g_pIB) < 0)
         return;
   }

   // Copy and convert all vertices into a single contiguous buffer
   D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
   if (g_pd3dDeviceContext->Map(g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource) != S_OK)
      return;
   if (g_pd3dDeviceContext->Map(g_pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource) != S_OK)
      return;
   ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
   ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
   for (int n = 0; n < draw_data->CmdListsCount; n++)
   {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      memcpy(vtx_dst, &cmd_list->VtxBuffer[0], cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
      memcpy(idx_dst, &cmd_list->IdxBuffer[0], cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
      vtx_dst += cmd_list->VtxBuffer.size();
      idx_dst += cmd_list->IdxBuffer.size();
   }
   g_pd3dDeviceContext->Unmap(g_pVB, 0);
   g_pd3dDeviceContext->Unmap(g_pIB, 0);

   // Setup orthographic projection matrix into our constant buffer
   {
      D3D11_MAPPED_SUBRESOURCE mappedResource;
      if (g_pd3dDeviceContext->Map(g_pVertexConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) != S_OK)
         return;

      VERTEX_CONSTANT_BUFFER* pConstantBuffer = (VERTEX_CONSTANT_BUFFER*)mappedResource.pData;
      const float L = 0.0f;
      const float R = ImGui::GetIO().DisplaySize.x;
      const float B = ImGui::GetIO().DisplaySize.y;
      const float T = 0.0f;
      const float mvp[4][4] =
      {
         { 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
         { 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
         { 0.0f,         0.0f,           0.5f,       0.0f },
         { (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
      };
      memcpy(&pConstantBuffer->mvp, mvp, sizeof(mvp));
      g_pd3dDeviceContext->Unmap(g_pVertexConstantBuffer, 0);
   }

   // Setup viewport
   {
      D3D11_VIEWPORT vp;
      memset(&vp, 0, sizeof(D3D11_VIEWPORT));
      vp.Width = ImGui::GetIO().DisplaySize.x;
      vp.Height = ImGui::GetIO().DisplaySize.y;
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;
      vp.TopLeftX = 0;
      vp.TopLeftY = 0;
      g_pd3dDeviceContext->RSSetViewports(1, &vp);
   }

   // Bind shader and vertex buffers
   unsigned int stride = sizeof(ImDrawVert);
   unsigned int offset = 0;
   g_pd3dDeviceContext->IASetInputLayout(g_pInputLayout);
   g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
   g_pd3dDeviceContext->IASetIndexBuffer(g_pIB, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
   g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   g_pd3dDeviceContext->VSSetShader(g_pVertexShader, nullptr, 0);
   g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &g_pVertexConstantBuffer);
   g_pd3dDeviceContext->PSSetShader(g_pPixelShader, nullptr, 0);
   g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_pFontSampler);

   // Setup render state
   const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
   g_pd3dDeviceContext->OMSetBlendState(g_pBlendState, blendFactor, 0xffffffff);
   g_pd3dDeviceContext->RSSetState(g_pRasterizerState);

   // Render command lists
   int vtx_offset = 0;
   int idx_offset = 0;
   for (int n = 0; n < draw_data->CmdListsCount; n++)
   {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
      {
         const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
         if (pcmd->UserCallback)
         {
            pcmd->UserCallback(cmd_list, pcmd);
         }
         else
         {
            const D3D11_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
            g_pd3dDeviceContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&pcmd->TextureId);
            g_pd3dDeviceContext->RSSetScissorRects(1, &r);
            g_pd3dDeviceContext->DrawIndexed(pcmd->ElemCount, idx_offset, vtx_offset);
         }
         idx_offset += pcmd->ElemCount;
      }
      vtx_offset += cmd_list->VtxBuffer.size();
   }

   // Restore modified state
   g_pd3dDeviceContext->IASetInputLayout(nullptr);
   g_pd3dDeviceContext->PSSetShader(nullptr, nullptr, 0);
   g_pd3dDeviceContext->VSSetShader(nullptr, nullptr, 0);
}

static void ImGui_ImplDX11_CreateFontsTexture()
{
   // Build texture atlas
   ImGuiIO& io = ImGui::GetIO();
   unsigned char* pixels;
   int width, height;
   io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

   // Upload texture to graphics system
   {
      D3D11_TEXTURE2D_DESC texDesc;
      ZeroMemory(&texDesc, sizeof(texDesc));
      texDesc.Width = width;
      texDesc.Height = height;
      texDesc.MipLevels = 1;
      texDesc.ArraySize = 1;
      texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      texDesc.SampleDesc.Count = 1;
      texDesc.Usage = D3D11_USAGE_DEFAULT;
      texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      texDesc.CPUAccessFlags = 0;

      ID3D11Texture2D *pTexture = nullptr;
      D3D11_SUBRESOURCE_DATA subResource;
      subResource.pSysMem = pixels;
      subResource.SysMemPitch = texDesc.Width * 4;
      subResource.SysMemSlicePitch = 0;
      g_pd3dDevice->CreateTexture2D(&texDesc, &subResource, &pTexture);

      // Create texture view
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
      ZeroMemory(&srvDesc, sizeof(srvDesc));
      srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
      srvDesc.Texture2D.MostDetailedMip = 0;
      g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &g_pFontTextureView);
      pTexture->Release();
   }

   // Store our identifier
   io.Fonts->TexID = (void *)g_pFontTextureView;

   // Create texture sampler
   {
      D3D11_SAMPLER_DESC samplerDesc;
      ZeroMemory(&samplerDesc, sizeof(samplerDesc));
      samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      samplerDesc.MipLODBias = 0.f;
      samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      samplerDesc.MinLOD = 0.f;
      samplerDesc.MaxLOD = 0.f;
      g_pd3dDevice->CreateSamplerState(&samplerDesc, &g_pFontSampler);
   }
}

bool    ImGui_ImplDX11_CreateDeviceObjects()
{
   if (!g_pd3dDevice)
      return false;
   if (g_pFontSampler)
      ImGui_ImplDX11_InvalidateDeviceObjects();

   // Create the vertex shader
   {
      io::file vertexShaderFile("Assets/imgui.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
      HRESULT hRes = g_pd3dDevice->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &g_pVertexShader);
      xassert(hRes == S_OK, "Failed to create imgui Vertex Shader");
      if (hRes != S_OK)
      {
         return false;
      }

      // Create the input layout
      D3D11_INPUT_ELEMENT_DESC localLayout[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (size_t)(&((ImDrawVert*)0)->col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
      };

      if (g_pd3dDevice->CreateInputLayout(localLayout, 3, vertexShaderFile.at(0), vertexShaderFile.get_size(), &g_pInputLayout) != S_OK)
         return false;

      // Create the constant buffer
      {
         D3D11_BUFFER_DESC cbDesc;
         cbDesc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER);
         cbDesc.Usage = D3D11_USAGE_DYNAMIC;
         cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
         cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
         cbDesc.MiscFlags = 0;
         g_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &g_pVertexConstantBuffer);
      }
   }

   // Create the pixel shader
   {
      io::file vertexShaderFile("Assets/imgui.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
      HRESULT hRes = g_pd3dDevice->CreatePixelShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &g_pPixelShader);
      xassert(hRes == S_OK, "Failed to create imgui pixel Shader");
      if (hRes != S_OK)
      {
         return false;
      }

   }

   // Create the blending setup
   {
      D3D11_BLEND_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.AlphaToCoverageEnable = false;
      desc.RenderTarget[0].BlendEnable = true;
      desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
      desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
      desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
      desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
      desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
      desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
      desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      g_pd3dDevice->CreateBlendState(&desc, &g_pBlendState);
   }

   // Create the rasterizer state
   {
      D3D11_RASTERIZER_DESC desc;
      ZeroMemory(&desc, sizeof(desc));
      desc.FillMode = D3D11_FILL_SOLID;
      desc.CullMode = D3D11_CULL_NONE;
      desc.ScissorEnable = true;
      desc.DepthClipEnable = true;
      g_pd3dDevice->CreateRasterizerState(&desc, &g_pRasterizerState);
   }

   ImGui_ImplDX11_CreateFontsTexture();

   return true;
}

void    ImGui_ImplDX11_InvalidateDeviceObjects()
{
   if (!g_pd3dDevice)
      return;

   if (g_pFontSampler) { g_pFontSampler->Release(); g_pFontSampler = nullptr; }
   if (g_pFontTextureView) { g_pFontTextureView->Release(); g_pFontTextureView = nullptr; ImGui::GetIO().Fonts->TexID = 0; }
   if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }
   if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }

   if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }
   if (g_pRasterizerState) { g_pRasterizerState->Release(); g_pRasterizerState = nullptr; }
   if (g_pPixelShader) { g_pPixelShader->Release(); g_pPixelShader = nullptr; }
   if (g_pVertexConstantBuffer) { g_pVertexConstantBuffer->Release(); g_pVertexConstantBuffer = nullptr; }
   if (g_pInputLayout) { g_pInputLayout->Release(); g_pInputLayout = nullptr; }
   if (g_pVertexShader) { g_pVertexShader->Release(); g_pVertexShader = nullptr; }
}

static bool initialized = false;

bool    ImGui_ImplDX11_Init(void*hwnd, ID3D11Device*device, ID3D11DeviceContext*device_context)
{
   if (!initialized)
   {
      g_hWnd = (HWND)hwnd;
      g_pd3dDevice = device;
      g_pd3dDeviceContext = device_context;

      if (!QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
         return false;
      if (!QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
         return false;

      ImGuiIO& io = ImGui::GetIO();
      io.KeyMap[ImGuiKey_Tab] = VK_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
      io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
      io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
      io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
      io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
      io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
      io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
      io.KeyMap[ImGuiKey_Home] = VK_HOME;
      io.KeyMap[ImGuiKey_End] = VK_END;
      io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
      io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
      io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
      io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
      io.KeyMap[ImGuiKey_A] = 'A';
      io.KeyMap[ImGuiKey_C] = 'C';
      io.KeyMap[ImGuiKey_V] = 'V';
      io.KeyMap[ImGuiKey_X] = 'X';
      io.KeyMap[ImGuiKey_Y] = 'Y';
      io.KeyMap[ImGuiKey_Z] = 'Z';

      io.RenderDrawListsFn = ImGui_ImplDX11_RenderDrawLists;  // Alternatively you can set this to nullptr and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
      io.ImeWindowHandle = g_hWnd;

      initialized = true;
   }
   return true;
}

void ImGui_ImplDX11_Shutdown()
{
   ImGui_ImplDX11_InvalidateDeviceObjects();
   ImGui::Shutdown();
   g_pd3dDevice = nullptr;
   g_pd3dDeviceContext = nullptr;
   g_hWnd = (HWND)0;
}

void ImGui_ImplDX11_NewFrame()
{
   if (!g_pFontSampler)
      ImGui_ImplDX11_CreateDeviceObjects();

   ImGuiIO& io = ImGui::GetIO();

   // Setup display size (every frame to accommodate for window resizing)
   RECT rect;
   GetClientRect(g_hWnd, &rect);
   io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

   // Setup time step
   INT64 current_time;
   QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
   io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
   g_Time = current_time;

   // Read keyboard modifiers inputs
   io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
   io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
   io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
   // io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
   // io.MousePos : filled by WM_MOUSEMOVE events
   // io.MouseDown : filled by WM_*BUTTON* events
   // io.MouseWheel : filled by WM_MOUSEWHEEL events

   // Hide OS mouse cursor if ImGui is drawing it
   SetCursor(io.MouseDrawCursor ? nullptr : LoadCursor(nullptr, IDC_ARROW));

   // Start the frame
   ImGui::NewFrame();
}

void ImGui_ImplDX11_Render()
{
   ImGui::Render();
}