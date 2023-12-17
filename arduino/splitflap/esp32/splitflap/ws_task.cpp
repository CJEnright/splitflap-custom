/*
    Websocket task, copied from http_task
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

void WSTask::process_message(const uint8_t *payload)
{
    char message[max(NUM_MODULES + 1, 256)];
    size_t len = strlcpy(message, (char *)payload, sizeof(message));

    if (strncmp(message, "calibrate", 9) == 0)
    {
        splitflap_task_.resetAll();
    }
    else if (strncmp(message, "text ", 5) == 0)
    {
        // Remove "text " prefix
        memmove(message, message + 5, len - 5 + 1);

        // Pad message for display
        memset(message + len, ' ', sizeof(message) - len);
        String message_string = String(message);

        if (last_message_ != message_string)
        {
            splitflap_task_.showString(message, NUM_MODULES, false);
            last_message_ = message_string;
        }
    }
}

void WSTask::loop(WStype_t type, uint8_t *payload, size_t length)
{
    char buf[max(NUM_MODULES + 1, 256)];

    switch (type)
    {
    case WStype_DISCONNECTED:
        logger_.log("[WSc] Disconnected!");
        break;
    case WStype_CONNECTED:
        logger_.log("[WSc] Connected to url");
        break;
    case WStype_TEXT:
        snprintf(buf, sizeof(buf), "[WSc] get text: %s", payload);
        logger_.log(buf);

        process_message(payload);

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

/*

* Investigate single character changes
* Investigate ghost flaps
* Hook up recalibration

*/