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

#include "Renderer.h"

#include <sstream>
#include <iomanip>
#include <inttypes.h>
#include "Fonts/Arial.h"

namespace Thunder {
namespace Compositor {

    static const char Namespace[] = EXPAND_AND_QUOTE(NAMESPACE);

    Render::Render()
        : _display(nullptr)
        , _surface(nullptr)
        , _displayName()
        , _canvasWidth(0)
        , _canvasHeight(0)
        , _eglDisplay(EGL_NO_DISPLAY)
        , _eglContext(EGL_NO_CONTEXT)
        , _eglSurface(EGL_NO_SURFACE)
        , _render()
        , _exitMutex()
        , _exitSignal()
        , _exitRequested(false)
        , _running(false)
        , _rendering()
        , _renderSync()
        , _showFps(true)
        , _models()
        , _selectedModel(~0)
        , _rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
        , _textRender(&Arial)
        , _lastFPSUpdate(0)
        , _frameCount(0)
        , _currentFPS(0.0f)
    {
    }

    bool Render::Configure(const uint16_t width, const uint16_t height)
    {
        _canvasWidth = width;
        _canvasHeight = height;

        _lastFPSUpdate = Core::Time::Now().Ticks();

        _displayName = Compositor::IDisplay::SuggestedName();

        if (_displayName.empty()) {
            pid_t pid = getpid();
            char uniqueName[64];
            snprintf(uniqueName, sizeof(uniqueName), "CompositorClient-%d-%" PRIu64, pid, _lastFPSUpdate);
            _displayName = uniqueName;
        }

        _display = Compositor::IDisplay::Instance(_displayName);

        if (_display != nullptr) {
            _surface = _display->Create(_displayName, width, height, this);
            ASSERT(_surface != nullptr);

            if (!InitializeEGL()) {
                fprintf(stderr, "Failed to initialize EGL\n");
                return false;
            }

            TextRender::Config config;
            config.FontAtlas = "/usr/share/" + std::string(Namespace) + "/ClientCompositorRender/Arial.png";
            config.Scale = 1.0f;
            config.Red = 0.0f; // Green text
            config.Green = 1.0f;
            config.Blue = 0.0f;
            config.Alpha = 1.0f;

            std::string configStr;
            config.ToString(configStr);

            if (!_textRender.Initialize(width, height, configStr)) {
                fprintf(stderr, "Failed to initialize FPS counter\n");
                return false;
            }

            // Release EGL context for render thread
            if (!eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
                EGLint error = eglGetError();
                fprintf(stderr, "Failed to release EGL context: 0x%x\n", error);
                return false;
            }

            return true;
        } else {
            fprintf(stderr, "Failed to initialize display\n");
            return false;
        }
    }

    // ICallback
    void Render::Rendered(Thunder::Compositor::IDisplay::ISurface*)
    {
        _renderSync.notify_all();
    }

    void Render::Published(Thunder::Compositor::IDisplay::ISurface*)
    {
        _frameCount++;

        uint64_t now = Core::Time::Now().Ticks();
        uint64_t elapsed = now - _lastFPSUpdate;

        // Update FPS every second (1000000 microseconds)
        if (elapsed >= 1000000) {
            _currentFPS = (_frameCount * 1000000.0f) / elapsed;
            _frameCount = 0;
            _lastFPSUpdate = now;
        }
    }

    bool Render::Register(IModel* model, const std::string& config)
    {
        bool result = InitializeModel(model, config);

        if (result == true) {
            _models.push_back(model);
        } else {
            fprintf(stderr, "Failed to initialize model during registration\n");
        }

        return result;
    }

    void Render::Unregister(IModel* model)
    {
        auto it = std::find(_models.begin(), _models.end(), model);
        if (it != _models.end()) {
            _models.erase(it);
        }
    }

