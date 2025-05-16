#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION 
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> 

#include "market_replay/logger.hpp"
#include "market_replay/dispatcher.hpp"
#include "market_replay/latency_model.hpp"
#include "market_replay/metrics.hpp"
#include <thread> 
#include <atomic> 
#include <string>   // For std::string
#include <vector>   // For std::vector (if needed later for log buffer)
#include <iostream> // For std::cerr in glfw_error_callback

// Forward declare strategy factory
namespace market_replay {
    std::unique_ptr<IStrategy> create_basic_strategy(const StrategyId& id,
                                                     IOrderSubmitter* order_submitter,
                                                     std::shared_ptr<MetricsCollector> metrics_collector);
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl; 
}

// --- Simulation State (Global or in a class) ---
std::atomic<bool> g_simulation_running = {false}; 
std::atomic<bool> g_simulation_finished_once = {false}; 
std::thread g_simulation_thread;

// Function to run the simulation (will be called in a new thread)
void run_simulation_task(
    std::string data_file_path,
    market_replay::LatencyModel::Config latency_cfg,
    std::string strategy_to_run
) {
    g_simulation_running = true;
    g_simulation_finished_once = false; // Reset this flag
    // Initialize logger for this simulation run. Make sure it's thread-safe if called multiple times
    // or ensure it's initialized once globally if that's your logger design.
    // For simplicity, let's assume Logger::init can be called; if it's a singleton, it will handle it.
    market_replay::Logger::init("gui_simulator_log.txt", spdlog::level::info, spdlog::level::debug, true);
    LOG_INFO("Simulation thread started via GUI for data: {}", data_file_path);

    try {
        auto metrics_collector = std::make_shared<market_replay::MetricsCollector>(
            "gui_sim_trades.csv", "gui_sim_latency.csv", "gui_sim_pnl.csv");
        
        market_replay::Dispatcher dispatcher(data_file_path, latency_cfg, metrics_collector);
        if (strategy_to_run == "BasicStrategy") {
            dispatcher.add_strategy("BasicStrat_GUI_1", market_replay::create_basic_strategy);
        }
        // TODO: Add more strategies based on UI selection

        dispatcher.run(); // Blocking call

        metrics_collector->report_final_metrics();
        LOG_INFO("Simulation task finished successfully.");

    } catch (const std::exception& e) {
        LOG_CRITICAL("Exception in simulation task: {}", e.what());
    }
    
    market_replay::Logger::shutdown(); // Shutdown logger for this thread/run
    g_simulation_running = false;
    g_simulation_finished_once = true;
}


int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    const char* glsl_version = ""; // Initialize
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Market Replay Simulator GUI", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate(); // Terminate GLFW before returning
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); // 'window' is now declared
    ImGui_ImplOpenGL3_Init(glsl_version);       // 'glsl_version' is now declared

    // UI State variables
    static char data_file_buf[256] = "../data/sample_ticks.csv"; 
    static float market_data_latency_us = 50.0f;


    // Main loop
    while (!glfwWindowShouldClose(window)) { // 'window' is known
        glfwPollEvents(); // 'glfwPollEvents' is known via GLFW/glfw3.h

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(); // Good for verifying ImGui itself is working

        ImGui::Begin("Configuration");
        ImGui::InputText("Data File Path", data_file_buf, IM_ARRAYSIZE(data_file_buf));
        ImGui::SliderFloat("Market Data Latency (us)", &market_data_latency_us, 0.0f, 1000.0f);


        if (g_simulation_running) { // 'g_simulation_running' is known (global atomic)
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Run Simulation")) {
            if (!g_simulation_running) {
                if (g_simulation_thread.joinable()) {
                    g_simulation_thread.join();
                }
                market_replay::LatencyModel::Config cfg;
                cfg.market_data_feed_latency = std::chrono::microseconds(static_cast<long long>(market_data_latency_us));

                g_simulation_thread = std::thread(run_simulation_task, 
                                                  std::string(data_file_buf), // Create std::string from C-style string
                                                  cfg,
                                                  "BasicStrategy"); // TODO: Get strategy from UI
            }
        }
        if (g_simulation_running) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text("Simulation Running...");
        } else if (g_simulation_finished_once) { // 'g_simulation_finished_once' is known
            ImGui::SameLine();
            ImGui::Text("Simulation Finished. Check output files.");
        }

        ImGui::End(); 


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h); // 'window' is known
        glViewport(0, 0, display_w, display_h); // 'glViewport' is known via OpenGL headers (dragged by glfw3.h)
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f); // 'glClearColor' is known
        glClear(GL_COLOR_BUFFER_BIT); // 'glClear' and 'GL_COLOR_BUFFER_BIT' are known
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window); 
    }

    if (g_simulation_thread.joinable()) {
        g_simulation_thread.join();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}