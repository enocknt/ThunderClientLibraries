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
 
#pragma once

#include "Module.h"

#include <interfaces/IMemory.h>

namespace Thunder {
namespace Plugin {
    class ClientCompositorRender : public PluginHost::IPlugin{
    private:
        class Notification : public RPC::IRemoteConnection::INotification, public PluginHost::IStateControl::INotification {
        public:
            Notification() = delete;
            Notification(const Notification&) = delete;
            explicit Notification(ClientCompositorRender* parent)
                : _parent(*parent)
            {
                ASSERT(parent != nullptr);
            }
            ~Notification() override
            {
                TRACE(Trace::Information, (_T("ClientCompositorRender::Notification destructed. Line: %d"), __LINE__));
            }

        public:
            void Activated(RPC::IRemoteConnection* /* connection */) override
            {
            }
            void Deactivated(RPC::IRemoteConnection* connectionId) override
            {
                _parent.Deactivated(connectionId);
            }
            void Terminated(RPC::IRemoteConnection* /* connection */) override
            {
            }


            void StateChange(const PluginHost::IStateControl::state state) override {
                _parent.StateChange(state);
            }

            BEGIN_INTERFACE_MAP(Notification)
            INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
            INTERFACE_ENTRY (PluginHost::IStateControl::INotification)
            END_INTERFACE_MAP

        private:
            ClientCompositorRender& _parent;
        };

    public:
        ClientCompositorRender(const ClientCompositorRender&) = delete;
        ClientCompositorRender& operator=(const ClientCompositorRender&) = delete;
PUSH_WARNING(DISABLE_WARNING_THIS_IN_MEMBER_INITIALIZER_LIST)
        ClientCompositorRender()
            : _connectionId(0)
            , _service(nullptr)
            , _memory(nullptr)
            , _statecontrol(nullptr)
            , _notification(this)
        {
        }
POP_WARNING()
        ~ClientCompositorRender() override = default;

        BEGIN_INTERFACE_MAP(ClientCompositorRender)
        INTERFACE_ENTRY(IPlugin)
        INTERFACE_AGGREGATE(Exchange::IMemory, _memory)
        INTERFACE_AGGREGATE(PluginHost::IStateControl, _statecontrol)
        END_INTERFACE_MAP

    public:
        // IPlugin  
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

        void StateChange(PluginHost::IStateControl::state);
    private:
        void Deactivated(RPC::IRemoteConnection* connection);

    private:
        uint32_t _skipURL;
        uint32_t _connectionId;
        PluginHost::IShell* _service;
        Exchange::IMemory* _memory;
        PluginHost::IStateControl* _statecontrol;
        Core::SinkType<Notification> _notification;
    };
}
}