    bool Render::InitializeModel(IModel* model, const std::string& config)
    {
        if (model == nullptr) {
            return false;
        }

        // Make context current
        if (!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
            EGLint error = eglGetError();
            fprintf(stderr, "InitializeModel: eglMakeCurrent failed: 0x%x\n", error);
            return false;
        }

        // Initialize model with active context
        bool result = model->Initialize(_canvasWidth, _canvasHeight, config);

        // Release context
        if (!eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
            EGLint error = eglGetError();
            fprintf(stderr, "InitializeModel: release failed: 0x%x\n", error);
        }

        return result;
    }

    bool Render::InitializeEGL()
    {
        EGLNativeDisplayType display = (_display != nullptr) ? _display->Native() : EGL_DEFAULT_DISPLAY;
        EGLNativeWindowType window = (_surface != nullptr) ? _surface->Native() : 0;

        _eglDisplay = eglGetDisplay(display);
        if (_eglDisplay == EGL_NO_DISPLAY) {
            fprintf(stderr, "eglGetDisplay failed\n");
            return false;
        }

        if (!eglInitialize(_eglDisplay, nullptr, nullptr)) {
            fprintf(stderr, "eglInitialize failed\n");
            return false;
        }

        static const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };

        EGLConfig eglConfig;
        EGLint numConfigs;
        if (!eglChooseConfig(_eglDisplay, configAttribs, &eglConfig, 1, &numConfigs)) {
            fprintf(stderr, "eglChooseConfig failed\n");
            return false;
        }

        static const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        _eglContext = eglCreateContext(_eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
        if (_eglContext == EGL_NO_CONTEXT) {
            fprintf(stderr, "eglCreateContext failed\n");
            return false;
        }

        _eglSurface = eglCreateWindowSurface(_eglDisplay, eglConfig, window, nullptr);
        if (_eglSurface == EGL_NO_SURFACE) {
            fprintf(stderr, "eglCreateWindowSurface failed\n");
            return false;
        }

        if (!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
            EGLint error = eglGetError();
            fprintf(stderr, "eglMakeCurrent failed: 0x%x\n", error);
            return false;
        }

        return true;
    }

    void Render::CleanupEGL()
    {
        if (_eglDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (_eglSurface != EGL_NO_SURFACE) {
                eglDestroySurface(_eglDisplay, _eglSurface);
                _eglSurface = EGL_NO_SURFACE;
            }
            if (_eglContext != EGL_NO_CONTEXT) {
                eglDestroyContext(_eglDisplay, _eglContext);
                _eglContext = EGL_NO_CONTEXT;
            }
            eglTerminate(_eglDisplay);
            _eglDisplay = EGL_NO_DISPLAY;
        }
    }

    void Render::Draw()
    {
        while (_running.load() && !ShouldExit()) {
            if (_models.empty() || _selectedModel >= _models.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Make context current for this render thread
            if (!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
                EGLint error = eglGetError();
                fprintf(stderr, "Draw: eglMakeCurrent failed: 0x%x\n", error);
                break;
            }

            if (_models[_selectedModel.load()]->Draw() == true) {

                // Draw FPS if enabled
                if (_showFps) {
                    _textRender.Draw(_displayName, 10, _canvasHeight - 40);
                    std::ostringstream ss;
                    ss << "FPS: " << std::fixed << std::setprecision(2) << _currentFPS;
                    _textRender.Draw(ss.str(), 10, 10);
                }

                // Swap buffers
                eglSwapBuffers(_eglDisplay, _eglSurface);

                _surface->RequestRender();

                if (WaitForRendered(1000) == Core::ERROR_TIMEDOUT) {
                    TRACE(Trace::Warning, ("Timed out waiting for rendered callback"));
                }
            } else {
                TRACE(Trace::Warning, ("Model draw failed"));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Release context when done
        eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    uint32_t Render::WaitForRendered(uint32_t timeoutMs)
    {
        uint32_t ret = Core::ERROR_NONE;

        std::unique_lock<std::mutex> lock(_rendering);
        if (timeoutMs == Core::infinite) {
            _renderSync.wait(lock);
        } else {
            if (_renderSync.wait_for(lock, std::chrono::milliseconds(timeoutMs)) == std::cv_status::timeout) {
                ret = Core::ERROR_TIMEDOUT;
            }
        }

        return ret;
    }
} // namespace Compositor
} // namespace Thunder