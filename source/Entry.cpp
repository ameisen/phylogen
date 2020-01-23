#include "phylogen.hpp"

#include "Simulation/Simulation.hpp"
#include "Renderer/Renderer.hpp"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

wchar_t WorkingDIr[MAX_PATH];

#include <Shlwapi.h>

void ResetWorkingDirectory()
{
   SetCurrentDirectoryW(WorkingDIr);
}

extern string getRandomName();
// HACK refactor
event HaveHashInput;
atomic<bool> InNewSim = { false };
atomic<bool> CancelNew = { false };
atomic<bool> RequestHashInput = { false };
string CurrentHashString;

namespace phylo
{
   enum class MenuItem : uint32
   {
      Exit,
      Save,
      Load,
      New,
      Settings,
      InstructionStats,
   };

   // Find a better place for this...
   Renderer *g_pRenderer = nullptr;
   Simulation *g_pSimulation = nullptr;

   class mouseHandler final : public ::mouse_handler
   {
      bool m_InDrag = false;
      vector2D m_DragStart;
      vector2D m_MouseDownPos;
      vector2D m_CurMousePosition;

   public:
      mouseHandler() = default;
      virtual ~mouseHandler() {}

      virtual void on_move(double x, double y, bool lButton, bool rButton)  override final
      {
         m_CurMousePosition = { x, y };

         if (!InNewSim && g_pSimulation && g_pRenderer)
         {
            auto unproject = g_pRenderer->unproject(m_CurMousePosition);
            g_pSimulation->on_click(unproject, rButton);
         }

         if (m_InDrag && !lButton)
         {
            m_InDrag = false;
         }
         else if (!m_InDrag && lButton)
         {
            m_InDrag = true;
            m_DragStart = m_CurMousePosition;
         }
         else if (m_InDrag)
         {
            vector2D mouseDiff = m_CurMousePosition - m_DragStart;
            if (mouseDiff.length_sq() != 0.0)
            {
               // every time we need to reset drag start because it's a diff.
               m_DragStart = m_CurMousePosition;
               if (g_pRenderer)
               {
                  g_pRenderer->move_screen(mouseDiff);
               }
            }
         }

         //if (g_pRenderer)
         //{
         //   auto unproject = g_pRenderer->unproject({ x, y });
         //   xdebug("MOUSE", "Unprojected Position: %s", unproject.to_string());
         //}
      }
      virtual void on_mousewheel(int wheelDelta)  override final
      {
         if (g_pRenderer)
         {
            g_pRenderer->adjust_zoom(-wheelDelta);
         }
      }
      virtual void on_leftbutton(bool state)  override final
      {
         if (m_InDrag && !state)
         {
            m_InDrag = false;
         }
         else if (!m_InDrag && state)
         {
            m_InDrag = true;
            m_DragStart = m_CurMousePosition;
            m_MouseDownPos = m_CurMousePosition;
         }
      }
      virtual void on_rightbutton(bool state)  override final
      {
         // Let's just presume that this is a click.
         if (!InNewSim && g_pSimulation)
         {
            auto unproject = g_pRenderer->unproject(m_CurMousePosition);
            g_pSimulation->on_click(unproject, state);
         }
      }
   };

