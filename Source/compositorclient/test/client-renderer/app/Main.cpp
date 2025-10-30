/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Module.h"

#include "TextureBounce.h"
#include "IModel.h"
#include "TerminalInput.h"
#include "Renderer.h"

#include <core/core.h>

#include <localtracer/localtracer.h>

#include <cstdio>
#include <unistd.h>

using namespace Thunder;

namespace {
const char Namespace[] = EXPAND_AND_QUOTE(NAMESPACE);
}

class ConsoleOptions : public Thunder::Core::Options {
public:
    ConsoleOptions(int argc, TCHAR* argv[])
        : Thunder::Core::Options(argc, argv, _T("t:n:W:H:h"))
        , Texture("/usr/share/" + std::string(Namespace) + "/ClientCompositorRender/ml-tv-color-small.png")
        , Width(1920)
        , Height(1080)
    {
        Parse();
    }

    std::string Texture;
    uint8_t TextureNumber;
    uint16_t Width;
    uint16_t Height;

private:
    void Option(const TCHAR option, const TCHAR* argument) override
    {
        switch (option) {
        case 't':
            Texture = argument;
            break;
        case 'n':
            TextureNumber = static_cast<uint8_t>(std::stoi(argument));
            break;
        case 'W':
            Width = static_cast<uint16_t>(std::stoi(argument));
            break;
        case 'H':
            Height = static_cast<uint16_t>(std::stoi(argument));
            break;
        case 'h':
        default:
            fprintf(stderr, "Usage: " EXPAND_AND_QUOTE(APPLICATION_NAME) " [-t <Texture.png>] [-n 40] [-W 1280] [-H 720]\n");
            exit(EXIT_FAILURE);
        }
    }
};

int main(int argc, char* argv[])
{
    Messaging::LocalTracer& tracer = Messaging::LocalTracer::Open();

    const std::map<std::string, std::vector<std::string>> modules = {
        { "App_CompositionClientRender", { "" } },
        { "Common_CompositionClientRender", { "" } },
        { "CompositorBuffer", { "Error", "Information" } },
        { "CompositorBackend", { "Error" } },
        { "CompositorRenderer", { "Error", "Warning", "Information" } },
        { "DRMCommon", { "Error", "Warning", "Information" } }
    };

    for (const auto& module_entry : modules) {
        for (const auto& category : module_entry.second) {
            tracer.EnableMessage(module_entry.first, category, true);
        }
    }

    const char* executableName(Thunder::Core::FileNameOnly(argv[0]));
    ConsoleOptions options(argc, argv);
    bool quitApp(false);

    TRACE_GLOBAL(Trace::Information, ("%s - build: %s", executableName, __TIMESTAMP__));

    std::string texturePath = options.Texture;

    if (texturePath.empty()) {
        texturePath = "/usr/share/" + std::string(Namespace) + "/ClientCompositorRender/ml-tv-color-small.png";
    }

    Compositor::TextureBounce::Config config;
    config.Image = texturePath;
    config.ImageCount = options.TextureNumber;

    std::string configStr;
    config.ToString(configStr);

    {
        Compositor::Render renderer;
        Compositor::TextureBounce model;

        if (renderer.Configure(options.Width, options.Height) == false) {
            fprintf(stderr, "Failed to initialize renderer\n");
            Core::Singleton::Dispose();
            return 1;
        }

        if (!renderer.Register(&model, configStr)) {
            fprintf(stderr, "Failed to initialize model\n");
            Core::Singleton::Dispose();
            return 1;
        }

        Compositor::TerminalInput keyboard;
        ASSERT(keyboard.IsValid() == true);

        renderer.Start();

        bool result;

        if (keyboard.IsValid() == true) {
            while (!renderer.ShouldExit() && !quitApp) {
                switch (toupper(keyboard.Read())) {
                case 'S':
                    if (renderer.ShouldExit() == false) {
                        (renderer.IsRunning() == false) ? renderer.Start() : renderer.Stop();
                    }
                    break;
                case 'F':
                    result = renderer.ToggleFPS();
                    printf("%d FPS: %s\n", __LINE__, result ? "off" : "on");
                    break;
                case 'Z':
                    result = renderer.ToggleRequestRender();
                    printf("%d RequestRender: %s\n", __LINE__, result ? "off" : "on");
                    break;
                case 'R':
                    renderer.TriggerRender();
                    break;
                case 'M':
                    result = renderer.ToggleModelRender();
                    printf("%d Model Render: %s\n", __LINE__, result ? "off" : "on");
                    break;
                case 'Q':
                    quitApp = true;
                    break;
                case 'H':
                    TRACE_GLOBAL(Trace::Information, ("Available commands:"));
                    TRACE_GLOBAL(Trace::Information, ("  S - Start/Stop the rendering"));
                    TRACE_GLOBAL(Trace::Information, ("  F - Show current FPS"));
                    TRACE_GLOBAL(Trace::Information, ("  Q - Quit the application"));
                    TRACE_GLOBAL(Trace::Information, ("  H - Show this help message"));
                    break;
                default:
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            TRACE_GLOBAL(Thunder::Trace::Error, ("Failed to initialize keyboard input"));
        }

        renderer.Stop();
        TRACE_GLOBAL(Thunder::Trace::Information, ("Exiting %s.... ", executableName));
    }

    Core::Singleton::Dispose();

    return 0;
}
