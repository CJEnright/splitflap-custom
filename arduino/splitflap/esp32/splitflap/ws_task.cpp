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
#if WS
#include "WS_task.h"

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <lwip/apps/sntp.h>

#include <WebSocketsClient.h>

#include "secrets.h"

// Timezone for local time strings; this is America/New_York. See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "EST5EDT,M3.2.0,M11.1.0"

bool WSTask::handleData(String data)
{
    // Extract data from the json response. You could use ArduinoJson, but I find json11 to be much
    // easier to use albeit not optimized for a microcontroller.

    // Example data:
    /*
        {
            ...
            "STATION": [
                {
                    "STID": "F4637",
                    "OBSERVATIONS": {
                        "wind_speed_value_1": {
                            "date_time": "2021-11-30T23:25:00Z",
                            "value": 0.87
                        },
                        "air_temp_value_1": {
                            "date_time": "2021-11-30T23:25:00Z",
                            "value": 69
                        }
                    },
                    ...
                },
                {
                    "STID": "C5988",
                    "OBSERVATIONS": {
                        "wind_speed_value_1": {
                            "date_time": "2021-11-30T23:24:00Z",
                            "value": 1.74
                        },
                        "air_temp_value_1": {
                            "date_time": "2021-11-30T23:24:00Z",
                            "value": 68
                        }
                    },
                    ...
                },
                ...
            ]
        }
    */
    /*

        // Validate json structure and extract data:
        auto station = json["STATION"];
        if (!station.is_array())
        {
            logger_.log("Parse error: STATION");
            return false;
        }
        auto station_array = station.array_items();

        std::vector<double> temps;
        std::vector<double> wind_speeds;

        for (uint8_t i = 0; i < station_array.size(); i++)
        {
            auto item = station_array[i];
            if (!item.is_object())
            {
                logger_.log("Bad station item, ignoring");
                continue;
            }
            auto observations = item["OBSERVATIONS"];
            if (!observations.is_object())
            {
                logger_.log("Bad station observations, ignoring");
                continue;
            }

            auto air_temp_value = observations["air_temp_value_1"];
            if (!air_temp_value.is_object())
            {
                logger_.log("Bad air_temp_value_1, ignoring");
                continue;
            }
            auto value = air_temp_value["value"];
            if (!value.is_number())
            {
                logger_.log("Bad air temp, ignoring");
                continue;
            }
            temps.push_back(value.number_value());

            auto wind_speed_value = observations["wind_speed_value_1"];
            if (!wind_speed_value.is_object())
            {
                logger_.log("Bad wind_speed_value_1, ignoring");
                continue;
            }
            value = wind_speed_value["value"];
            if (!value.is_number())
            {
                logger_.log("Bad wind speed, ignoring");
                continue;
            }
            wind_speeds.push_back(value.number_value());
        }

        auto entries = temps.size();
        if (entries == 0)
        {
            logger_.log("No data found");
            return false;
        }

        // Calculate medians
        std::sort(temps.begin(), temps.end());
        std::sort(wind_speeds.begin(), wind_speeds.end());
        double median_temp;
        double median_wind_speed;
        if ((entries % 2) == 0)
        {
            median_temp = (temps[entries / 2 - 1] + temps[entries / 2]) / 2;
            median_wind_speed = (wind_speeds[entries / 2 - 1] + wind_speeds[entries / 2]) / 2;
        }
        else
        {
            median_temp = temps[entries / 2];
            median_wind_speed = wind_speeds[entries / 2];
        }

        char buf[200];
        snprintf(buf, sizeof(buf), "Medians from %d stations: temp=%dºF, wind speed=%d knots", entries, (int)median_temp, (int)median_wind_speed);
        logger_.log(buf);

        // Construct the messages to display
        messages_.clear();

        snprintf(buf, sizeof(buf), "%d f", (int)median_temp);
        messages_.push_back(String(buf));

        snprintf(buf, sizeof(buf), "%d mph", (int)(median_wind_speed * 1.151));
        messages_.push_back(String(buf));

        // Show the data fetch time on the LCD
        time_t now;
        time(&now);
        strftime(buf, sizeof(buf), "Data: %Y-%m-%d %H:%M:%S", localtime(&now));
        display_task_.setMessage(0, String(buf));
    return true;
     */
}

