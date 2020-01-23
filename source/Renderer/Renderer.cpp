#include "Phylogen.hpp"
#include "Renderer.hpp"

using namespace phylo;

// In Phylo2, we will use xtd::graphics. xtd::graphics is not yet done.
static const usize WideArraySize = 5000000ull;

extern void    ImGui_ImplDX11_InvalidateDeviceObjects();
extern bool    ImGui_ImplDX11_CreateDeviceObjects();
extern bool    ImGui_ImplDX11_Init(void*hwnd, ID3D11Device*device, ID3D11DeviceContext*device_context);

Renderer::Renderer(Simulation *simulation, window *outputWindow) : System(),
m_Simulation(simulation),
m_NewSimulation(simulation),
m_pWindow(outputWindow)
{
	xassert(m_pWindow != nullptr, "Renderer window was null!");

	m_CurrentInstanceData.reserve(WideArraySize);
	m_PendingArray.reserve(WideArraySize);

	memset(m_InstructionTracker.data(), 0, m_InstructionTracker.size_raw());

	init_all();

	for (uint i = 0; i < traits<uint16>::max; ++i)
	{
		m_InstructionCheckboxes[i] = false;
	}
}

Renderer::~Renderer()
{
	extern void ImGui_ImplDX11_Shutdown();
	ImGui_ImplDX11_Shutdown();
	release_all();
}

#include "imgui\imgui.h"
#include "Simulation\Simulation.hpp"

// https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

#include <Pdh.h>

static array<PDH_HQUERY> cpuQueries;
static array<PDH_HCOUNTER> cpuTotals;

static void InitCPULoad(uint num_cpu)
{
	cpuQueries.resize(num_cpu);
	cpuTotals.resize(num_cpu);

	for (uint i = 0; i < num_cpu; ++i)
	{
		auto &query = cpuQueries[i];
		auto &total = cpuTotals[i];
		PdhOpenQuery(nullptr, 0, &query);
		PdhAddEnglishCounter(query, string::format("\\Processor(%u)\\%% Processor Time", i).data(), 0, &total);
		PdhCollectQueryData(query);
	}
}

static double GetCPULoad(uint cpu_num)
{
	PDH_FMT_COUNTERVALUE counterVal;
	PdhCollectQueryData(cpuQueries[cpu_num]);
	PdhGetFormattedCounterValue(cpuTotals[cpu_num], PDH_FMT_DOUBLE, nullptr, &counterVal);
	return counterVal.doubleValue;
}

// **********

#include <Powerbase.h>

