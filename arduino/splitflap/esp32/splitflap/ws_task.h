/*
   Copyright 2021 Scott Bezek and the splitflap contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       WS://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#pragma once

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <WebSocketsClient.h>

#include "../core/logger.h"
#include "../core/splitflap_task.h"
#include "../core/task.h"

#include "display_task.h"

class WSTask : public Task<WSTask>
{
    friend class Task<WSTask>; // Allow base Task to invoke protected run()

public:
    WSTask(SplitflapTask &splitflap_task, DisplayTask &display_task, Logger &logger, const uint8_t task_core);

protected:
    void run();

private:
    void connectWifi();
    void loop(WStype_t type, uint8_t *payload, size_t length);

    SplitflapTask &splitflap_task_;
    DisplayTask &display_task_;
    Logger &logger_;
    WiFiClient wifi_client_;
    WebSocketsClient webSocket;
};