WSTask::WSTask(SplitflapTask &splitflap_task, DisplayTask &display_task, Logger &logger, const uint8_t task_core) : Task("WS", 8192, 1, task_core),
                                                                                                                    splitflap_task_(splitflap_task),
                                                                                                                    display_task_(display_task),
                                                                                                                    logger_(logger),
                                                                                                                    wifi_client_()
{
}

void WSTask::connectWifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setHostname(HOSTNAME);
    char buf[256];

    logger_.log("Establishing connection to WiFi..");
    snprintf(buf, sizeof(buf), "Wifi connecting to %s", WIFI_SSID);
    display_task_.setMessage(1, String(buf));
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
    }

    snprintf(buf, sizeof(buf), "Connected to network %s", WIFI_SSID);
    logger_.log(buf);

    // Sync SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    char server[] = "time.nist.gov"; // sntp_setservername takes a non-const char*, so use a non-const variable to avoid warning
    sntp_setservername(0, server);
    sntp_init();

    logger_.log("Waiting for NTP time sync...");
    snprintf(buf, sizeof(buf), "Syncing NTP time via %s...", server);
    display_task_.setMessage(1, String(buf));
    time_t now;
    while (time(&now), now < 1625099485)
    {
        delay(1000);
    }

    setenv("TZ", TIMEZONE, 1);
    tzset();
    strftime(buf, sizeof(buf), "Got time: %Y-%m-%d %H:%M:%S", localtime(&now));
    logger_.log(buf);
}

void WSTask::loop(WStype_t type, uint8_t *payload, size_t length)
{
    char buf[max(NUM_MODULES + 1, 256)];
    size_t len = 0;

    logger_.log("In loop");

    switch (type)
    {
    case WStype_DISCONNECTED:
        logger_.log("[WSc] Disconnected!");
        break;
    case WStype_CONNECTED:
        logger_.log("[WSc] Connected to url");

        // send message to server when Connected
        webSocket.sendTXT("Connected");
        break;
    case WStype_TEXT:

        snprintf(buf, sizeof(buf), "[WSc] get text: %s", payload);
        logger_.log(buf);

        // Pad message for display
        len = strlcpy(buf, (char *)payload, sizeof(buf));
        memset(buf + len, ' ', sizeof(buf) - len);

        splitflap_task_.showString(buf, NUM_MODULES);

        snprintf(buf, sizeof(buf), "[WSc] set text: %s", payload);
        logger_.log(buf);

        break;
    case WStype_BIN:
        logger_.log("[WSc] get binary length");

        // send data to server
        break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }

    String wifi_status;
    switch (WiFi.status())
    {
    case WL_IDLE_STATUS:
        wifi_status = "Idle";
        break;
    case WL_NO_SSID_AVAIL:
        wifi_status = "No SSID";
        break;
    case WL_CONNECTED:
        wifi_status = String(WIFI_SSID) + " " + WiFi.localIP().toString();
        break;
    case WL_CONNECT_FAILED:
        wifi_status = "Connection failed";
        break;
    case WL_CONNECTION_LOST:
        wifi_status = "Connection lost";
        break;
    case WL_DISCONNECTED:
        wifi_status = "Disconnected";
        break;
    default:
        wifi_status = "Unknown";
        break;
    }
    display_task_.setMessage(1, String("Wifi: ") + wifi_status);
}

void WSTask::run()
{
    connectWifi();

    // server address, port and URL
    webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH);

    // event handler
    webSocket.onEvent(std::bind(&WSTask::loop, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // try ever 5000 again if connection has failed
    webSocket.setReconnectInterval(5000);

    while (1)
    {
        webSocket.loop();
        delay(10);
    }
}
#endif