typedef struct _PROCESSOR_POWER_INFORMATION {
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

// HACK refactor
extern event HaveHashInput;
extern atomic<bool> CancelNew;
extern atomic<bool> RequestHashInput;
extern string CurrentHashString;

void Renderer::render_ui() 
{
	static const uint numCores = system::get_system_information().logical_core_count;

	//ImVec4 clear_col = ImColor(114, 144, 154);
	//static float f = 0.0f;
	//ImGui::Text("Hello, world!");
	//ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
	//ImGui::ColorEdit3("clear color", (float*)&clear_col);

	//bool opened = false;
	//ImGui::Begin("Test", &opened, 0);
	//ImGui::Text("Hello, world!");
	//ImGui::End();

	const auto tooltip = [](const string_view &str)
	{
		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			ImGui::SetTooltip(str.data());
	};

	static constexpr float SpeedMenuWidth = 80.f;
	static constexpr float SpeedMenuHeight = 150.f;
	static constexpr float SpeedButtonWidth = 65.f;

	{
		// Speed menu
		static constexpr float MenuWidth = SpeedMenuWidth;
		static constexpr float MenuHeight = SpeedMenuHeight;
		static constexpr float ButtonWidth = SpeedButtonWidth;

		bool opened = false;
		ImGui::Begin("SpeedMenu", &opened, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
		ImGui::SetWindowPos({ 0.0f, 0.0f });
		ImGui::SetWindowSize({ MenuWidth, MenuHeight });

		const uint pauseState = (m_UIData.speedState == uint(Simulation::SpeedState::Pause)) ? (ImGuiButtonFlags_Disabled | ImGuiButtonFlags_Current_PC) : 0;
		const uint stepState = 0;
		const uint slowState = (m_UIData.speedState == uint(Simulation::SpeedState::Slow)) ? (ImGuiButtonFlags_Disabled | ImGuiButtonFlags_Current_PC) : 0;
		const uint mediumState = (m_UIData.speedState == uint(Simulation::SpeedState::Medium)) ? (ImGuiButtonFlags_Disabled | ImGuiButtonFlags_Current_PC) : 0;
		const uint fastState = (m_UIData.speedState == uint(Simulation::SpeedState::Fast)) ? (ImGuiButtonFlags_Disabled | ImGuiButtonFlags_Current_PC) : 0;
		const uint plaidState = (m_UIData.speedState == uint(Simulation::SpeedState::Ludicrous)) ? (ImGuiButtonFlags_Disabled | ImGuiButtonFlags_Current_PC) : 0;

		Simulation::SpeedState newSpeedState = Simulation::SpeedState(m_UIData.speedState);

		bool step = false;

		if (ImGui::ButtonEx("Pause", { ButtonWidth, 0.0f }, pauseState)) { newSpeedState = Simulation::SpeedState::Pause; }
		tooltip("pause the simulation");
		if (ImGui::ButtonEx("Step", { ButtonWidth, 0.0f }, stepState)) { step = true; }
		tooltip("pause the simulation and step execution one tick forward");
		if (ImGui::ButtonEx("Slow", { ButtonWidth, 0.0f }, slowState)) { newSpeedState = Simulation::SpeedState::Slow; }
		tooltip("set the speed to 'slow'");
		if (ImGui::ButtonEx("Medium", { ButtonWidth, 0.0f }, mediumState)) { newSpeedState = Simulation::SpeedState::Medium; }
		tooltip("set the speed to 'medium'");
		if (ImGui::ButtonEx("Fast", { ButtonWidth, 0.0f }, fastState)) { newSpeedState = Simulation::SpeedState::Fast; }
		tooltip("set the speed to 'fast'");
		if (ImGui::ButtonEx("Ludicrous", { ButtonWidth, 0.0f }, plaidState)) { newSpeedState = Simulation::SpeedState::Ludicrous; }
		tooltip("They've gone plaid!");
		ImGui::End();

		if (step)
		{
			newSpeedState = Simulation::SpeedState::Pause;
		}

		if (newSpeedState != Simulation::SpeedState(m_UIData.speedState))
		{
			m_Simulation->pushCallback([newSpeedState](Simulation *simulation) {
				simulation->SetSpeedState(newSpeedState);
			});
		}
		if (step)
		{
			m_Simulation->pushCallback([newSpeedState](Simulation *simulation) {
				simulation->SetStep();
			});
		}



		//ImGui::Image();

		// ImGui::ImageButton()
		// if (ImGui::Button("Test",)) {}
	}
	{
		// Simulation Current Status thing
		bool saving = false;
		bool loading = false;
		if (m_Simulation->is_saving())
		{
			saving = true;
		}
		if (m_Simulation->is_loading())
		{
			loading = true;
		}
		if (saving | loading)
		{
			bool opened = false;
			ImGui::Begin("SimulationStatus", &opened, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
			ImGui::SetWindowPos({ SpeedMenuWidth, 0.0f });
			ImGui::SetWindowFontScale(2.0f);
			ImGui::Text(saving ? "Saving ..." : "Loading ...");
			ImGui::End();
		}
	}

	float StatsHeight = 180.f;
	{
		static constexpr float StatsWidth = 350.f;

		bool opened = false;
		ImGui::Begin("Stats", &opened, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
		ImGui::SetWindowSize({ StatsWidth, 0 });

		auto reformat = [](string &&str) -> string
		{
			string out;
			usize nextOffset = (str.length() >= 4) ? 1 : 5;

			if (str.length() >= 4)
			{
				usize offsetBasis = str.length() - 4;
				offsetBasis %= 3;
				++offsetBasis;
				nextOffset = offsetBasis;
			}

			for (usize i = 0; i < str.length(); ++i)
			{
				if (nextOffset == i)
				{
					out.push_back(',');
					nextOffset = i + 3;
				}
				out.push_back(str.data()[i]);
			}

			return out;
		};

		// Stats Print
		string statsString;
#if XTD_DEBUG
		statsString += string::format("Ver: %s DBG\n", __TIMESTAMP__);
#else
		statsString += string::format("Ver: %s\n", __TIMESTAMP__);
#endif
		statsString += string::format("Seed: %s\n", m_HashName);
		uint64 totalTime = xtd::max(uint64(int64(m_UIData.totalTime)), 1ull);

		statsString += string::format("Total Tick     : %*s (%s TPS)\n", m_UIData.totalTime.unicode() ? 10 : 9, m_UIData.totalTime.to_string(), reformat(string::format("%llu", 1'000'000'000ull / totalTime)));
		statsString += string::format("  VM Step      : %*s\n", m_UIData.vmTime.unicode() ? 10 : 9, m_UIData.vmTime.to_string());
		statsString += string::format("  Physics      : %*s\n", m_UIData.physicsTime.unicode() ? 10 : 9, m_UIData.physicsTime.to_string());
		statsString += string::format("  Cell Update  : %*s\n", m_UIData.updateTime.unicode() ? 10 : 9, m_UIData.updateTime.to_string());
		statsString += string::format("  Render Copy  : %*s\n", m_UIData.renderUpdateTime.unicode() ? 10 : 9, m_UIData.renderUpdateTime.to_string());
		statsString += string::format("  Synch Step   : %*s\n", m_UIData.postTime.unicode() ? 10 : 9, m_UIData.postTime.to_string());
		statsString += string::format("  Light Update : %*s\n", m_UIData.lightTime.unicode() ? 10 : 9, m_UIData.lightTime.to_string());

		double renderTime = xtd::max(double(int64(m_RenderCPUTime)) / 1'000'000.0, 1.0);
		statsString += string::format("Render CPU     : %*s (%.2f FPS)\n", m_RenderCPUTime.unicode() ? 10 : 9, m_RenderCPUTime.to_string(), 1'000.0 / renderTime);
		auto totalSPTime = m_UIData.serialTime + m_UIData.parallelTime;

		statsString += string::format("Serial Time    : %*s (%2.2f %%%%)\n", m_UIData.serialTime.unicode() ? 10 : 9, m_UIData.serialTime.to_string(), 100.0 * (double(int64(m_UIData.serialTime)) / double(int64(totalSPTime))));
		statsString += string::format("Parallel Time  : %*s (%2.2f %%%%)\n", m_UIData.parallelTime.unicode() ? 10 : 9, m_UIData.parallelTime.to_string(), 100.0 * (double(int64(m_UIData.parallelTime)) / double(int64(totalSPTime))));
		statsString += string::format("Cell Count     : %s\n", reformat(string::format("%u", m_UIData.NumCells)));
		statsString += string::format("Total Cells    : %s\n", reformat(string::format("%u", m_UIData.TotalCells)));
		statsString += string::format("Current Tick   : %s\n", reformat(string::format("%llu", m_UIData.CurTick)));

		ImGui::Text(statsString.data());

		StatsHeight = ImGui::GetWindowHeight();
		ImGui::SetWindowPos({ 0.0f, m_CurrentResolution.y - StatsHeight });

		ImGui::End();
	}

	{
		static constexpr float UsageWidth = 350.f;

		bool opened = false;
		ImGui::Begin("Usage", &opened, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
		ImGui::SetWindowSize({ UsageWidth, -1 });

		static string CPU_Name;
		static bool _once = true;
		static usize longestCpu;
		static string loadFormatString;
		constexpr usize usageSmoothingMaxIndex = 64;
		static array<array<double, usageSmoothingMaxIndex>> usageSmoothing;
		static usize usageSmoothingIndex = 0;
		static array<double> defaultCpuFrequencies;
		if (_once)
		{
			array<int, 4> cpui;
			array<array<int, 4>> data;
			__cpuid(cpui.data(), 0);
			int numIds = cpui[0];
			for (int i = 0; i < numIds; ++i)
			{
				__cpuidex(cpui.data(), i, 0);
				data.push_back(cpui);
			}

			char vendor[0x20];
			memset(vendor, 0, sizeof(vendor));
			*reinterpret_cast<int* >(vendor) = data[0][1];
			*reinterpret_cast<int* >(vendor + 4) = data[0][3];
			*reinterpret_cast<int* >(vendor + 8) = data[0][2];

			__cpuid(cpui.data(), 0x80000000);
			int numExIds = cpui[0];

			char brand[0x40];
			memset(brand, 0, sizeof(brand));

			data.clear();
			for (int i = 0x80000000; i <= numExIds; ++i)
			{
				__cpuidex(cpui.data(), i, 0);
				data.push_back(cpui);
			}

			if (numExIds >= 0x80000004)
			{
				memcpy(brand, data[2].data(), sizeof(cpui));
				memcpy(brand + 16, data[3].data(), sizeof(cpui));
				memcpy(brand + 32, data[4].data(), sizeof(cpui));
				CPU_Name = brand;
			}
			else
			{
				CPU_Name = vendor;
			}

			InitCPULoad(numCores);

			_once = false;

			longestCpu = string::from(numCores - 1).length();
			loadFormatString = string::format("  %%%lluu: %%f\n", longestCpu);
			usageSmoothing.resize(numCores);
			for (auto & arr : usageSmoothing)
			{
				for (auto & v : arr)
				{
					v = 0.0;
				}
			}

			defaultCpuFrequencies.resize(numCores);

			array<PROCESSOR_POWER_INFORMATION> procInfo{ numCores };
			auto res = CallNtPowerInformation(ProcessorInformation, nullptr, 0, procInfo.data(), procInfo.size_raw());
			xassert(res == 0, "Could not get CPU information");
			for (uint i = 0; i < numCores; ++i)
			{
				defaultCpuFrequencies[i] = procInfo[i].MaxMhz;
			}
		}

		const uint numProcessors = numCores;

		array<PROCESSOR_POWER_INFORMATION> procInfo{ numCores };
		auto res = CallNtPowerInformation(ProcessorInformation, nullptr, 0, procInfo.data(), procInfo.size_raw());
		if (res == 0)
		{
			for (uint i = 0; i < numCores; ++i)
			{
				uint maxspeed = procInfo[i].CurrentMhz;
				maxspeed = xtd::max(maxspeed, (uint)procInfo[i].MaxMhz);
				maxspeed = xtd::max(maxspeed, (uint)procInfo[i].MhzLimit);
				defaultCpuFrequencies[i] = xtd::max(defaultCpuFrequencies[i], double(maxspeed));
			}
		}

		// Stats Print
		ImGui::Text(string::format("CPU: %s\n", CPU_Name).data());
		for (uint i = 0; i < numProcessors; ++i)
		{
			const double loadScale = double(procInfo[i].CurrentMhz) / defaultCpuFrequencies[i];

			double cpuLoad = 0.0;
			usageSmoothing[i][usageSmoothingIndex % usageSmoothingMaxIndex] = GetCPULoad(i) * loadScale;
			for (double load : usageSmoothing[i])
			{
				cpuLoad += load;
			}
			cpuLoad /= double(usageSmoothingMaxIndex);

			//statsString += string::format(loadFormatString, i, cpuLoad);
			//ImGui::PushItemWidth(UsageWidth - 100);

			ImGui::ProgressBar(float(cpuLoad) * 0.01f, { -1.0f, 4.0f }, "");
		}
		++usageSmoothingIndex;

		string statsString;

		auto reformat = [](const string &str) -> string
		{
			string out;
			usize nextOffset = (str.length() >= 4) ? 1 : 5;

			if (str.length() >= 4)
			{
				usize offsetBasis = str.length() - 4;
				offsetBasis %= 3;
				++offsetBasis;
				nextOffset = offsetBasis;
			}

			for (usize i = 0; i < str.length(); ++i)
			{
				if (nextOffset == i)
				{
					out.push_back(',');
					nextOffset = i + 3;
				}
				out.push_back(str.data()[i]);
			}

			return out;
		};

		ImGui::Text(statsString.data());
		ImGui::SetWindowPos({ 0.0f, m_CurrentResolution.y - (ImGui::GetWindowHeight() + StatsHeight) });
		ImGui::End();
	}

	{
		// Render Modes
		const float width = 310.f;
		const float height = 35.f;
		bool opened = false;
		ImGui::Begin("RenderModes", &opened, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
		ImGui::SetWindowPos({ float(m_CurrentResolution.x / 2) - (width / 2), 0.0f });
		ImGui::SetWindowSize({ width, height });

		int &RenderMode = (int &)m_RenderMode;

		ImGui::RadioButton("NormalRender", &RenderMode, (int)RenderMode::Normal);
		ImGui::SameLine(); ImGui::RadioButton("HashRender", &RenderMode, (int)RenderMode::HashCode);
		ImGui::SameLine(); ImGui::RadioButton("DyeRender", &RenderMode, (int)RenderMode::Dye);

		ImGui::End();
	}

	if (m_InstructionStatsOpenRequested)
	{
		struct InstructionPair final : trait_simple
		{
			uint16 Instruction;
			uint32 Count;

			bool operator < (const InstructionPair &pair) const 
			{
				if (Count == pair.Count)
				{
					return Instruction < pair.Instruction;
				}
				return Count > pair.Count;
			}
		};

		array<InstructionPair, VM::NumOperations + 1> InstructionPairs;
		for (uint i = 0; i < VM::NumOperations + 1; ++i)
		{
			InstructionPairs[i].Instruction = i;
			InstructionPairs[i].Count = m_InstructionTracker[i];
		}
		std::sort(&InstructionPairs[0], InstructionPairs.data() + (VM::NumOperations + 1));

		uint32 max = InstructionPairs[0].Count;
		uint32 total = 0;
		for (const auto &pair : InstructionPairs)
		{
			total += pair.Count;
		}

		bool opened = true;
		ImGui::Begin("Instruction Stats", &opened, 0);
		ImGui::SetWindowSize({ 800.0, 0.0 });

		static constexpr float width = 200.0f;

		static bool absoluteScale = false;

		double testValue = double(total);

		ImGui::Checkbox("Absolute Scale", &absoluteScale);
		if (absoluteScale)
		{
			testValue = double(max);
		}

		for (const auto &pair : InstructionPairs)
		{
			double widthScale = double(pair.Count) / double(testValue);

			const string description = VM::Instance::getInstructionDescription(pair.Instruction);

			int count = pair.Count;
			{
				//std::unique_lock<decltype(m_InstructionCheckboxLock)> _lock(m_InstructionCheckboxLock);
				bool &checkboxed = m_InstructionCheckboxes[pair.Instruction];
				ImGui::Checkbox((VM::Instance::getInstructionName(pair.Instruction) + "checkbox").data(), &checkboxed, true);
				tooltip("highlight cells calling this instruction");
			}

			ImGui::SameLine();
			ImGui::PushItemWidth(width);
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.0, 0.0, 0.0, 1.0 });
			float fraction;
			if (testValue == 0.0)
			{
				fraction = 0.0f;
			}
			else
			{
				fraction = float(widthScale);
			}
			ImGui::ProgressBar(fraction, { -1.0, 0.0 }, string::format("%s - ( %.2f %% )", VM::Instance::getInstructionName(pair.Instruction), fraction * 100.0f).data());
			ImGui::PopStyleColor();
			tooltip(description);
			ImGui::PopItemWidth();
			tooltip(description);
			count = pair.Count;
		}

		ImGui::End();

		if (!opened)
		{
			m_InstructionStatsOpenRequested = false;
		}
	}

	static options_delta optionsDelta;

	if (m_SettingsOpenRequested)
	{
		options_delta originalOptions;

		bool opened = true;
		ImGui::Begin("Settings", &opened, ImGuiWindowFlags_NoResize);
		ImGui::SetWindowSize({ 800.0f, 0.0f });
		ImGui::PushItemWidth(500.0f);
		ImGui::DragFloat("Light Change Rate", &optionsDelta.LightMapChangeRate, 0.0000001f, 0.0f, 1.0f, "%.7f");
		tooltip("how quickly the background light changes");
		ImGui::DragInt("Baseline Bytecode Size", &optionsDelta.BaselineBytecodeSize, 1, 1, 1000000);
		tooltip("the bytecode program size at which cells begin to be penalized");
		ImGui::DragFloat("Energy Burn Percentage", &optionsDelta.BurnEnergyPercentage, 0.01f, 0.0f, 10000.0f, "%.2f");
		tooltip("what percentage of energy is burned off by the OP_BURN opcode");
		ImGui::DragFloat("Base Energy Multiplier", &optionsDelta.BaseEnergyMultiplier, 1.f, 0.0f, 10000.0f, "%.1f");
		tooltip("how much energy that cells gain from light is multiplied by");
		ImGui::DragFloat("Substitution Mutation Chance", &optionsDelta.MutationSubstitutionChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a substitution mutation will occur");
		ImGui::DragFloat("Increment Mutation Chance", &optionsDelta.MutationIncrementChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a increment mutation will occur");
		ImGui::DragFloat("Decrement Mutation Chance", &optionsDelta.MutationDecrementChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a decrement mutation will occur");
		ImGui::DragFloat("Insertion Mutation Chance", &optionsDelta.MutationInsertionChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a insertion mutation will occur");
		ImGui::DragFloat("Deletion Mutation Chance", &optionsDelta.MutationDeletionChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a deletion mutation will occur");
		ImGui::DragFloat("Duplication Mutation Chance", &optionsDelta.MutationDuplicationChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a duplication mutation will occur");
		ImGui::DragFloat("Range Duplication Mutation Chance", &optionsDelta.MutationRangeDuplicationChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a range duplication mutation will occur");
		ImGui::DragFloat("Range Deletion Mutation Chance", &optionsDelta.MutationRangeDeletionChance, 0.01f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a range deletion mutation will occur");
		ImGui::DragFloat("Code Increment Mutation Chance", &optionsDelta.CodeMutationIncrementChance, 0.00001f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a byte translation increment mutation will occur");
		ImGui::DragFloat("Code Decrement Mutation Chance", &optionsDelta.CodeMutationDecrementChance, 0.00001f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a byte translation decrement mutation will occur");
		ImGui::DragFloat("Code Random Mutation Chance", &optionsDelta.CodeMutationRandomChance, 0.00001f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a byte translation random mutation will occur");
		ImGui::DragFloat("Code Swap Mutation Chance", &optionsDelta.CodeMutationSwapChance, 0.00001f, 0.0f, 1.0f, "%.9f");
		tooltip("chance that a byte translation swap mutation will occur");

		ImGui::DragFloat("Armor Regrowth Rate", &optionsDelta.ArmorRegrowthRate, 0.00001f, 0.0f, 1.0f, "%.5f");
		tooltip("how much armor is regrow per tick");
		ImGui::DragFloat("Baseline Random Mutation Chance", &optionsDelta.LiveMutationChance, 0.000001f, 0.0f, 1.0f, "%.6f");
		tooltip("the chance that a cell will randomly mutate at times other than splitting");
		ImGui::DragFloat("Crowding Penalty", &optionsDelta.CrowdingPenalty, 0.0001f, 0.0f, 1.0f, "%.4f");
		tooltip("energy penalty applied to cells which have other cells touching them");

		ImGui::DragInt("Base Tick Cost", &optionsDelta.TickEnergyLost, 1, 1, 1000000);
		tooltip("base energy cost per tick");
		ImGui::DragInt("Base Sleep Cost", &optionsDelta.SleepTickEnergyLost, 1, 1, 1000000);
		tooltip("energy cost per tick while sleeping");
		ImGui::DragInt("Base Move Cost", &optionsDelta.BaseMoveCost, 1, 1, 1000000);
		tooltip("maximum energy cost for a move instruction");
		ImGui::DragInt("Base Rotate Cost", &optionsDelta.BaseRotateCost, 1, 1, 1000000);
		tooltip("maximum energy cost for a rotate instruction");
		ImGui::DragInt("Base Split Cost", &optionsDelta.BaseSplitCost, 1, 1, 1000000);
		tooltip("maximum energy cost for a split instruction");
		ImGui::DragInt("Base Growth Cost", &optionsDelta.BaseGrowCost, 1, 1, 1000000);
		tooltip("maximum energy cost for a growth instruction");
		ImGui::PopItemWidth();

		bool apply = false;

		if (ImGui::Button("Apply")) { apply = true; }
		tooltip("apply settings");
		ImGui::SameLine(); if (ImGui::Button("Reset")) { optionsDelta = originalOptions; }
		tooltip("reset settings");
		ImGui::SameLine(); if (ImGui::Button("Default")) { optionsDelta = defaultOptions; }
		tooltip("reset all settings back to default");

		ImGui::End();

		if (!opened)
		{
			m_SettingsOpenRequested = false;
		}

		if (apply && originalOptions != optionsDelta)
		{
			// Options were changed!
			originalOptions = optionsDelta;
			apply = false;
			scoped_lock<mutex> _lock(m_Simulation->getSimulationLock());
			optionsDelta.apply();
		}
	}
	else
	{
		optionsDelta = options_delta(); // don't need to do this every tick.
	}

	// extern event HaveHashInput;
	// extern atomic<bool> CancelNew = { false };
	// extern atomic<bool> RequestHashInput = { false };
	// extern string CurrentHashString;
	if (RequestHashInput)
	{
		char seedBuffer[4096];
		memcpy(seedBuffer, CurrentHashString.data(), CurrentHashString.size());
		bool opened = true;
		ImGui::Begin("New Simulation", &opened, ImGuiWindowFlags_NoResize);
		ImGui::SetWindowSize({ 300.0f, 0.0f });

		bool result = ImGui::InputText("Seed", seedBuffer, sizeof(seedBuffer));

		if (ImGui::Button("Cancel"))
		{
			CancelNew = true;
			RequestHashInput = false;
			HaveHashInput.set();
		}
		ImGui::SameLine(); if (ImGui::Button("Done"))
		{
			CurrentHashString = seedBuffer;
			RequestHashInput = false;
			HaveHashInput.set();
		}

		ImGui::End();
		CurrentHashString = seedBuffer;
	}
}

void Renderer::render_frame(clock::time_point timeAtRenderStart) 
{
	// Set the depth-stencil state to NONE.
	m_pContext->OMSetDepthStencilState(State.m_NoDepthStencilState, 0);
	m_pContext->OMSetBlendState(nullptr, nullptr, -1);
	const RECT scissorRect = { 0, 0, m_CurrentResolution.x, m_CurrentResolution.y };
	m_pContext->RSSetScissorRects(1, &scissorRect);

	// Reset the render state
	m_RenderState = _RenderState();

	bool newData = false;

	// See if there is new pending data.
	{
		scoped_lock<mutex> _lock(m_FramePendingLock);

		if (m_CurrentFrame < m_PendingFrame || m_ForceUpdate)
		{
			m_CurrentInstanceData.swap(m_PendingArray);
			m_PendingArray.clear();

			m_CurrentLightData.swap(m_PendingLightData);
			m_PendingLightData.clear();

			m_CurrentWasteData.swap(m_PendingWasteData);
			m_PendingWasteData.clear();

			newData = true;

			m_CurrentFrame = m_PendingFrame;
			m_ForceUpdate = false;
		}
	}

	// light
	if (m_CurrentLightData.size() && !Buffer.m_pLightTexture)
	{
		// We need to create the light texture.
		// m_LightEdge

		D3D11_TEXTURE2D_DESC lightTextureDesc = { 0 };
		lightTextureDesc.Width = m_LightEdge;
		lightTextureDesc.Height = m_LightEdge;
		lightTextureDesc.MipLevels = 1;
		lightTextureDesc.ArraySize = 1;
		lightTextureDesc.Format = DXGI_FORMAT_R8_UNORM;
		lightTextureDesc.SampleDesc.Count = 1;
		lightTextureDesc.SampleDesc.Quality = 0;
		lightTextureDesc.Usage = D3D11_USAGE_DYNAMIC;
		lightTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		lightTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		lightTextureDesc.MiscFlags = 0;

		HRESULT hRes = m_pDevice->CreateTexture2D(
			&lightTextureDesc,
			nullptr,
			&Buffer.m_pLightTexture
		);
		xassert(hRes == S_OK, "Failed to create light texture");
		m_RenderResources += Buffer.m_pLightTexture;

		D3D11_SHADER_RESOURCE_VIEW_DESC lightTextureViewDesc;
		lightTextureViewDesc.Format = DXGI_FORMAT_R8_UNORM;
		lightTextureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		lightTextureViewDesc.Texture2D.MipLevels = 1;
		lightTextureViewDesc.Texture2D.MostDetailedMip = 0;

		hRes = m_pDevice->CreateShaderResourceView(
			Buffer.m_pLightTexture,
			&lightTextureViewDesc,
			&Buffer.m_pLightTextureView
		);
		xassert(hRes == S_OK, "Failed to create light texture view");
		m_RenderResources += Buffer.m_pLightTextureView;

		D3D11_MAPPED_SUBRESOURCE subResource;
		hRes = m_pContext->Map(Buffer.m_pLightTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
		xassert(hRes == S_OK, "Failed to map light texture");

		uint8 *texData = (uint8 *)subResource.pData;
		for (uint row = 0; row < m_LightEdge; ++row)
		{
			memset(texData, 0xFF, m_LightEdge);

			texData += subResource.RowPitch;
		}

		m_pContext->Unmap(Buffer.m_pLightTexture, 0);

		// m_pLightSamplerState

		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;

		hRes = m_pDevice->CreateSamplerState(&samplerDesc, &Buffer.m_pLightSamplerState);
		xassert(hRes == S_OK, "Failed to create light texture sampler state");
		m_RenderResources += Buffer.m_pLightSamplerState;

	}
	if (newData && Buffer.m_pLightTexture)
	{
		D3D11_MAPPED_SUBRESOURCE subResource;
		HRESULT hRes = m_pContext->Map(Buffer.m_pLightTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
		xassert(hRes == S_OK, "Failed to map light texture");

		const uint8 *srcData = m_CurrentLightData.data();
		uint8 *texData = (uint8 *)subResource.pData;
		for (uint row = 0; row < m_LightEdge; ++row)
		{
			memcpy(texData, srcData, m_LightEdge);

			texData += subResource.RowPitch;
			srcData += m_LightEdge;
		}

		m_pContext->Unmap(Buffer.m_pLightTexture, 0);
	}

	// waste
	if (m_CurrentWasteData.size() && !Buffer.m_pWasteTexture)
	{
		// We need to create the waste texture.
		// m_WasteEdge

		D3D11_TEXTURE2D_DESC wasteTextureDesc = { 0 };
		wasteTextureDesc.Width = m_WasteEdge;
		wasteTextureDesc.Height = m_WasteEdge;
		wasteTextureDesc.MipLevels = 1;
		wasteTextureDesc.ArraySize = 1;
		wasteTextureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		wasteTextureDesc.SampleDesc.Count = 1;
		wasteTextureDesc.SampleDesc.Quality = 0;
		wasteTextureDesc.Usage = D3D11_USAGE_DYNAMIC;
		wasteTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		wasteTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		wasteTextureDesc.MiscFlags = 0;

		HRESULT hRes = m_pDevice->CreateTexture2D(
			&wasteTextureDesc,
			nullptr,
			&Buffer.m_pWasteTexture
		);
		xassert(hRes == S_OK, "Failed to create waste texture");
		m_RenderResources += Buffer.m_pWasteTexture;

		D3D11_SHADER_RESOURCE_VIEW_DESC wasteTextureViewDesc;
		wasteTextureViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		wasteTextureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		wasteTextureViewDesc.Texture2D.MipLevels = 1;
		wasteTextureViewDesc.Texture2D.MostDetailedMip = 0;

		hRes = m_pDevice->CreateShaderResourceView(
			Buffer.m_pWasteTexture,
			&wasteTextureViewDesc,
			&Buffer.m_pWasteTextureView
		);
		xassert(hRes == S_OK, "Failed to create waste texture view");
		m_RenderResources += Buffer.m_pWasteTextureView;

		D3D11_MAPPED_SUBRESOURCE subResource;
		hRes = m_pContext->Map(Buffer.m_pWasteTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
		xassert(hRes == S_OK, "Failed to map waste texture");

		uint8 *texData = (uint8 *)subResource.pData;
		for (uint row = 0; row < m_WasteEdge; ++row)
		{
			memset(texData, 0xFF, m_WasteEdge * 4);

			texData += subResource.RowPitch;
		}

		m_pContext->Unmap(Buffer.m_pWasteTexture, 0);

		// m_pWasteSamplerState

		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;

		hRes = m_pDevice->CreateSamplerState(&samplerDesc, &Buffer.m_pWasteSamplerState);
		xassert(hRes == S_OK, "Failed to create waste texture sampler state");
		m_RenderResources += Buffer.m_pWasteSamplerState;

	}
	if (newData && Buffer.m_pWasteTexture)
	{
		D3D11_MAPPED_SUBRESOURCE subResource;
		HRESULT hRes = m_pContext->Map(Buffer.m_pWasteTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &subResource);
		xassert(hRes == S_OK, "Failed to map waste texture");

		const float *srcData = (float *)m_CurrentWasteData.data();
		uint8 *texData = (uint8 *)subResource.pData;
		for (uint row = 0; row < m_WasteEdge; ++row)
		{
			memcpy(texData, srcData, m_WasteEdge * 4);

			texData += subResource.RowPitch;
			srcData += m_WasteEdge;
		}

		m_pContext->Unmap(Buffer.m_pWasteTexture, 0);
	}

	m_SignalFrameReady = true;

	// Generate Per-Frame Constant Data
	{
		if (m_Viewport.Width > m_Viewport.Height)
		{
			float Width = m_Viewport.Width / m_Viewport.Height;
			m_RenderState.Projection = math::matrix4F::ortho(float(-Width * m_ZoomScalar), float(Width * m_ZoomScalar), float(-1.0f * m_ZoomScalar), float(1.0f * m_ZoomScalar), 0.1f, 1.0f);
		}
		else
		{
			float Height = m_Viewport.Height / m_Viewport.Width;
			m_RenderState.Projection = math::matrix4F::ortho(float(-1.0f * m_ZoomScalar), float(1.0f * m_ZoomScalar), float(-Height * m_ZoomScalar), float(Height * m_ZoomScalar), 0.1f, 1.0f);
		}

		//m_RenderState.Model = matrix4F::Identity;
		m_RenderState.View = matrix4F::Identity;
		// View should be adjusted to our camera view.
		vector4F cameraPos = { m_CameraOffset, 0.0f };
		cameraPos = -cameraPos;
		cameraPos.w = 1.0f;

		m_RenderState.View[3] = cameraPos;
		m_ViewProjection = (m_RenderState.Projection * m_RenderState.View).transpose();
		m_RenderState.VP = m_ViewProjection;

		auto currentTime = clock::get_current_time();
		auto timeSinceRenderStart = currentTime - timeAtRenderStart;
		double timeRenderStartVar = double(xtd::time(timeSinceRenderStart));
		m_RenderState.Time = {
		   float(timeRenderStartVar),
		   float(fmod(timeRenderStartVar, 1.0)),
		   float(fmod(timeRenderStartVar, 10.0)),
		   float(fmod(timeRenderStartVar, 100.0))
		};
	}

	auto update_const_data = [this]()
	{
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pRenderStateBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, &m_RenderState, sizeof(m_RenderState));
		m_pContext->Unmap(Buffer.m_pRenderStateBuffer, 0);
	};

	// DRAW
	{
		m_pContext->RSSetViewports(1, &m_Viewport);

		m_pContext->OMSetRenderTargets(1, &Target.m_pRenderTargetView, nullptr); // Target.m_pDepthTargetView
		m_pContext->ClearRenderTargetView(Target.m_pRenderTargetView, color::black);

		m_pContext->VSSetConstantBuffers(0, 1, &Buffer.m_pRenderStateBuffer);
		m_pContext->PSSetConstantBuffers(0, 1, &Buffer.m_pRenderStateBuffer);

		m_pContext->IASetInputLayout(Buffer.m_pSquareInputLayout);
		m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		static const uint Zero = 0;
		m_pContext->IASetVertexBuffers(0, 1, &Buffer.m_pSquareVertexBuffer, &Buffer.m_uSquareStride, &Zero);

		// For every batch of instances, we need to update the instance buffer.

		auto getTransformTemp = [](const vector4F &vec) -> matrix4F
		{
			matrix4F objTrans = matrix4F::Identity;
			objTrans[3] = vec;
			return objTrans;
		};

		//static random::source<random::engine::xorshift_plus> RandSource;
		//static InstanceData tempInstanceData[2] = {
		//   { getTransformTemp({  0.25f, 0.0f, 0.0f, 1.0f }), { 0.16f, 0.62f, 0.2f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 10.0f, 0.0f, 0.0f, 0.0f }, 0.15f, RandSource.uniform<float>(0.0f, 10.0f) },
		//   { getTransformTemp({ -0.25f, 0.0f, 0.0f, 1.0f }), { 0.16f, 0.62f, 0.2f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, 0.25f, RandSource.uniform<float>(0.0f, 10.0f) }
		//};

		//tempInstanceData[0].Transform[3].x += 0.001f;

		m_RenderState.Scale_ColorLerp_MoveForward.w = 0.0;
		m_RenderState.WorldRadius_Lit_NuclearScaleLerp.x = options::WorldRadius;
		m_RenderState.WorldRadius_Lit_NuclearScaleLerp.y = m_Illumination;
		m_RenderState.WorldRadius_Lit_NuclearScaleLerp.z = 0.0;

		if (Buffer.m_pLightTexture)
		{
			m_pContext->PSSetShaderResources(1, 1, &Buffer.m_pLightTextureView);
			m_pContext->PSSetSamplers(1, 1, &Buffer.m_pLightSamplerState);
		}

		if (Buffer.m_pWasteTexture)
		{
			m_pContext->PSSetShaderResources(2, 1, &Buffer.m_pWasteTextureView);
			m_pContext->PSSetSamplers(2, 1, &Buffer.m_pWasteSamplerState);
		}

		// Render backdrop.
		{
			static const InstanceData backdropInstance = {
			   matrix4F::Identity,
			   color::white * 0.45f,
			   color::white * 0.45f,
			   {0.0f, 0.0f, 0.0f, 0.0f},
			   float(options::WorldRadius), 0.0f,
			   nullptr,
			};


			m_RenderState.Color = color::white * 0.75;
			m_RenderState.Scale_ColorLerp_MoveForward.x = 1.01f;
			update_const_data();
			render_circles_constcolor(1, &backdropInstance);

			m_RenderState.Color = color::white * 0.5;
			m_RenderState.Scale_ColorLerp_MoveForward.x = 1.0f;
			update_const_data();
			render_circles_backdrop(1, &backdropInstance);
		}

		m_pContext->OMSetBlendState(State.m_TransparentBlendState, nullptr, -1);
		m_RenderState.WorldRadius_Lit_NuclearScaleLerp.y = 1.0;

		// Render Cell Outlines
		m_RenderState.Scale_ColorLerp_MoveForward.x = 1.3f;
		m_RenderState.Color = color::white;
		update_const_data();
		render_circles_jitter(m_CurrentInstanceData.size(), m_CurrentInstanceData.data());

		m_RenderState.Scale_ColorLerp_MoveForward.x = 1.0f;
		m_RenderState.Color = { 0.5f, 0.5f, 0.5f, 1.0f };
		update_const_data();
		render_circles_multiply(m_CurrentInstanceData.size(), m_CurrentInstanceData.data());

		m_RenderState.Scale_ColorLerp_MoveForward.x = 0.9f;
		update_const_data();
		render_circles(m_CurrentInstanceData.size(), m_CurrentInstanceData.data());

		// Render Cell Nucleii
		m_RenderState.Scale_ColorLerp_MoveForward.x = 0.3f;
		m_RenderState.Scale_ColorLerp_MoveForward.y = 1.0f;
		m_RenderState.Scale_ColorLerp_MoveForward.w = -0.5f;
		m_RenderState.WorldRadius_Lit_NuclearScaleLerp.z = 1.0f;
		update_const_data();
		render_circles(m_CurrentInstanceData.size(), m_CurrentInstanceData.data());

		extern void ImGui_ImplDX11_NewFrame();
		ImGui_ImplDX11_NewFrame();


		render_ui();

		extern void ImGui_ImplDX11_Render();
		ImGui_ImplDX11_Render();

		m_pContext->ResolveSubresource(
			Target.m_pBackbufferTexture, 0,
			Target.m_pRenderTargetTexture, 0,
			DXGI_FORMAT_R8G8B8A8_UNORM
		);
	}
}

void Renderer::render_circles_multiply(usize instanceCount, const InstanceData *instanceData)
{
	m_pContext->VSSetShader(Shader.m_pCircleMultiplyVertexShader, nullptr, 0);
	m_pContext->PSSetShader(Shader.m_pCircleMultiplyPixelShader, nullptr, 0);

	usize numInstances = instanceCount;
	usize instanceOffset = 0;
	while (numInstances)
	{
		usize numInstancesProcess = Buffer.m_uInstanceBufferElements;
		if (numInstancesProcess > numInstances)
		{
			numInstancesProcess = numInstances;
		}

		// Send down the data.
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, instanceData + instanceOffset, sizeof(instanceData[0]) * numInstancesProcess);
		m_pContext->Unmap(Buffer.m_pInstanceBuffer, 0);

		m_pContext->PSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);
		m_pContext->VSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);

		xassert(numInstancesProcess <= 0xFFFFFFFF, "numInstanceProcess cannot be cast to uint -- value too large");
		m_pContext->DrawInstanced(Buffer.m_uSquareVertices, uint(numInstancesProcess), 0, 0);

		numInstances -= numInstancesProcess;
		instanceOffset += numInstancesProcess;
	}
}

void Renderer::render_circles_backdrop(usize instanceCount, const InstanceData *instanceData)
{
	m_pContext->VSSetShader(Shader.m_pCircleBackdropVertexShader, nullptr, 0);
	m_pContext->PSSetShader(Shader.m_pCircleBackdropPixelShader, nullptr, 0);

	usize numInstances = instanceCount;
	usize instanceOffset = 0;
	while (numInstances)
	{
		usize numInstancesProcess = Buffer.m_uInstanceBufferElements;
		if (numInstancesProcess > numInstances)
		{
			numInstancesProcess = numInstances;
		}

		// Send down the data.
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, instanceData + instanceOffset, sizeof(instanceData[0]) * numInstancesProcess);
		m_pContext->Unmap(Buffer.m_pInstanceBuffer, 0);

		m_pContext->PSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);
		m_pContext->VSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);

		xassert(numInstancesProcess <= 0xFFFFFFFF, "numInstanceProcess cannot be cast to uint -- value too large");
		m_pContext->DrawInstanced(Buffer.m_uSquareVertices, uint(numInstancesProcess), 0, 0);

		numInstances -= numInstancesProcess;
		instanceOffset += numInstancesProcess;
	}
}

void Renderer::render_circles_constcolor(usize instanceCount, const InstanceData *instanceData)
{
	m_pContext->VSSetShader(Shader.m_pCircleConstColorVertexShader, nullptr, 0);
	m_pContext->PSSetShader(Shader.m_pCircleConstColorPixelShader, nullptr, 0);

	usize numInstances = instanceCount;
	usize instanceOffset = 0;
	while (numInstances)
	{
		usize numInstancesProcess = Buffer.m_uInstanceBufferElements;
		if (numInstancesProcess > numInstances)
		{
			numInstancesProcess = numInstances;
		}

		// Send down the data.
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, instanceData + instanceOffset, sizeof(instanceData[0]) * numInstancesProcess);
		m_pContext->Unmap(Buffer.m_pInstanceBuffer, 0);

		m_pContext->PSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);
		m_pContext->VSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);

		xassert(numInstancesProcess <= 0xFFFFFFFF, "numInstanceProcess cannot be cast to uint -- value too large");
		m_pContext->DrawInstanced(Buffer.m_uSquareVertices, uint(numInstancesProcess), 0, 0);

		numInstances -= numInstancesProcess;
		instanceOffset += numInstancesProcess;
	}
}

void Renderer::render_circles_jitter(usize instanceCount, const InstanceData *instanceData)
{
	m_pContext->VSSetShader(Shader.m_pCircleJitterVertexShader, nullptr, 0);
	m_pContext->PSSetShader(Shader.m_pCircleJitterPixelShader, nullptr, 0);

	usize numInstances = instanceCount;
	usize instanceOffset = 0;
	while (numInstances)
	{
		usize numInstancesProcess = Buffer.m_uInstanceBufferElements;
		if (numInstancesProcess > numInstances)
		{
			numInstancesProcess = numInstances;
		}

		// Send down the data.
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, instanceData + instanceOffset, sizeof(instanceData[0]) * numInstancesProcess);
		m_pContext->Unmap(Buffer.m_pInstanceBuffer, 0);

		m_pContext->PSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);
		m_pContext->VSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);

		xassert(numInstancesProcess <= 0xFFFFFFFF, "numInstanceProcess cannot be cast to uint -- value too large");
		m_pContext->DrawInstanced(Buffer.m_uSquareVertices, uint(numInstancesProcess), 0, 0);

		numInstances -= numInstancesProcess;
		instanceOffset += numInstancesProcess;
	}
}

void Renderer::render_circles(usize instanceCount, const InstanceData *instanceData)
{
	m_pContext->VSSetShader(Shader.m_pCircleVertexShader, nullptr, 0);
	m_pContext->PSSetShader(Shader.m_pCirclePixelShader, nullptr, 0);

	usize numInstances = instanceCount;
	usize instanceOffset = 0;
	while (numInstances)
	{
		usize numInstancesProcess = Buffer.m_uInstanceBufferElements;
		if (numInstancesProcess > numInstances)
		{
			numInstancesProcess = numInstances;
		}

		// Send down the data.
		D3D11_MAPPED_SUBRESOURCE mappedData;
		m_pContext->Map(Buffer.m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
		memcpy(mappedData.pData, instanceData + instanceOffset, sizeof(instanceData[0]) * numInstancesProcess);
		m_pContext->Unmap(Buffer.m_pInstanceBuffer, 0);

		m_pContext->PSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);
		m_pContext->VSSetShaderResources(0, 1, &Buffer.m_pShaderResourceView);

		xassert(numInstancesProcess <= 0xFFFFFFFF, "numInstanceProcess cannot be cast to uint -- value too large");
		m_pContext->DrawInstanced(Buffer.m_uSquareVertices, uint(numInstancesProcess), 0, 0);

		numInstances -= numInstancesProcess;
		instanceOffset += numInstancesProcess;
	}
}

void Renderer::render_loop()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	auto timeAtRenderStart = clock::get_current_time();

	while (m_DrawThreadRun)
	{
		auto startTime = clock::get_current_time();

		// Pre-frame duties.
		{
			scoped_lock<mutex> _lock(m_CameraLock);
			m_CameraOffset = m_DirtyCameraOffset;
			m_ZoomScalar = m_DirtyZoomScalar;
		}


		HRESULT hRes = m_pSwapChain->Present(0, DXGI_PRESENT_TEST);
		switch (hRes)
		{
		case S_OK:
			break;
		case DXGI_STATUS_OCCLUDED:
			system::yield(1_msec);
			continue;
		case DXGI_ERROR_DEVICE_RESET:
		case DXGI_ERROR_DEVICE_REMOVED:
			// Device was removed, we must rebuild.
			m_CurrentResolution = { 0, 0 };
			rebuild();
			continue;
		default:
			xassert(hRes == S_OK, "present failed"); break;
		}

		m_Simulation = m_NewSimulation;

		if (m_Simulation)
		{
			scoped_lock<mutex> _lock(m_RenderLock);
			{
				// Every tick, let's revalidate things.
				vector2I currentResolution = m_pWindow->get_size();
				if (currentResolution.x == 0 || currentResolution.y == 0)
				{
					continue;
				}
				if (currentResolution != m_CurrentResolution)
				{
					m_pContext->Flush();
					m_CurrentResolution = currentResolution;
					m_Viewport.TopLeftX = 0.0f;
					m_Viewport.TopLeftY = 0.0f;
					m_Viewport.Width = float(m_CurrentResolution.x);
					m_Viewport.Height = float(m_CurrentResolution.y);
					m_Viewport.MinDepth = 0.0f;
					m_Viewport.MaxDepth = 1.0f;
					clear_targets();
					Target.m_pBackbufferTexture->Release();
					hRes = m_pSwapChain->ResizeBuffers(
						3,
						currentResolution.x,
						currentResolution.y,
						DXGI_FORMAT_R8G8B8A8_UNORM,
						DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
					);
					switch (hRes)
					{
					case DXGI_ERROR_DEVICE_RESET:
					case DXGI_ERROR_DEVICE_REMOVED:
						// Device was removed, we must rebuild.
						m_CurrentResolution = { 0, 0 };
						rebuild();
						continue;
					default: break;
					}
					xassert(hRes == S_OK, "Failed to resize swapchain buffers");
					hRes = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&Target.m_pBackbufferTexture);
					xassert(hRes == S_OK && Target.m_pBackbufferTexture, "Could not get DXGI backbuffer texture");
					init_targets();
				}
			}

			render_frame(timeAtRenderStart);
		}

		hRes = m_pSwapChain->Present(1, 0); // we don't want tearing. Ever.
		switch (hRes)
		{
		case DXGI_STATUS_OCCLUDED:
			break;
		case DXGI_ERROR_DEVICE_RESET:
		case DXGI_ERROR_DEVICE_REMOVED:
			// Device was removed, we must rebuild.
			m_CurrentResolution = { 0, 0 };
			rebuild();
			continue;
		default:
			xassert(hRes == S_OK, "present failed"); break;
		}

		m_RenderCPUTime = clock::get_current_time() - startTime;
	}
}

void Renderer::init_resources()
{
	Buffer.m_pLightTexture = nullptr;
	Buffer.m_pWasteTexture = nullptr;

	// init shaders

	{
		io::file vertexShaderFile("Assets/circle_jitter.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		HRESULT hRes = m_pDevice3->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &Shader.m_pCircleJitterVertexShader);

		xassert(hRes == S_OK, "Failed to create Circle Jitter Vertex Shader");
		m_RenderResources += Shader.m_pCircleJitterVertexShader;

		io::file pixelShaderFile("Assets/circle_jitter.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		hRes = m_pDevice->CreatePixelShader(pixelShaderFile.at(0), pixelShaderFile.get_size(), nullptr, &Shader.m_pCircleJitterPixelShader);
		xassert(hRes == S_OK, "Failed to create Circle Jitter Pixel Shader");
		m_RenderResources += Shader.m_pCircleJitterPixelShader;
	}

	{
		io::file vertexShaderFile("Assets/circle_constcolor.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		HRESULT hRes = m_pDevice->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &Shader.m_pCircleConstColorVertexShader);
		xassert(hRes == S_OK, "Failed to create Circle ConstColor Vertex Shader");
		m_RenderResources += Shader.m_pCircleConstColorVertexShader;

		io::file pixelShaderFile("Assets/circle_constcolor.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		hRes = m_pDevice->CreatePixelShader(pixelShaderFile.at(0), pixelShaderFile.get_size(), nullptr, &Shader.m_pCircleConstColorPixelShader);
		xassert(hRes == S_OK, "Failed to create Circle ConstColor Pixel Shader");
		m_RenderResources += Shader.m_pCircleConstColorPixelShader;
	}

	{
		io::file vertexShaderFile("Assets/circle_multiply.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		HRESULT hRes = m_pDevice->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &Shader.m_pCircleMultiplyVertexShader);
		xassert(hRes == S_OK, "Failed to create Circle Multiply Vertex Shader");
		m_RenderResources += Shader.m_pCircleMultiplyVertexShader;

		io::file pixelShaderFile("Assets/circle_multiply.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		hRes = m_pDevice->CreatePixelShader(pixelShaderFile.at(0), pixelShaderFile.get_size(), nullptr, &Shader.m_pCircleMultiplyPixelShader);
		xassert(hRes == S_OK, "Failed to create Circle Multiply Pixel Shader");
		m_RenderResources += Shader.m_pCircleMultiplyPixelShader;
	}

	{
		io::file vertexShaderFile("Assets/circle_backdrop.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		HRESULT hRes = m_pDevice->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &Shader.m_pCircleBackdropVertexShader);
		xassert(hRes == S_OK, "Failed to create Circle Backdrop Vertex Shader");
		m_RenderResources += Shader.m_pCircleBackdropVertexShader;

		io::file pixelShaderFile("Assets/circle_backdrop.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
		hRes = m_pDevice->CreatePixelShader(pixelShaderFile.at(0), pixelShaderFile.get_size(), nullptr, &Shader.m_pCircleBackdropPixelShader);
		xassert(hRes == S_OK, "Failed to create Circle Backdrop Pixel Shader");
		m_RenderResources += Shader.m_pCircleBackdropPixelShader;
	}

	io::file vertexShaderFile("Assets/circle.v.cso", io::file::Flags::Sequential | io::file::Flags::Read);
	HRESULT hRes = m_pDevice->CreateVertexShader(vertexShaderFile.at(0), vertexShaderFile.get_size(), nullptr, &Shader.m_pCircleVertexShader);
	xassert(hRes == S_OK, "Failed to create Circle Vertex Shader");
	m_RenderResources += Shader.m_pCircleVertexShader;


	io::file pixelShaderFile("Assets/circle.p.cso", io::file::Flags::Sequential | io::file::Flags::Read);
	hRes = m_pDevice->CreatePixelShader(pixelShaderFile.at(0), pixelShaderFile.get_size(), nullptr, &Shader.m_pCirclePixelShader);
	xassert(hRes == S_OK, "Failed to create Circle Pixel Shader");
	m_RenderResources += Shader.m_pCirclePixelShader;

	// init buffers
	{
		static const array<float, 8> squarePoints = {
		   1.2f, 1.2f,
		   1.2f, -1.2f,
		   -1.2f, 1.2f,
		   -1.2f, -1.2f,
		};

		D3D11_BUFFER_DESC bufferDesc;
		memzero(bufferDesc);
		bufferDesc.ByteWidth = sizeof(squarePoints);
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA subResData;
		memzero(subResData);
		subResData.pSysMem = squarePoints.data();
		subResData.SysMemPitch = 0;
		subResData.SysMemSlicePitch = 0;

		hRes = m_pDevice->CreateBuffer(
			&bufferDesc,
			&subResData,
			&Buffer.m_pSquareVertexBuffer
		);
		xassert(hRes == S_OK, "Failed to create square vertex buffer");
		m_RenderResources += Buffer.m_pSquareVertexBuffer;

		static const D3D11_INPUT_ELEMENT_DESC Elements[] = {
		   { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		hRes = m_pDevice->CreateInputLayout(
			Elements,
			sizeof(Elements) / sizeof(Elements[0]),
			vertexShaderFile.at(0),
			vertexShaderFile.get_size(),
			&Buffer.m_pSquareInputLayout
		);
		xassert(hRes == S_OK, "Failed to create square input layout");
		m_RenderResources += Buffer.m_pSquareInputLayout;

		Buffer.m_uSquareVertices = 4;
		Buffer.m_uSquareStride = sizeof(float) * 2;
	}
	{
		D3D11_BUFFER_DESC bufferDesc;
		memzero(bufferDesc);
		bufferDesc.ByteWidth = sizeof(m_RenderState);
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		hRes = m_pDevice->CreateBuffer(
			&bufferDesc,
			nullptr,
			&Buffer.m_pRenderStateBuffer
		);
		xassert(hRes == S_OK, "Failed to create RenderState constant buffer");
		m_RenderResources += Buffer.m_pRenderStateBuffer;
	}
	// Instance Buffer
	{
		static const uint maxElements = 8192;

		const usize sz = sizeof(InstanceData);

		D3D11_BUFFER_DESC bufferDesc;
		memzero(bufferDesc);
		bufferDesc.ByteWidth = sizeof(InstanceData) * maxElements;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(InstanceData);

		hRes = m_pDevice->CreateBuffer(
			&bufferDesc,
			nullptr,
			&Buffer.m_pInstanceBuffer
		);
		xassert(hRes == S_OK, "Failed to create InstanceData buffer");
		m_RenderResources += Buffer.m_pInstanceBuffer;
		Buffer.m_uInstanceBufferElements = maxElements;

		// Create the SRV
		hRes = m_pDevice->CreateShaderResourceView(
			Buffer.m_pInstanceBuffer,
			nullptr,
			&Buffer.m_pShaderResourceView
		);
		xassert(hRes == S_OK, "Failed to create InstanceData buffer view");
		m_RenderResources += Buffer.m_pShaderResourceView;
	}

	init_targets();
}

void Renderer::clear_targets()
{
	for (IUnknown *resource : m_RenderTargets)
	{
		resource->Release();
	}
	m_RenderTargets.clear();
}

void Renderer::init_targets()
{
	// init targets - these must be regenerated on a window resize.
	{
		D3D11_TEXTURE2D_DESC textureDesc;
		memzero(textureDesc);

		textureDesc.Width = m_CurrentResolution.x;
		textureDesc.Height = m_CurrentResolution.y;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 8;
		textureDesc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		HRESULT hRes = m_pDevice->CreateTexture2D(
			&textureDesc,
			nullptr,
			&Target.m_pRenderTargetTexture
		);
		xassert(hRes == S_OK, "Failed to create render target texture for reason 0x%08X", hRes);

		D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
		memzero(viewDesc);

		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		viewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;

		hRes = m_pDevice->CreateRenderTargetView(
			Target.m_pRenderTargetTexture,
			&viewDesc,
			&Target.m_pRenderTargetView
		);
		xassert(hRes == S_OK, "Failed to create render target view");
		m_RenderTargets += Target.m_pRenderTargetTexture;
		m_RenderTargets += Target.m_pRenderTargetView;
	}
	{
		D3D11_TEXTURE2D_DESC textureDesc;
		memzero(textureDesc);

		textureDesc.Width = m_CurrentResolution.x;
		textureDesc.Height = m_CurrentResolution.y;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		textureDesc.SampleDesc.Count = 8;
		textureDesc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		HRESULT hRes = m_pDevice->CreateTexture2D(
			&textureDesc,
			nullptr,
			&Target.m_pDepthTargetTexture
		);
		xassert(hRes == S_OK, "Failed to create depth target texture for reason 0x%08X", hRes);

		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
		memzero(viewDesc);

		viewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		viewDesc.Flags = 0;
		viewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;

		hRes = m_pDevice->CreateDepthStencilView(
			Target.m_pDepthTargetTexture,
			&viewDesc,
			&Target.m_pDepthTargetView
		);
		xassert(hRes == S_OK, "Failed to create depth target view");
		m_RenderTargets += Target.m_pDepthTargetTexture;
		m_RenderTargets += Target.m_pDepthTargetView;
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthDesc = { 0 };
		depthDesc.DepthEnable = FALSE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthDesc.StencilEnable = FALSE;
		depthDesc.StencilReadMask = 0;
		depthDesc.StencilWriteMask = 0;
		depthDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

		HRESULT hRes = m_pDevice->CreateDepthStencilState(&depthDesc, &State.m_NoDepthStencilState);
		xassert(hRes == S_OK, "Failed to create depth-stencil state");
	}

	{
		D3D11_BLEND_DESC blendDesc = { 0 };
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		auto &blendTarget = blendDesc.RenderTarget[0];
		blendTarget.BlendEnable = TRUE;
		blendTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendTarget.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendTarget.BlendOp = D3D11_BLEND_OP_ADD;
		blendTarget.SrcBlendAlpha = D3D11_BLEND_ONE;
		blendTarget.DestBlendAlpha = D3D11_BLEND_ONE;
		blendTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		HRESULT hRes = m_pDevice->CreateBlendState(&blendDesc, &State.m_TransparentBlendState);
		xassert(hRes == S_OK, "Failed to create blend state");
	}
}

void Renderer::rebuild()
{
	release_all();
	init_all();
}

void Renderer::release_all()
{
	ImGui_ImplDX11_InvalidateDeviceObjects();

	if (State.m_NoDepthStencilState)
	{
		State.m_NoDepthStencilState->Release();
	}

	if (State.m_TransparentBlendState)
	{
		State.m_TransparentBlendState->Release();
	}

	if (m_pContext)
	{
		m_pContext->Flush();
		m_pContext->Release();
	}
	if (Target.m_pBackbufferTexture)
	{
		Target.m_pBackbufferTexture->Release();
	}
	for (IUnknown *resource : m_RenderResources)
	{
		resource->Release();
	}
	m_RenderResources.clear();
	for (IUnknown *resource : m_RenderTargets)
	{
		resource->Release();
	}
	m_RenderTargets.clear();

	if (m_pSwapChain)
	{
		m_pSwapChain->Release();
	}
	if (m_pDevice)
	{
		m_pDevice->Release();
	}
	if (m_pDevice3)
	{
		m_pDevice3->Release();
	}
}

void Renderer::init_all()
{
	auto windowResolution = m_pWindow->get_size();

	m_Viewport.TopLeftX = 0.0f;
	m_Viewport.TopLeftY = 0.0f;
	m_Viewport.Width = float(windowResolution.x);
	m_Viewport.Height = float(windowResolution.y);
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;

	static const D3D_FEATURE_LEVEL FeatureLevels[] = {
	   D3D_FEATURE_LEVEL_12_1,
	   D3D_FEATURE_LEVEL_12_0
	};

	D3D_FEATURE_LEVEL featureLevel;

	HRESULT hRes = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
#if XTD_DEBUG
		D3D11_CREATE_DEVICE_DEBUG |
#endif
		D3D11_CREATE_DEVICE_SINGLETHREADED,
		FeatureLevels,
		sizeof(FeatureLevels) / sizeof(FeatureLevels[0]),
		D3D11_SDK_VERSION,
		&m_pDevice,
		&featureLevel,
		&m_pContext
	);

	xassert(hRes == S_OK, "D3D11CreateDevice failed: 0x%08x", hRes);
	xassert(m_pDevice != nullptr, "device is null");
	xassert(m_pContext != nullptr, "m_pContext is null");


	hRes = m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&m_pDevice3);

	xassert(m_pDevice3 != nullptr, "m_pDevice3 is null");
	xassert(hRes == S_OK, "11.3 Device Creation Failed: 0x%08x", hRes);

	IDXGIFactory *DXGIFactory = nullptr;
	hRes = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&DXGIFactory);
	xassert(hRes == S_OK && DXGIFactory != nullptr, "Failed to create DXGI Factory");

	IDXGIAdapter *DXGIAdapter = nullptr;
	DXGIFactory->EnumAdapters(0, &DXGIAdapter);
	xassert(DXGIAdapter != nullptr, "Could not get DXGI adapter");

	IDXGIOutput *DXGIOutput = nullptr;
	DXGIAdapter->EnumOutputs(0, &DXGIOutput);

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	memzero(swapChainDesc);
	swapChainDesc.BufferDesc.Width = windowResolution.x;
	swapChainDesc.BufferDesc.Height = windowResolution.y;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	DXGIOutput->FindClosestMatchingMode(&swapChainDesc.BufferDesc, &swapChainDesc.BufferDesc, m_pDevice);
	swapChainDesc.BufferDesc.Width = windowResolution.x; // override
	swapChainDesc.BufferDesc.Height = windowResolution.y; // override
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 3;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapChainDesc.OutputWindow = (HWND)m_pWindow->_get_platform_handle();
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SampleDesc.Count = 1;

	hRes = DXGIFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
	xassert(hRes == S_OK && m_pSwapChain != nullptr, "Could not create DXGI Swapchain");

	hRes = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&Target.m_pBackbufferTexture);
	xassert(hRes == S_OK && Target.m_pBackbufferTexture, "Could not get DXGI backbuffer texture");

	m_CurrentResolution = windowResolution;

	DXGIOutput->Release();
	DXGIAdapter->Release();
	DXGIFactory->Release();

	init_resources();

	bool imguiRes = ImGui_ImplDX11_Init((HWND)m_pWindow->_get_platform_handle(), m_pDevice, m_pContext);
	xassert(imguiRes, "Failed to initialize imgui");
	imguiRes = ImGui_ImplDX11_CreateDeviceObjects();
	xassert(imguiRes, "Failed to configure imgui");

	extern uptr imgui_wndproc_hook(void *hWnd, uint msg, uint64 wParam, uint64 lParam);
	m_pWindow->set_callback_hook_native(&imgui_wndproc_hook);

	// Kick off the draw thread.
	event waitForThreadStart;
	if (!m_DrawThread.started())
	{
		m_DrawThread = [this, &waitForThreadStart] {
			waitForThreadStart.set();

			render_loop();
		};
		m_DrawThread.set_name("Render Thread");
		m_DrawThread.start();

		waitForThreadStart.join();
	}
}

void Renderer::move_screen(const vector2D &mouseMove)
{
	vector2D mouseMoveAdj = mouseMove;
	mouseMoveAdj.x = -mouseMoveAdj.x;

	if (m_Viewport.Width > m_Viewport.Height)
	{
		float Width = m_Viewport.Width / m_Viewport.Height;
		mouseMoveAdj.x *= Width;
	}
	else
	{
		float Height = m_Viewport.Height / m_Viewport.Width;
		mouseMoveAdj.y *= Height;
	}

	mouseMoveAdj *= 2.0;
	mouseMoveAdj *= m_ZoomScalar;

	scoped_lock<mutex> _lock(m_CameraLock);
	m_DirtyCameraOffset += mouseMoveAdj;

	// clamp camera position
	m_DirtyCameraOffset.x = clamp(m_DirtyCameraOffset.x, -(double)options::WorldRadius, (double)options::WorldRadius);
	m_DirtyCameraOffset.y = clamp(m_DirtyCameraOffset.y, -(double)options::WorldRadius, (double)options::WorldRadius);
}

void Renderer::set_screen_position(const vector2D &position)
{
	scoped_lock<mutex> _lock(m_CameraLock);
	m_DirtyCameraOffset = position;
	m_DirtyCameraOffset.x = clamp(m_DirtyCameraOffset.x, -(double)options::WorldRadius, (double)options::WorldRadius);
	m_DirtyCameraOffset.y = clamp(m_DirtyCameraOffset.y, -(double)options::WorldRadius, (double)options::WorldRadius);
}

void Renderer::adjust_zoom(int zoomDelta)
{
	scoped_lock<mutex> _lock(m_CameraLock);
	m_Zoom += zoomDelta;
	// m_ZoomScalar
	if (m_Zoom > 0)
	{
		m_DirtyZoomScalar = pow(2.0, double(m_Zoom) * 0.1);
	}
	else if (m_Zoom < 0)
	{
		m_DirtyZoomScalar = pow(2.0, double(m_Zoom) * 0.1);
	}
	else
	{
		m_DirtyZoomScalar = 1.0;
	}
}

vector2D Renderer::unproject(const vector2D &screenPos)
{
	vector4D screenPosAdj = { (screenPos.x * 2.0) - 1.0, -((screenPos.y * 2.0) - 1.0), 0.0, 1.0 };

	scoped_lock<mutex> _lock(m_CameraLock);

	return vector2F(m_ViewProjection.inverse().transpose() * screenPosAdj);
}

void Renderer::update_from_sim(float light, uint64 frame, const wide_array<InstanceData> &instanceData, const array<uint8> &lightData, uint lightEdge, const array<uint32> &wasteData, uint wasteEdge, Renderer::UIData &uiData, const array<atomic<uint32>, VM::NumOperations + 1> &VMInstructionTrackerArray)
{
	scoped_lock<mutex> _lock(m_FramePendingLock);
	m_PendingFrame = frame;

	m_PendingArray.resize(instanceData.size());
	memcpy(m_PendingArray.data(), instanceData.data(), instanceData.size_raw());

	m_PendingLightData.resize(lightData.size());
	memcpy(m_PendingLightData.data(), lightData.data(), lightData.size_raw());

	m_LightEdge = lightEdge;
	m_Illumination = light;

	m_PendingWasteData.resize(wasteData.size());
	const usize wasteSize = wasteData.size();
	for (usize i = 0; i < wasteSize; ++i)
	{
		m_PendingWasteData[i] = min(1.0f, float(double(wasteData[i]) / 10000000.0));
	}

	m_WasteEdge = wasteEdge;

	m_UIData = (UIData &&)uiData;

	for (uint i = 0; i < VMInstructionTrackerArray.size(); ++i)
	{
		m_InstructionTracker[i] = VMInstructionTrackerArray[i];
	}
}

void Renderer::update_hash_name(const string_view &hash_name)
{
	scoped_lock<mutex> _lock(m_FramePendingLock);
	m_HashName = hash_name;
}

void Renderer::update_from_sim(const Renderer::UIData &uiData)
{
	scoped_lock<mutex> _lock(m_FramePendingLock);
	m_UIData = uiData;
}

void Renderer::halt()
{
	m_DrawThreadRun = false;
	if (m_DrawThread.started())
	{
		m_DrawThread.join();
	}
}

void Renderer::openInstructionStats()
{
	m_InstructionStatsOpenRequested = true;
}

void Renderer::openSettings()
{
	m_SettingsOpenRequested = true;
}
