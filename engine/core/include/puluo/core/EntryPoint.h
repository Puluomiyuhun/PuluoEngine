#pragma once

#include "puluo/core/Log.h"
#include "puluo/core/Application.h"

extern Puluo::Application* Puluo::CreateApplication();

int main(int argc, char** argv) {
    Puluo::Log::Init();
    PULUO_CORE_INFO("PuluoEngine v0.1.0 starting...");

    auto app = Puluo::CreateApplication();
    app->Run();
    delete app;

    PULUO_CORE_INFO("PuluoEngine shutdown.");
    return 0;
}