   int exec (const array_view<string_view> &arguments) 
   {
      GetCurrentDirectoryW(MAX_PATH, WorkingDIr);
      ResetWorkingDirectory();

      xdebug("PHYLO", "Starting Phylogen");

      // The game consists of two discrete systems:
      // SIMULATION
      // The simulation processes the cells and the environment.
      // RENDERER
      // The renderer draws the cells, the environment, and the UI.

      mouseHandler mouseHandler;
      window *mainWindow = new window("phylogen", { 0, 0 }, { 1820, 1024 }, window::Attributes::Resizeable);

      xassert(mainWindow != nullptr, "Could not create the main window!");
      mainWindow->set_mouse_handler(mouseHandler);

      mainWindow->set_menu_callback([](const window &refWindow, uint64 menu_id){
         switch (MenuItem(menu_id))
         {
         case MenuItem::Exit:
         {
            // Send down a terminate message.
            PostMessage(HWND(refWindow._get_platform_handle()), WM_CLOSE, 0, 0);
         } break;
         case MenuItem::Save:
         {
            // We need to interrupt the simulation for this.
            try
            {
               if (InNewSim)
               {
                  break;
               }
               g_pSimulation->onSave();
            }
            catch (...)
            {
               MessageBoxW(HWND(refWindow._get_platform_handle()), L"Failed to save.", L"Failed", MB_OK | MB_ICONWARNING);
            }
         } break;
         case MenuItem::Load:
         {
            // We need to interrupt the simulation which will interrupt the renderer.
            try
            {
               if (InNewSim)
               {
                  break;
               }
               g_pSimulation->onLoad();
            }
            catch (...)
            {
               MessageBoxW(HWND(refWindow._get_platform_handle()), L"Failed to load.", L"Failed", MB_OK | MB_ICONWARNING);
            }
         } break;
         case MenuItem::New:
         {
            if (InNewSim)
            {
               break;
            }
            InNewSim = true;
            // Kick off a thread to handle this.
            auto *window = &refWindow;
            thread newThread = thread{[window]() {
               try
               {
                  CurrentHashString = getRandomName();

                  RequestHashInput = true;

                  HaveHashInput.join();

                  if (CancelNew)
                  {
                     CancelNew = false;
                     RequestHashInput = false;
                     InNewSim = false;
                     return;
                  }

                  // strip leading and terminating spaces.
                  while (CurrentHashString.length() && isspace(CurrentHashString.front()))
                  {
                     CurrentHashString = &CurrentHashString.data()[1];
                  }
                  while (CurrentHashString.length() && isspace(CurrentHashString.back()))
                  {
                     CurrentHashString.pop_back();
                  }

                  g_pSimulation->newSim(CurrentHashString);
                  RequestHashInput = false;
                  InNewSim = false;
               }
               catch (...)
               {
                  RequestHashInput = false;
                  InNewSim = false;
                  MessageBoxW(HWND(window->_get_platform_handle()), L"Failed to create new simulation.", L"Failed", MB_OK | MB_ICONWARNING);
               }
            }};
            newThread.set_name("New Sim Thread");
            newThread.start();
            newThread.detach();
         } break;
         case MenuItem::Settings:
         {
            try
            {
               if (InNewSim)
               {
                  break;
               }
               g_pSimulation->openSettings();
            }
            catch (...)
            {
               MessageBoxW(HWND(refWindow._get_platform_handle()), L"Failed to open settings menu.", L"Failed", MB_OK | MB_ICONWARNING);
            }
         } break;
         case MenuItem::InstructionStats:
         {
            try
            {
               if (InNewSim)
               {
                  break;
               }
               g_pSimulation->openInstructionStats();
            }
            catch (...)
            {
               MessageBoxW(HWND(refWindow._get_platform_handle()), L"Failed to open instruction stats menu.", L"Failed", MB_OK | MB_ICONWARNING);
            }
         } break;
         }
      });

      // Add some fun Windows menu stuff. libxtd doesn't support this, but we should be able to do it ourselves.
      HMENU hMenu;
      {
         hMenu = CreateMenu();
         {
            // File Submenu
            HMENU hSimulationMenu = CreateMenu();
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::Settings);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("&Settings");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hSimulationMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
                  );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::InstructionStats);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("&Instruction Stats");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hSimulationMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
                  );
            }

            MENUITEMINFO fileMenuInfo;
            memzero(fileMenuInfo);
            fileMenuInfo.cbSize = sizeof(fileMenuInfo);
            fileMenuInfo.fMask = MIIM_STRING | MIIM_SUBMENU;
            fileMenuInfo.fType = MFT_STRING;
            fileMenuInfo.hSubMenu = hSimulationMenu;
            fileMenuInfo.dwTypeData = const_cast<LPSTR>("Simulation");
            fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

            InsertMenuItem(
               hMenu,
               0,
               TRUE,
               &fileMenuInfo
               );
         }
         {
            // File Submenu
            HMENU hFileMenu = CreateMenu();
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::Exit);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("E&xit\tAlt+F4");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
               );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fType = MFT_MENUBARBREAK;
               fileMenuInfo.cch = 0;

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
                  );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::Load);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("&Open");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
               );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::Save);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("&Save");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
               );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fType = MFT_MENUBARBREAK;
               fileMenuInfo.cch = 0;

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
                  );
            }
            {
               MENUITEMINFO fileMenuInfo;
               memzero(fileMenuInfo);
               fileMenuInfo.cbSize = sizeof(fileMenuInfo);
               fileMenuInfo.fMask = MIIM_STRING | MIIM_ID;
               fileMenuInfo.fType = MFT_STRING;
               fileMenuInfo.wID = uint(MenuItem::New);
               fileMenuInfo.dwTypeData = const_cast<LPSTR>("&New");
               fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

               InsertMenuItem(
                  hFileMenu,
                  0,
                  TRUE,
                  &fileMenuInfo
                  );
            }

            MENUITEMINFO fileMenuInfo;
            memzero(fileMenuInfo);
            fileMenuInfo.cbSize = sizeof(fileMenuInfo);
            fileMenuInfo.fMask = MIIM_STRING | MIIM_SUBMENU;
            fileMenuInfo.fType = MFT_STRING;
            fileMenuInfo.hSubMenu = hFileMenu;
            fileMenuInfo.dwTypeData = const_cast<LPSTR>("File");
            fileMenuInfo.cch = uint(strlen(fileMenuInfo.dwTypeData) + 1);

            InsertMenuItem(
               hMenu,
               0,
               TRUE,
               &fileMenuInfo
            );
         }
         
         SetMenu(HWND(mainWindow->_get_platform_handle()), hMenu);
      }
      {
         // Fun icon stuff. Needs to go into libxtd at some point.
         HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(104)); // IDI_ICON1
         xassert(icon != nullptr, "Failed to load application icon");
         PostMessage(HWND(mainWindow->_get_platform_handle()), WM_SETICON, ICON_BIG, LPARAM(icon));
         PostMessage(HWND(mainWindow->_get_platform_handle()), WM_SETICON, ICON_SMALL, LPARAM(icon));
      }

	  const string randomName = getRandomName();

      auto *simulation = new Simulation(randomName);
      auto *renderer = new Renderer(simulation, mainWindow);

      array<System * > runningSystems = {
         g_pSimulation = simulation,
         g_pRenderer = renderer
      };

      g_pRenderer->adjust_zoom(50);

      g_pSimulation->set_renderer(g_pRenderer);

      g_pSimulation->kickoff();

      while (mainWindow->is_running()) // replace with a running bool
      {
         system::yield(64_msec);
      }
      
	  thread destroyThread{ []()
	  {
		  // Make sure this process gets cleaned up.
		  // 5 seconds
		  for (uint i = 0; i < 50; ++i)
		  {
			 system::yield(100_msec);
		  }

		  TerminateProcess(GetCurrentProcess(), 0);
	   } };

      // meh, just terminate.
      TerminateProcess(GetCurrentProcess(), 0);

      for (System *system : runningSystems)
      {
         system->halt();
      }

      for (System *system : runningSystems)
      {
         delete system;
      }

      DestroyMenu(hMenu);

      return 0;
   }
}

void xtd::init(const array_view<string_view> &arguments, initialization_parameters &initParameters)
{
   initParameters.graphics.enable = false;
   initParameters.math.halfClipRange = true;
}

int xtd::run(const array_view<string_view> &arguments)
{
   return phylo::exec(arguments);
}
