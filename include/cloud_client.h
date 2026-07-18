#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "state.h"

/**
 * @brief Cloud client for Azure Function integration
 *
 * A background task short-polls GET /sync (~3x per second while the
 * joystick is active, slower when idle). One sync response carries:
 * - the latest joystick target (latest-value semantics via Table Storage -
 *   stale intermediate values are skipped by design). Targets older than
 *   the staleness window are ignored so a vanished frontend cannot keep
 *   moving the platform.
 * - the axis limits (rotation/tilt) when set in the cloud frontend. The
 *   WebUI owns these values; the device applies but never persists them.
 * - at most one discrete command from the queue (stop, center, ...)
 *
 * The main loop consumes these via getTarget()/getLimitsUpdate()/
 * getCommand() and pushes the current PlatformStatus to the cloud
 * periodically.
 */
class CloudClient {
public:
    CloudClient();

    void begin();

    /** Main update loop - pushes state periodically. */
    void handle();

    bool isEnabled() const { return enabled_; }

    /** Enable or disable cloud sync (starts/stops the background task). */
    void setEnabled(bool enabled);

    void getConfig(String& apiEndpoint, String& apiKey, bool& enabled) const;
    bool setConfig(const String& apiEndpoint, const String& apiKey, bool enabled);
    bool testConnection();

    /** Current platform status to push to the cloud. */
    void setStatus(const PlatformStatus& status);

    /** Fetch the latest fresh joystick target. Returns true once per update. */
    bool getTarget(float& x, float& y);

    /** Fetch changed cloud axis limits. Returns true once per change. */
    bool getLimitsUpdate(float& rotationLimitDeg, float& tiltLimitDeg);

    bool hasCommand() const { return hasPendingCommand_; }

    /** Get pending discrete command JSON (clears the command). */
    String getCommand();

private:
    // Configuration
    Preferences preferences_;
    String apiEndpoint_;
    String apiKey_;
    bool enabled_;

    // State push
    PlatformStatus currentStatus_;
    unsigned long lastStatePush_;

    // Data received from sync (guarded by syncMutex_)
    SemaphoreHandle_t syncMutex_;
    bool hasTarget_;
    float targetX_;
    float targetY_;
    bool hasLimitsUpdate_;
    float cloudRotationLimitDeg_;
    float cloudTiltLimitDeg_;
    bool cloudLimitsSeen_;
    float lastSeenRotationLimitDeg_;
    float lastSeenTiltLimitDeg_;
    volatile bool hasPendingCommand_;
    String pendingCommand_;
    volatile bool targetActive_;  // recent joystick activity -> poll faster

    // HTTP clients
    HTTPClient http_;      // main loop (state push, connection test)
    HTTPClient syncHttp_;  // background sync task

    // FreeRTOS task for background sync polling
    TaskHandle_t syncTaskHandle_;
    volatile bool syncTaskRunning_;

    // Timing constants (in milliseconds)
    static constexpr unsigned long STATE_PUSH_INTERVAL = 2000;
    static constexpr unsigned long SYNC_INTERVAL_ACTIVE_MS = 300;
    static constexpr unsigned long SYNC_INTERVAL_IDLE_MS = 1500;
    // Joystick targets older than this are ignored (frontend gone)
    static constexpr float TARGET_MAX_AGE_S = 2.0f;

    void loadConfig();
    bool saveConfig();
    void pushState();
    void syncOnce();
    void startSyncTask();
    void stopSyncTask();
    static void syncTaskFunction(void* parameter);
    int sendRequest(const String& endpoint, const String& method, const String& payload = "");
};

#endif // CLOUD_CLIENT_H
