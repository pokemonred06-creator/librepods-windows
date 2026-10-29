#include <QSettings>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QBluetoothLocalDevice>
#include <QBluetoothSocket>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>
#include <QProcess>
#include <QRegularExpression>

#include "airpods_packets.h"
#include "logger.h"
#include "media/mediacontroller.h"
#include "trayiconmanager.h"
#include "enums.h"
#include "battery.hpp"
#include "BluetoothMonitor.h"
#include "autostartmanager.hpp"
#include "deviceinfo.hpp"
#include "ble/blemanager.h"
#include "ble/bleutils.h"
#include "QRCodeImageProvider.hpp"

#ifdef Q_OS_LINUX
#include "systemsleepmonitor.hpp"
#elif defined(Q_OS_WIN)
#include "windowssleepmonitor.hpp"
#include <windows.h>
#include <cfgmgr32.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

static const GUID GUID_DEVINTERFACE_MAGICAAP =
    { 0x9eec98bb, 0x3c54, 0x45d4, { 0xa8, 0x43, 0x79, 0x00, 0xc4, 0x63, 0x5e, 0x08 } };

static const GUID GUID_DEVINTERFACE_AAP_SERVER =
    { 0x74ec2172, 0x0bad, 0x4d01, { 0x8f, 0x77, 0x99, 0x7b, 0x2b, 0xe0, 0x72, 0x2a } };

static std::vector<std::wstring> GetMagicAapInterfaces(const GUID &interfaceGuid) {
    std::vector<std::wstring> paths;
    ULONG length = 0;
    CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&length, const_cast<LPGUID>(&interfaceGuid), NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS || length <= 1) return paths;

    std::vector<wchar_t> buffer(length);
    cr = CM_Get_Device_Interface_ListW(const_cast<LPGUID>(&interfaceGuid), NULL, buffer.data(), length, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (cr != CR_SUCCESS) return paths;

    wchar_t* p = buffer.data();
    while (*p) {
        paths.push_back(p);
        p += wcslen(p) + 1;
    }
    return paths;
}
typedef HANDLE HBLUETOOTH_DEVICE_FIND;
typedef struct _BLUETOOTH_DEVICE_SEARCH_PARAMS {
    DWORD dwSize;
    BOOL fReturnAuthenticated;
    BOOL fReturnRemembered;
    BOOL fReturnUnknown;
    BOOL fReturnConnected;
    BOOL fIssueInquiry;
    UCHAR cTimeoutMultiplier;
    HANDLE hRadio;
} BLUETOOTH_DEVICE_SEARCH_PARAMS;

typedef struct _BLUETOOTH_DEVICE_INFO {
    DWORD dwSize;
    ULONG64 Address;
    ULONG ulClassofDevice;
    BOOL fConnected;
    BOOL fRemembered;
    BOOL fAuthenticated;
    SYSTEMTIME stLastSeen;
    SYSTEMTIME stLastUsed;
    WCHAR szName[248];
} BLUETOOTH_DEVICE_INFO;

typedef HBLUETOOTH_DEVICE_FIND (WINAPI *pfnBluetoothFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS*, BLUETOOTH_DEVICE_INFO*);
typedef BOOL (WINAPI *pfnBluetoothFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO*);
typedef BOOL (WINAPI *pfnBluetoothFindDeviceClose)(HBLUETOOTH_DEVICE_FIND);

static bool IsAirPodsConnectedWin32() {
    HMODULE hBth = LoadLibraryW(L"bthprops.cpl");
    if (!hBth) return false;

    pfnBluetoothFindFirstDevice pFindFirst = (pfnBluetoothFindFirstDevice)GetProcAddress(hBth, "BluetoothFindFirstDevice");
    pfnBluetoothFindNextDevice pFindNext = (pfnBluetoothFindNextDevice)GetProcAddress(hBth, "BluetoothFindNextDevice");
    pfnBluetoothFindDeviceClose pFindClose = (pfnBluetoothFindDeviceClose)GetProcAddress(hBth, "BluetoothFindDeviceClose");

    if (!pFindFirst || !pFindNext || !pFindClose) {
        FreeLibrary(hBth);
        return false;
    }

    BLUETOOTH_DEVICE_SEARCH_PARAMS sp = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
    sp.fReturnAuthenticated = TRUE;
    sp.fReturnRemembered = TRUE;
    sp.fReturnConnected = TRUE;
    sp.fIssueInquiry = FALSE;
    sp.hRadio = NULL;

    BLUETOOTH_DEVICE_INFO di = { sizeof(BLUETOOTH_DEVICE_INFO) };
    HBLUETOOTH_DEVICE_FIND hFind = pFindFirst(&sp, &di);
    bool isConnected = false;

    if (hFind) {
        do {
            if (di.fConnected) {
                QString name = QString::fromWCharArray(di.szName);
                if (name.contains("AirPods", Qt::CaseInsensitive)) {
                    isConnected = true;
                    break;
                }
            }
        } while (pFindNext(hFind, &di));
        pFindClose(hFind);
    }

    FreeLibrary(hBth);
    return isConnected;
}
#endif

using namespace AirpodsTrayApp::Enums;

Q_LOGGING_CATEGORY(librepods, "librepods")

class AirPodsTrayApp : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool airpodsConnected READ areAirpodsConnected NOTIFY airPodsStatusChanged)
    Q_PROPERTY(int earDetectionBehavior READ earDetectionBehavior WRITE setEarDetectionBehavior NOTIFY earDetectionBehaviorChanged)
    Q_PROPERTY(bool crossDeviceEnabled READ crossDeviceEnabled WRITE setCrossDeviceEnabled NOTIFY crossDeviceEnabledChanged)
    Q_PROPERTY(AutoStartManager *autoStartManager READ autoStartManager CONSTANT)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(int retryAttempts READ retryAttempts WRITE setRetryAttempts NOTIFY retryAttemptsChanged)
    Q_PROPERTY(bool hideOnStart READ hideOnStart CONSTANT)
    Q_PROPERTY(DeviceInfo *deviceInfo READ deviceInfo CONSTANT)
    Q_PROPERTY(QString phoneMacStatus READ phoneMacStatus NOTIFY phoneMacStatusChanged)
    Q_PROPERTY(bool hearingAidEnabled READ hearingAidEnabled WRITE setHearingAidEnabled NOTIFY hearingAidEnabledChanged)

public:
    AirPodsTrayApp(bool debugMode, bool hideOnStart, QQmlApplicationEngine *parent = nullptr)
        : QObject(parent), debugMode(debugMode), m_settings(new QSettings("AirPodsTrayApp", "AirPodsTrayApp"))
        , m_autoStartManager(new AutoStartManager(this)), m_hideOnStart(hideOnStart), parent(parent)
        , m_deviceInfo(new DeviceInfo(this)), m_bleManager(new BleManager(this))
#ifdef Q_OS_LINUX
        , m_systemSleepMonitor(new SystemSleepMonitor(this))
#elif defined(Q_OS_WIN)
        , m_systemSleepMonitor(new WindowsSleepMonitor(this))
#endif
    {
        QLoggingCategory::setFilterRules(QString("librepods.debug=%1").arg(debugMode ? "true" : "false"));
        LOG_INFO("Initializing LibrePods");

        // Initialize tray icon and connect signals
        trayManager = new TrayIconManager(this);
        trayManager->setNotificationsEnabled(loadNotificationsEnabled());
        connect(trayManager, &TrayIconManager::trayClicked, this, &AirPodsTrayApp::onTrayIconActivated);
        connect(trayManager, &TrayIconManager::openApp, this, &AirPodsTrayApp::onOpenApp);
        connect(trayManager, &TrayIconManager::openSettings, this, &AirPodsTrayApp::onOpenSettings);
        connect(trayManager, &TrayIconManager::noiseControlChanged, this, &AirPodsTrayApp::setNoiseControlMode);
        connect(trayManager, &TrayIconManager::conversationalAwarenessToggled, this, &AirPodsTrayApp::setConversationalAwareness);
        connect(m_deviceInfo, &DeviceInfo::batteryStatusChanged, trayManager, &TrayIconManager::updateBatteryStatus);
        connect(m_deviceInfo, &DeviceInfo::noiseControlModeChanged, trayManager, &TrayIconManager::updateNoiseControlState);
        connect(m_deviceInfo, &DeviceInfo::conversationalAwarenessChanged, trayManager, &TrayIconManager::updateConversationalAwareness);
        connect(trayManager, &TrayIconManager::notificationsEnabledChanged, this, &AirPodsTrayApp::saveNotificationsEnabled);
        connect(trayManager, &TrayIconManager::notificationsEnabledChanged, this, &AirPodsTrayApp::notificationsEnabledChanged);

        // Load saved preferences into deviceInfo
        m_deviceInfo->setNoiseControlMode(loadSavedNoiseControlMode());
        trayManager->updateNoiseControlState(m_deviceInfo->noiseControlMode());

        m_deviceInfo->setConversationalAwareness(m_settings->value("device/conversationalAwareness", false).toBool());
        m_deviceInfo->setAdaptiveNoiseLevel(m_settings->value("device/adaptiveNoiseLevel", 50).toInt());
        m_deviceInfo->setHearingAidEnabled(m_settings->value("device/hearingAidEnabled", false).toBool());
        m_deviceInfo->setOneBudANCMode(m_settings->value("device/oneBudANCMode", false).toBool());
        m_deviceInfo->getSpatialTracker()->setEnabled(m_settings->value("spatial/enabled", false).toBool());

        connect(m_deviceInfo->getSpatialTracker(), &SpatialAudioTracker::enabledChanged, this, [this](bool enabled) {
            m_settings->setValue("spatial/enabled", enabled);
        });

        // Initialize MediaController and connect signals
        mediaController = new MediaController(this);
        connect(mediaController, &MediaController::mediaStateChanged, this, &AirPodsTrayApp::handleMediaStateChange);
        mediaController->followMediaChanges();

        monitor = new BluetoothMonitor(this);
        connect(monitor, &BluetoothMonitor::deviceConnected, this, &AirPodsTrayApp::bluezDeviceConnected);
        connect(monitor, &BluetoothMonitor::deviceDisconnected, this, &AirPodsTrayApp::bluezDeviceDisconnected);

        connect(m_bleManager, &BleManager::deviceFound, this, &AirPodsTrayApp::bleDeviceFound);
        connect(m_deviceInfo->getBattery(), &Battery::primaryChanged, this, &AirPodsTrayApp::primaryChanged);
        connect(m_deviceInfo->getSpatialTracker(), &SpatialAudioTracker::sendPacketRequested, this, [this](const QByteArray &packet) {
            if (!areAirpodsConnected()) {
                return;
            }
            writePacketToSocket(packet, "Spatial Audio packet written: ");
        });
        
#ifdef Q_OS_LINUX
        connect(m_systemSleepMonitor, &SystemSleepMonitor::systemGoingToSleep, this, &AirPodsTrayApp::onSystemGoingToSleep);
        connect(m_systemSleepMonitor, &SystemSleepMonitor::systemWakingUp, this, &AirPodsTrayApp::onSystemWakingUp);
#elif defined(Q_OS_WIN)
        if (m_systemSleepMonitor->initialize())
        {
            connect(m_systemSleepMonitor, &WindowsSleepMonitor::systemGoingToSleep, this, &AirPodsTrayApp::onSystemGoingToSleep);
            connect(m_systemSleepMonitor, &WindowsSleepMonitor::systemWakingUp, this, &AirPodsTrayApp::onSystemWakingUp);
        }
        else
        {
            LOG_ERROR("Failed to initialize Windows Sleep Monitor");
        }
#endif

        // Load settings
        CrossDevice.isEnabled = loadCrossDeviceEnabled();
        setEarDetectionBehavior(loadEarDetectionSettings());
        setRetryAttempts(loadRetryAttempts());

        monitor->checkAlreadyConnectedDevices();
        LOG_INFO("AirPodsTrayApp initialized");

#ifndef Q_OS_WIN
        QBluetoothLocalDevice localDevice;

        const QList<QBluetoothAddress> connectedDevices = localDevice.connectedDevices();
        for (const QBluetoothAddress &address : connectedDevices) {
            QBluetoothDeviceInfo device(address, "", 0);
            if (isAirPodsDevice(device)) {
                connectToDevice(device);

                // On startup after reboot, activate A2DP profile for already connected AirPods
                QTimer::singleShot(2000, this, [this, address]()
                {
                    QString formattedAddress = address.toString().replace(":", "_");
                    mediaController->setConnectedDeviceMacAddress(formattedAddress);
                    mediaController->activateA2dpProfile();
                    LOG_INFO("A2DP profile activation attempted for AirPods found on startup");
                });
                return;
            }
        }
#endif

        initializeDBus();
        initializeBluetooth();

#ifdef Q_OS_WIN
        m_magicAapPollTimer = new QTimer(this);
        connect(m_magicAapPollTimer, &QTimer::timeout, this, &AirPodsTrayApp::checkBluetoothConnection);
        m_magicAapPollTimer->start(2000);
        checkBluetoothConnection();
#endif
    }

    ~AirPodsTrayApp() {
        saveCrossDeviceEnabled();
        saveEarDetectionSettings();

#ifdef Q_OS_WIN
        if (m_magicAapPollTimer) {
            m_magicAapPollTimer->stop();
        }
        closeMagicAap();
#endif

        if (socket) {
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }
        if (phoneSocket) {
            phoneSocket->close();
            phoneSocket->deleteLater();
            phoneSocket = nullptr;
        }
    }

    bool areAirpodsConnected() const {
#ifdef Q_OS_WIN
        if (m_hMagicAap != INVALID_HANDLE_VALUE) return true;
        if (m_isAirPodsConnectedFallback) return true;
#endif
        return socket && socket->isOpen() && socket->state() == QBluetoothSocket::SocketState::ConnectedState;
    }
    int earDetectionBehavior() const { return mediaController->getEarDetectionBehavior(); }
    bool crossDeviceEnabled() const { return CrossDevice.isEnabled; }
    AutoStartManager *autoStartManager() const { return m_autoStartManager; }
    bool notificationsEnabled() const { return trayManager->notificationsEnabled(); }
    void setNotificationsEnabled(bool enabled) { trayManager->setNotificationsEnabled(enabled); }
    int retryAttempts() const { return m_retryAttempts; }
    bool hideOnStart() const { return m_hideOnStart; }
    DeviceInfo *deviceInfo() const { return m_deviceInfo; }
    QString phoneMacStatus() const { return m_phoneMacStatus; }
    bool hearingAidEnabled() const { return m_deviceInfo->hearingAidEnabled(); }

private:
    bool debugMode;
    bool isConnectedLocally = false;

    QQmlApplicationEngine *parent = nullptr;

    struct {
        bool isAvailable = true;
        bool isEnabled = true; // Ability to disable the feature
    } CrossDevice;

    void initializeDBus() { }

    bool isAirPodsDevice(const QBluetoothDeviceInfo &device)
    {
        return device.serviceUuids().contains(QBluetoothUuid("74ec2172-0bad-4d01-8f77-997b2be0722a"));
    }

    void notifyAndroidDevice()
    {
        if (!CrossDevice.isEnabled) {
            return;
        }

        if (phoneSocket && phoneSocket->isOpen())
        {
            phoneSocket->write(AirPodsPackets::Phone::NOTIFICATION);
            LOG_DEBUG("Sent notification packet to Android: " << AirPodsPackets::Phone::NOTIFICATION.toHex());
        }
        else
        {
            LOG_WARN("Phone socket is not open, cannot send notification packet");
        }
    }

    void disconnectDevice(const QString &devicePath) {
        LOG_INFO("Disconnecting device at " << devicePath);
    }

public slots:
    void connectToDevice(const QString &address) {
        LOG_INFO("Connecting to device with address: " << address);
        QBluetoothAddress btAddress(address);
        QBluetoothDeviceInfo device(btAddress, "", 0);
        connectToDevice(device);
    }

    void setNoiseControlMode(NoiseControlMode mode)
    {
        LOG_INFO("Setting noise control mode to: " << mode);
        saveNoiseControlMode(mode);
        QByteArray packet = AirPodsPackets::NoiseControl::getPacketForMode(mode);
        if (writePacketToSocket(packet, "Noise control mode packet written: "))
        {
            m_deviceInfo->setNoiseControlMode(mode);
        }
    }
    void setNoiseControlModeInt(int mode)
    {
        if (mode < 0 || mode > static_cast<int>(NoiseControlMode::Adaptive))
        {
            LOG_ERROR("Invalid noise control mode: " << mode);
            return;
        }
        setNoiseControlMode(static_cast<NoiseControlMode>(mode));
    }

    void setConversationalAwareness(bool enabled)
    {
        LOG_INFO("Setting conversational awareness to: " << (enabled ? "enabled" : "disabled"));
        m_settings->setValue("device/conversationalAwareness", enabled);
        QByteArray packet = enabled ? AirPodsPackets::ConversationalAwareness::ENABLED
                                    : AirPodsPackets::ConversationalAwareness::DISABLED;

        writePacketToSocket(packet, "Conversational awareness packet written: ");
        m_deviceInfo->setConversationalAwareness(enabled);
    }

    void setOneBudANCMode(bool enabled)
    {
        LOG_INFO("Setting One Bud ANC mode to: " << (enabled ? "enabled" : "disabled"));
        m_settings->setValue("device/oneBudANCMode", enabled);
        QByteArray packet = enabled ? AirPodsPackets::OneBudANCMode::ENABLED
                                    : AirPodsPackets::OneBudANCMode::DISABLED;

        if (writePacketToSocket(packet, "One Bud ANC mode packet written: "))
        {
            m_deviceInfo->setOneBudANCMode(enabled);
        }
        else
        {
            LOG_ERROR("Failed to send One Bud ANC mode command: socket not open");
            m_deviceInfo->setOneBudANCMode(enabled);
        }
    }

    void setRetryAttempts(int attempts)
    {
        if (m_retryAttempts != attempts)
        {
            LOG_DEBUG("Setting retry attempts to: " << attempts);
            m_retryAttempts = attempts;
            emit retryAttemptsChanged(attempts);
            saveRetryAttempts(attempts);
        }
    }

    void initiateMagicPairing()
    {
        if (!areAirpodsConnected())
        {
            LOG_ERROR("AirPods not connected, Magic Pairing cannot be started");
            return;
        }

        writePacketToSocket(AirPodsPackets::MagicPairing::REQUEST_MAGIC_CLOUD_KEYS, "Magic Pairing packet written: ");
    }

    void setAdaptiveNoiseLevel(int level)
    {
        level = qBound(0, level, 100);
        LOG_INFO("Setting adaptive noise level to: " << level);
        m_settings->setValue("device/adaptiveNoiseLevel", level);
        QByteArray packet = AirPodsPackets::AdaptiveNoise::getPacket(level);
        writePacketToSocket(packet, "Adaptive noise level packet written: ");
        m_deviceInfo->setAdaptiveNoiseLevel(level);
    }

    void renameAirPods(const QString &newName)
    {
        if (newName.isEmpty())
        {
            LOG_WARN("Cannot set empty name");
            return;
        }
        if (newName.size() > 32)
        {
            LOG_WARN("Name is too long, must be 32 characters or less");
            return;
        }
        if (newName == m_deviceInfo->deviceName())
        {
            LOG_INFO("Name is already set to: " << newName);
            return;
        }

        QByteArray packet = AirPodsPackets::Rename::getPacket(newName);
        if (writePacketToSocket(packet, "Rename packet written: "))
        {
            LOG_INFO("Sent rename command for new name: " << newName);
            m_deviceInfo->setDeviceName(newName);
        }
        else
        {
            LOG_ERROR("Failed to send rename command: socket not open");
        }
    }

    void setEarDetectionBehavior(int behavior)
    {
        if (behavior == earDetectionBehavior())
        {
            LOG_INFO("Ear detection behavior is already set to: " << behavior);
            return;
        }

        mediaController->setEarDetectionBehavior(static_cast<MediaController::EarDetectionBehavior>(behavior));
        saveEarDetectionSettings();
        emit earDetectionBehaviorChanged(behavior);
    }

    void setCrossDeviceEnabled(bool enabled)
    {
        if (CrossDevice.isEnabled == enabled)
        {
            LOG_INFO("Cross-device feature is already " << (enabled ? "enabled" : "disabled"));
            return;
        }

        CrossDevice.isEnabled = enabled;
        saveCrossDeviceEnabled();
        connectToPhone();
        emit crossDeviceEnabledChanged(enabled);
    }

    void setPhoneMac(const QString &mac)
    {
        if (mac.isEmpty()) {
            LOG_WARN("Empty MAC provided, ignoring");
            m_phoneMacStatus = QStringLiteral("No MAC provided (ignoring)");
            emit phoneMacStatusChanged();
            return;
        }

        // Basic MAC address validation (accepts formats like AA:BB:CC:DD:EE:FF, AABBCCDDEEFF, AA-BB-CC-DD-EE-FF)
        QRegularExpression re("^([0-9A-Fa-f]{2}([-:]?)){5}[0-9A-Fa-f]{2}$");
        if (!re.match(mac).hasMatch()) {
            LOG_ERROR("Invalid MAC address format: " << mac);
            m_phoneMacStatus = QStringLiteral("Invalid MAC: ") + mac;
            emit phoneMacStatusChanged();
            return;
        }

        // Set environment variable for the running process
        qputenv("PHONE_MAC_ADDRESS", mac.toUtf8());
        LOG_INFO("PHONE_MAC_ADDRESS environment variable set to: " << mac);

        m_phoneMacStatus = QStringLiteral("Updated MAC: ") + mac;
        emit phoneMacStatusChanged();

        // Update QML context property so UI placeholders reflect the new value
        if (parent) {
            parent->rootContext()->setContextProperty("PHONE_MAC_ADDRESS", mac);
        }

        // If a phone socket exists, restart connection using the new MAC
        if (phoneSocket && phoneSocket->isOpen()) {
            phoneSocket->close();
            phoneSocket->deleteLater();
            phoneSocket = nullptr;
        }
        connectToPhone();
    }

    void updatePhoneMacStatus(const QString &status)
    {
        m_phoneMacStatus = status;
        emit phoneMacStatusChanged();
    }

    void setHearingAidEnabled(bool enabled)
    {
        LOG_INFO("Setting hearing aid to: " << (enabled ? "enabled" : "disabled"));
        m_settings->setValue("device/hearingAidEnabled", enabled);
        QByteArray packet = enabled ? AirPodsPackets::HearingAid::ENABLED
                                    : AirPodsPackets::HearingAid::DISABLED;

        writePacketToSocket(packet, "Hearing aid packet written: ");
        m_deviceInfo->setHearingAidEnabled(enabled);
    }

    bool writePacketToSocket(const QByteArray &packet, const QString &logMessage)
    {
#ifdef Q_OS_WIN
        if (ensureMagicAapOpen())
        {
            OVERLAPPED ov = {0};
            ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            DWORD written = 0;
            BOOL ok = WriteFile(m_hMagicAap, packet.constData(), static_cast<DWORD>(packet.size()), &written, &ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING)
            {
                if (WaitForSingleObject(ov.hEvent, 300) == WAIT_OBJECT_0)
                {
                    GetOverlappedResult(m_hMagicAap, &ov, &written, FALSE);
                }
                else
                {
                    LOG_ERROR("MagicAAP WriteFile timed out after 300ms");
                    CancelIoEx(m_hMagicAap, &ov);
                }
            }
            CloseHandle(ov.hEvent);

            if (written == static_cast<DWORD>(packet.size()))
            {
                LOG_INFO("MagicAAP write: " << logMessage << packet.toHex());
                return true;
            }
            else
            {
                LOG_ERROR("MagicAAP WriteFile failed: " << GetLastError());
                closeMagicAap();
            }
        }
#endif

        if (socket && socket->isOpen())
        {
            socket->write(packet);
            LOG_DEBUG(logMessage << packet.toHex());
            return true;
        }
        else
        {
            LOG_ERROR("Socket/MagicAAP is not open, cannot write packet");
            return false;
        }
    }

    bool loadCrossDeviceEnabled() { return m_settings->value("crossdevice/enabled", false).toBool(); }
    void saveCrossDeviceEnabled() { m_settings->setValue("crossdevice/enabled", CrossDevice.isEnabled); }

    int loadEarDetectionSettings() { return m_settings->value("earDetection/setting", MediaController::EarDetectionBehavior::PauseWhenOneRemoved).toInt(); }
    void saveEarDetectionSettings() { m_settings->setValue("earDetection/setting", mediaController->getEarDetectionBehavior()); }

    bool loadNotificationsEnabled() const { return m_settings->value("notifications/enabled", true).toBool(); }
    void saveNotificationsEnabled(bool enabled) { m_settings->setValue("notifications/enabled", enabled); }

    int loadRetryAttempts() const { return m_settings->value("bluetooth/retryAttempts", 3).toInt(); }
    void saveRetryAttempts(int attempts) { m_settings->setValue("bluetooth/retryAttempts", attempts); }

    NoiseControlMode loadSavedNoiseControlMode() const
    {
        int mode = m_settings->value("device/noiseControlMode", static_cast<int>(NoiseControlMode::NoiseCancellation)).toInt();
        if (mode < static_cast<int>(NoiseControlMode::MinValue) || mode > static_cast<int>(NoiseControlMode::MaxValue))
        {
            mode = static_cast<int>(NoiseControlMode::NoiseCancellation);
        }
        return static_cast<NoiseControlMode>(mode);
    }
    void saveNoiseControlMode(NoiseControlMode mode)
    {
        m_settings->setValue("device/noiseControlMode", static_cast<int>(mode));
    }

    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height)
    {
        if (width >= 200 && height >= 200)
        {
            m_settings->setValue("window/x", x);
            m_settings->setValue("window/y", y);
            m_settings->setValue("window/width", width);
            m_settings->setValue("window/height", height);
        }
    }

    Q_INVOKABLE int windowWidth() const { return m_settings->value("window/width", 420).toInt(); }
    Q_INVOKABLE int windowHeight() const { return m_settings->value("window/height", 580).toInt(); }
    Q_INVOKABLE int windowX() const { return m_settings->value("window/x", -1).toInt(); }
    Q_INVOKABLE int windowY() const { return m_settings->value("window/y", -1).toInt(); }

    void restoreSavedPreferences()
    {
        NoiseControlMode savedMode = loadSavedNoiseControlMode();
        LOG_INFO("Restoring saved noise control mode: " << savedMode);
        QByteArray noisePacket = AirPodsPackets::NoiseControl::getPacketForMode(savedMode);
        writePacketToSocket(noisePacket, "Restoring Noise control mode packet: ");
        m_deviceInfo->setNoiseControlMode(savedMode);

        bool savedCA = m_settings->value("device/conversationalAwareness", false).toBool();
        LOG_INFO("Restoring saved conversational awareness: " << (savedCA ? "enabled" : "disabled"));
        QByteArray caPacket = savedCA ? AirPodsPackets::ConversationalAwareness::ENABLED
                                      : AirPodsPackets::ConversationalAwareness::DISABLED;
        writePacketToSocket(caPacket, "Restoring Conversational awareness packet: ");
        m_deviceInfo->setConversationalAwareness(savedCA);

        int savedNoiseLevel = m_settings->value("device/adaptiveNoiseLevel", 50).toInt();
        LOG_INFO("Restoring saved adaptive noise level: " << savedNoiseLevel);
        QByteArray adaptivePacket = AirPodsPackets::AdaptiveNoise::getPacket(savedNoiseLevel);
        writePacketToSocket(adaptivePacket, "Restoring Adaptive noise level packet: ");
        m_deviceInfo->setAdaptiveNoiseLevel(savedNoiseLevel);

        bool savedHearingAid = m_settings->value("device/hearingAidEnabled", false).toBool();
        LOG_INFO("Restoring saved hearing aid: " << (savedHearingAid ? "enabled" : "disabled"));
        QByteArray haPacket = savedHearingAid ? AirPodsPackets::HearingAid::ENABLED
                                              : AirPodsPackets::HearingAid::DISABLED;
        writePacketToSocket(haPacket, "Restoring Hearing aid packet: ");
        m_deviceInfo->setHearingAidEnabled(savedHearingAid);

        bool savedOneBud = m_settings->value("device/oneBudANCMode", false).toBool();
        LOG_INFO("Restoring saved One Bud ANC mode: " << (savedOneBud ? "enabled" : "disabled"));
        QByteArray oneBudPacket = savedOneBud ? AirPodsPackets::OneBudANCMode::ENABLED
                                              : AirPodsPackets::OneBudANCMode::DISABLED;
        writePacketToSocket(oneBudPacket, "Restoring One Bud ANC mode packet: ");
        m_deviceInfo->setOneBudANCMode(savedOneBud);

        bool savedSpatial = m_settings->value("spatial/enabled", false).toBool();
        if (savedSpatial) {
            LOG_INFO("Restoring saved spatial tracking: enabled");
            m_deviceInfo->getSpatialTracker()->setEnabled(true);
        }
    }

    void onSystemGoingToSleep()
    {
        if (m_bleManager->isScanning())
        {
            LOG_INFO("Stopping BLE scan before going to sleep");
            m_bleManager->stopScan();
        }
    }
    void onSystemWakingUp()
    {
        LOG_INFO("System is waking up, starting ble scan");
        m_bleManager->startScan();

        // Check if AirPods are already connected and activate A2DP profile
        if (areAirpodsConnected() && m_deviceInfo && !m_deviceInfo->bluetoothAddress().isEmpty())
        {
            LOG_INFO("AirPods already connected after wake-up, re-activating A2DP profile");
            mediaController->setConnectedDeviceMacAddress(m_deviceInfo->bluetoothAddress().replace(":", "_"));

            // Always activate A2DP profile after system wake since the profile might have been lost
            QTimer::singleShot(1000, this, [this]()
            {
                mediaController->activateA2dpProfile();
                LOG_INFO("A2DP profile activation attempted after system wake-up");
            });
        }

        // Also check for already connected devices via BlueZ
        monitor->checkAlreadyConnectedDevices();
    }

private slots:
    void onTrayIconActivated()
    {
        if (QGuiApplication::topLevelWindows().isEmpty())
        {
            loadMainModule();
            return;
        }
        QQuickWindow *window = qobject_cast<QQuickWindow *>(
            QGuiApplication::topLevelWindows().constFirst());
        if (window)
        {
            window->show();
            window->raise();
            window->requestActivate();
        }
    }

    void onOpenApp()
    {
        reopenOrLoadModule("app");
    }

    void onOpenSettings()
    {
        reopenOrLoadModule("settings");
    }

    void sendHandshake() {
        LOG_INFO("Connected to device, sending initial packets");
        writePacketToSocket(AirPodsPackets::Connection::HANDSHAKE, "Handshake packet written: ");
    }

    void bluezDeviceConnected(const QString &address, const QString &name)
    {
        QBluetoothDeviceInfo device(QBluetoothAddress(address), name, 0);
        connectToDevice(device);

        // After system reboot, AirPods might be connected but A2DP profile not active
        // Attempt to activate A2DP profile after a delay to ensure connection is established
        QTimer::singleShot(2000, this, [this, address]()
        {
            if (!address.isEmpty())
            {
                QString formattedAddress = address;
                formattedAddress = formattedAddress.replace(":", "_");
                mediaController->setConnectedDeviceMacAddress(formattedAddress);
                mediaController->activateA2dpProfile();
                LOG_INFO("A2DP profile activation attempted for newly connected device");
            }
        });
    }

    void onDeviceDisconnected(const QBluetoothAddress &address)
    {
        LOG_INFO("Device disconnected: " << address.toString());
        if (socket)
        {
            LOG_WARN("Socket is still open, closing it");
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }
        if (phoneSocket && phoneSocket->isOpen())
        {
            phoneSocket->write(AirPodsPackets::Connection::AIRPODS_DISCONNECTED);
            LOG_DEBUG("AIRPODS_DISCONNECTED packet written: " << AirPodsPackets::Connection::AIRPODS_DISCONNECTED.toHex());
        }

        // Clear the device name and model
        m_deviceInfo->reset();
        if (mediaController)
        {
            mediaController->cancelAutoResume();
        }
        if (mediaController)
        {
            if (mediaController->getEarDetectionBehavior() != MediaController::Disabled &&
                mediaController->getCurrentMediaState() == MediaController::Playing)
            {
                LOG_INFO("AirPods disconnected while media was playing; pausing playback");
                mediaController->pause();
            }
            mediaController->resetConversationalAwareness();
        }
        if (m_deviceInfo && m_deviceInfo->getSpatialTracker())
        {
            m_deviceInfo->getSpatialTracker()->setConnected(false);
        }
        m_bleManager->startScan();
        emit airPodsStatusChanged();

        // Show system notification
        trayManager->showNotification(
            tr("AirPods Disconnected"),
            tr("Your AirPods have been disconnected"));
        trayManager->resetTrayIcon();
    }

    void bluezDeviceDisconnected(const QString &address, const QString &name)
    {
        if (address == m_deviceInfo->bluetoothAddress())
        {
            onDeviceDisconnected(QBluetoothAddress(address));
        } else {
            LOG_WARN("Disconnected device does not match connected device: " << address << " != " << m_deviceInfo->bluetoothAddress());
        }
    }

    void parseMetadata(const QByteArray &data)
    {
        // Verify the data starts with the METADATA header
        if (!data.startsWith(AirPodsPackets::Parse::METADATA))
        {
            LOG_ERROR("Invalid metadata packet: Incorrect header");
            return;
        }

        int pos = AirPodsPackets::Parse::METADATA.size(); // Start after the header

        // Check if there is enough data to skip the initial bytes (based on example structure)
        if (data.size() < pos + 6)
        {
            LOG_ERROR("Metadata packet too short to parse initial bytes");
            return;
        }
        pos += 6; // Skip 6 bytes after the header as per example structure

        auto extractString = [&data, &pos]() -> QString
        {
            if (pos >= data.size())
            {
                return QString();
            }
            int start = pos;
            while (pos < data.size() && data.at(pos) != '\0')
            {
                ++pos;
            }
            QString str = QString::fromUtf8(data.mid(start, pos - start));
            if (pos < data.size())
            {
                ++pos; // Move past the null terminator
            }
            return str;
        };

        m_deviceInfo->setDeviceName(extractString());
        m_deviceInfo->setModelNumber(extractString());
        m_deviceInfo->setManufacturer(extractString());

        m_deviceInfo->setModel(parseModelNumber(m_deviceInfo->modelNumber()));
        emit modelChanged();

        // Log extracted metadata
        LOG_INFO("Parsed AirPods metadata:");
        LOG_INFO("Device Name: " << m_deviceInfo->deviceName());
        LOG_INFO("Model Number: " << m_deviceInfo->modelNumber());
        LOG_INFO("Manufacturer: " << m_deviceInfo->manufacturer());
    }

    QString getEarStatus(char value)
    {
        return (value == 0x00) ? "In Ear" : (value == 0x01) ? "Out of Ear"
                                                            : "In case";
    }

    void connectToDevice(const QBluetoothDeviceInfo &device)
    {
        if (socket && socket->isOpen() && socket->peerAddress() == device.address())
        {
            LOG_INFO("Already connected to the device: " << device.name());
            return;
        }

        LOG_INFO("Connecting to device: " << device.name());

        // Clean up any existing socket
        if (socket)
        {
            socket->close();
            socket->deleteLater();
            socket = nullptr;
        }

        QBluetoothSocket *localSocket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol);
        socket = localSocket;

        // Connection handler
        auto handleConnection = [this, localSocket]()
        {
            connect(localSocket, &QBluetoothSocket::readyRead, this, [this, localSocket]()
                    {
            QByteArray data = localSocket->readAll();
            QMetaObject::invokeMethod(this, "parseData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
            QMetaObject::invokeMethod(this, "relayPacketToPhone", Qt::QueuedConnection, Q_ARG(QByteArray, data)); });
            sendHandshake();
            m_initialSyncInProgress = true;
            QTimer::singleShot(1500, this, [this]() {
                m_initialSyncInProgress = false;
            });
        };

        // Error handler with retry
        auto handleError = [this, device, localSocket](QBluetoothSocket::SocketError error)
        {
            LOG_ERROR("Socket error: " << error << ", " << localSocket->errorString());

            static int retryCount = 0;
            if (retryCount < m_retryAttempts)
            {
                retryCount++;
                LOG_INFO("Retrying connection (attempt " << retryCount << ")");
                QTimer::singleShot(1500, this, [this, device]()
                                   { connectToDevice(device); });
            }
            else
            {
                LOG_ERROR("Failed to connect after 3 attempts");
                retryCount = 0;
            }
        };

        connect(localSocket, &QBluetoothSocket::connected, this, handleConnection);
        connect(localSocket, QOverload<QBluetoothSocket::SocketError>::of(&QBluetoothSocket::errorOccurred),
                this, handleError);

        localSocket->connectToService(device.address(), QBluetoothUuid("74ec2172-0bad-4d01-8f77-997b2be0722a"));
        m_deviceInfo->setBluetoothAddress(device.address().toString());
        notifyAndroidDevice();
    }

    void parseData(const QByteArray &data)
    {
        LOG_DEBUG("Received: " << data.toHex());

        if (data.startsWith(AirPodsPackets::Parse::HANDSHAKE_ACK))
        {
            writePacketToSocket(AirPodsPackets::Connection::SET_SPECIFIC_FEATURES, "Set specific features packet written: ");
        }
        else if (data.startsWith(AirPodsPackets::Parse::FEATURES_ACK))
        {
            writePacketToSocket(AirPodsPackets::Connection::REQUEST_NOTIFICATIONS, "Request notifications packet written: ");

            QTimer::singleShot(600, this, [this]() {
                restoreSavedPreferences();
            });

            QTimer::singleShot(2000, this, [this]() {
                if (m_deviceInfo->batteryStatus().isEmpty()) {
                    writePacketToSocket(AirPodsPackets::Connection::REQUEST_NOTIFICATIONS, "Request notifications packet written: ");
                }
            });
        }
        // Magic Cloud Keys Response
        else if (data.startsWith(AirPodsPackets::MagicPairing::MAGIC_CLOUD_KEYS_HEADER))
        {
            auto keys = AirPodsPackets::MagicPairing::parseMagicCloudKeysPacket(data);
            LOG_INFO("Received Magic Cloud Keys:");
            LOG_INFO("MagicAccIRK: " << keys.magicAccIRK.toHex());
            LOG_INFO("MagicAccEncKey: " << keys.magicAccEncKey.toHex());

            // Store the keys
            m_deviceInfo->setMagicAccIRK(keys.magicAccIRK);
            m_deviceInfo->setMagicAccEncKey(keys.magicAccEncKey);
            m_deviceInfo->saveToSettings(*m_settings);
        }
        // Get CA state
        else if (data.startsWith(AirPodsPackets::ConversationalAwareness::HEADER)) {
            if (auto result = AirPodsPackets::ConversationalAwareness::parseState(data))
            {
                bool reported = result.value();
                bool saved = m_settings->value("device/conversationalAwareness", false).toBool();
                LOG_INFO("Conversational awareness state received: " << reported << " (saved: " << saved << ")");
                if (reported != saved)
                {
                    LOG_INFO("AirPods CA state differs from saved preference, enforcing saved preference (" << saved << ")");
                    setConversationalAwareness(saved);
                }
                else
                {
                    m_deviceInfo->setConversationalAwareness(reported);
                }
            }
        }
        // Hearing Aid state
        else if (data.startsWith(AirPodsPackets::HearingAid::HEADER)) {
            if (auto result = AirPodsPackets::HearingAid::parseState(data))
            {
                bool reported = result.value();
                bool saved = m_settings->value("device/hearingAidEnabled", false).toBool();
                LOG_INFO("Hearing aid state received: " << reported << " (saved: " << saved << ")");
                if (reported != saved)
                {
                    LOG_INFO("AirPods Hearing Aid state differs from saved preference, enforcing saved preference (" << saved << ")");
                    setHearingAidEnabled(saved);
                }
                else
                {
                    m_deviceInfo->setHearingAidEnabled(reported);
                }
            }
        }
        // Noise Control Mode
        else if (data.size() == 11 && data.startsWith(AirPodsPackets::NoiseControl::HEADER))
        {
            if (auto value = AirPodsPackets::NoiseControl::parseMode(data))
            {
                NoiseControlMode reported = value.value();
                LOG_INFO("Noise control mode received: " << reported);
                if (m_initialSyncInProgress)
                {
                    NoiseControlMode saved = loadSavedNoiseControlMode();
                    if (reported != saved)
                    {
                        LOG_INFO("Initial sync: AirPods mode (" << reported << ") differs from saved preference (" << saved << "), enforcing saved preference");
                        setNoiseControlMode(saved);
                    }
                    else
                    {
                        m_deviceInfo->setNoiseControlMode(reported);
                    }
                }
                else
                {
                    m_deviceInfo->setNoiseControlMode(reported);
                    saveNoiseControlMode(reported);
                }
            }
        }
        // Ear Detection
        else if (data.size() >= 8 && data.startsWith(AirPodsPackets::Parse::EAR_DETECTION))
        {
            m_deviceInfo->getEarDetection()->parseData(data);
            mediaController->handleEarDetection(m_deviceInfo->getEarDetection());
        }
        // Battery Status
        else if (data.size() >= 7 && data.startsWith(AirPodsPackets::Parse::BATTERY_STATUS))
        {
            m_deviceInfo->getBattery()->parsePacket(data);
            m_deviceInfo->updateBatteryStatus();
            LOG_INFO("Battery status: " << m_deviceInfo->batteryStatus());
        }
        // Conversational Awareness Data
        else if (data.size() == 10 && data.startsWith(AirPodsPackets::ConversationalAwareness::DATA_HEADER))
        {
            LOG_INFO("Received conversational awareness data");
            mediaController->handleConversationalAwareness(data);
        }
        // Spatial Audio / Head Tracking Data
        else if (data.startsWith(AirPodsPackets::Parse::SPATIAL_AUDIO))
        {
            m_deviceInfo->getSpatialTracker()->parsePacket(data);
        }
        else if (data.startsWith(AirPodsPackets::Parse::METADATA))
        {
            parseMetadata(data);
            initiateMagicPairing();
            mediaController->setConnectedDeviceMacAddress(m_deviceInfo->bluetoothAddress().replace(":", "_"));
            if (m_deviceInfo->getEarDetection()->oneOrMorePodsInEar()) // AirPods get added as output device only after this
            {
                mediaController->activateA2dpProfile();
            }
            m_bleManager->stopScan();
            emit airPodsStatusChanged();
        }
        else if (data.startsWith(AirPodsPackets::OneBudANCMode::HEADER)) {
            if (auto value = AirPodsPackets::OneBudANCMode::parseState(data))
            {
                bool reported = value.value();
                bool saved = m_settings->value("device/oneBudANCMode", false).toBool();
                LOG_INFO("One Bud ANC mode received: " << reported << " (saved: " << saved << ")");
                if (reported != saved)
                {
                    LOG_INFO("AirPods One Bud ANC state differs from saved preference, enforcing saved preference (" << saved << ")");
                    setOneBudANCMode(saved);
                }
                else
                {
                    m_deviceInfo->setOneBudANCMode(reported);
                }
            }
        }
        else
        {
            LOG_DEBUG("Unrecognized packet format: " << data.toHex());
        }
    }

    void connectToPhone() {
        if (!CrossDevice.isEnabled) {
            return;
        }

        if (phoneSocket && phoneSocket->isOpen()) {
            LOG_INFO("Already connected to the phone");
            return;
        }
        QBluetoothAddress phoneAddress("00:00:00:00:00:00"); // Default address, will be overwritten if PHONE_MAC_ADDRESS is set
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        if (!env.value("PHONE_MAC_ADDRESS").isEmpty())
        {
            phoneAddress = QBluetoothAddress(env.value("PHONE_MAC_ADDRESS"));
        }
        phoneSocket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol);
        connect(phoneSocket, &QBluetoothSocket::connected, this, [this]() {
            LOG_INFO("Connected to phone");
            if (!lastBatteryStatus.isEmpty()) {
                phoneSocket->write(lastBatteryStatus);
                LOG_DEBUG("Sent last battery status to phone: " << lastBatteryStatus.toHex());
            }
            if (!lastEarDetectionStatus.isEmpty()) {
                phoneSocket->write(lastEarDetectionStatus);
                LOG_DEBUG("Sent last ear detection status to phone: " << lastEarDetectionStatus.toHex());
            }
        });

        connect(phoneSocket, QOverload<QBluetoothSocket::SocketError>::of(&QBluetoothSocket::errorOccurred), this, [this](QBluetoothSocket::SocketError error) {
            LOG_ERROR("Phone socket error: " << error << ", " << phoneSocket->errorString());
        });

        phoneSocket->connectToService(phoneAddress, QBluetoothUuid("1abbb9a4-10e4-4000-a75c-8953c5471342"));
    }

    void relayPacketToPhone(const QByteArray &packet)
    {
        if (!CrossDevice.isEnabled) {
            return;
        }
        if (phoneSocket && phoneSocket->isOpen())
        {
            phoneSocket->write(AirPodsPackets::Phone::NOTIFICATION + packet);
        }
        else
        {
            connectToPhone();
            LOG_WARN("Phone socket is not open, cannot relay packet");
        }
    }

    void handlePhonePacket(const QByteArray &packet) {
        if (packet.startsWith(AirPodsPackets::Phone::NOTIFICATION))
        {
            QByteArray airpodsPacket = packet.mid(4);
            if (areAirpodsConnected()) {
                writePacketToSocket(airpodsPacket, "Relayed notification to AirPods: ");
            } else {
                LOG_ERROR("AirPods not connected, cannot relay packet to AirPods");
            }
        }
        else if (packet.startsWith(AirPodsPackets::Phone::CONNECTED))
        {
            LOG_INFO("AirPods connected");
            isConnectedLocally = true;
            CrossDevice.isAvailable = false;
        }
        else if (packet.startsWith(AirPodsPackets::Phone::DISCONNECTED))
        {
            LOG_INFO("AirPods disconnected");
            isConnectedLocally = false;
            CrossDevice.isAvailable = true;
        }
        else if (packet.startsWith(AirPodsPackets::Phone::STATUS_REQUEST))
        {
            LOG_INFO("Connection status request received");
            QByteArray response = areAirpodsConnected() ? AirPodsPackets::Phone::CONNECTED
                                                        : AirPodsPackets::Phone::DISCONNECTED;
            phoneSocket->write(response);
            LOG_DEBUG("Sent connection status response: " << response.toHex());
        }
        else if (packet.startsWith(AirPodsPackets::Phone::DISCONNECT_REQUEST))
        {
            LOG_INFO("Disconnect request received");
            if (areAirpodsConnected()) {
#ifdef Q_OS_WIN
                closeMagicAap();
                LOG_INFO("Disconnected from AirPods via closeMagicAap");
#else
                if (socket && socket->isOpen()) {
                    socket->close();
                    LOG_INFO("Disconnected from AirPods");
                    QProcess process;
                    process.start("bluetoothctl", QStringList() << "disconnect" << m_deviceInfo->bluetoothAddress());
                    process.waitForFinished();
                    QString output = process.readAllStandardOutput().trimmed();
                    LOG_INFO("Bluetoothctl output: " << output);
                }
#endif
                isConnectedLocally = false;
                CrossDevice.isAvailable = true;
            }
        }
        else
        {
            if (areAirpodsConnected()) {
                writePacketToSocket(packet, "Relayed packet to AirPods: ");
            } else {
                LOG_ERROR("AirPods not connected, cannot relay packet to AirPods");
            }
        }
    }

    void onPhoneDataReceived() {
        QByteArray data = phoneSocket->readAll();
        LOG_DEBUG("Data received from phone: " << data.toHex());
        QMetaObject::invokeMethod(this, "handlePhonePacket", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }

    void bleDeviceFound(const BleInfo &device)
    {
        if (m_deviceInfo->magicAccIRK().isEmpty() || !BLEUtils::isValidIrkRpa(m_deviceInfo->magicAccIRK(), device.address)) {
            if (device.modelName != AirPodsModel::Unknown) {
                m_deviceInfo->setModel(device.modelName);
            }
            if (device.leftPodBattery >= 0 || device.rightPodBattery >= 0 || device.caseBattery >= 0) {
                m_deviceInfo->getBattery()->updateFromBleRaw(
                    device.leftPodBattery, device.leftCharging,
                    device.rightPodBattery, device.rightCharging,
                    device.caseBattery, device.caseCharging,
                    device.primaryLeft
                );
                m_deviceInfo->updateBatteryStatus();
            }
            return;
        }

        m_deviceInfo->setModel(device.modelName);
        auto decryptet = BLEUtils::decryptLastBytes(device.encryptedPayload, m_deviceInfo->magicAccEncKey());
        m_deviceInfo->getBattery()->parseEncryptedPacket(decryptet, device.primaryLeft, device.isThisPodInTheCase, isModelHeadset(m_deviceInfo->model()));
        m_deviceInfo->getEarDetection()->overrideEarDetectionStatus(device.isPrimaryInEar, device.isSecondaryInEar);
        if (mediaController)
        {
            mediaController->handleEarDetection(m_deviceInfo->getEarDetection());
        }
    }

public:
    void handleMediaStateChange(MediaController::MediaState state) {
        if (state == MediaController::MediaState::Playing) {
            LOG_INFO("Media started playing, sending disconnect request to Android and taking over audio");
            sendDisconnectRequestToAndroid();
            connectToAirPods(true);
        }
    }

    void sendDisconnectRequestToAndroid()
    {
        if (!CrossDevice.isEnabled) return;

        if (phoneSocket && phoneSocket->isOpen())
        {
            phoneSocket->write(AirPodsPackets::Phone::DISCONNECT_REQUEST);
            LOG_DEBUG("Sent disconnect request to Android: " << AirPodsPackets::Phone::DISCONNECT_REQUEST.toHex());
        }
        else
        {
            LOG_WARN("Phone socket is not open, cannot send disconnect request");
        }
    }

    bool isPhoneConnected() {
        return phoneSocket && phoneSocket->isOpen();
    }

    void connectToAirPods(bool force) {
#ifndef Q_OS_WIN
        if (socket && socket->isOpen()) {
            LOG_INFO("Already connected to AirPods");
            return;
        }

        if (force) {
            LOG_INFO("Forcing connection to AirPods");
            QProcess process;
            process.start("bluetoothctl", QStringList() << "connect" << m_deviceInfo->bluetoothAddress());
            process.waitForFinished();
            QString output = process.readAllStandardOutput().trimmed();
            LOG_INFO("Bluetoothctl output: " << output);
        }
        QBluetoothLocalDevice localDevice;
        const QList<QBluetoothAddress> connectedDevices = localDevice.connectedDevices();
        for (const QBluetoothAddress &address : connectedDevices) {
            QBluetoothDeviceInfo device(address, "", 0);
            LOG_DEBUG("Connected device: " << device.name() << " (" << device.address().toString() << ")");
            if (isAirPodsDevice(device)) {
                connectToDevice(device);
                return;
            }
        }
        LOG_WARN("AirPods not found among connected devices");
#else
        Q_UNUSED(force);
#endif
    }

    void initializeBluetooth() {
        connectToPhone();

        m_deviceInfo->loadFromSettings(*m_settings);
        if (!areAirpodsConnected()) {
            m_bleManager->startScan();
        }
    }

    void loadMainModule() {
        parent->load(QUrl(QStringLiteral("qrc:/windows/Main.qml")));
        if (!parent->rootObjects().isEmpty()) {
            QQuickWindow *window = qobject_cast<QQuickWindow *>(parent->rootObjects().first());
            if (window) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    }

    void reopenOrLoadModule(const QString &pageToLoad = "app") {
        if (parent->rootObjects().isEmpty())
        {
            LOG_WARN("Root objects list is empty, loading main module");
            loadMainModule();
            return;
        }
        QObject *rootObject = parent->rootObjects().first();
        QMetaObject::invokeMethod(rootObject, "reopen", Q_ARG(QVariant, pageToLoad));
    }

#ifdef Q_OS_WIN
    bool writeMagicAapRaw(const void *data, DWORD size)
    {
        if (m_hMagicAap == INVALID_HANDLE_VALUE) return false;
        OVERLAPPED ov = {0};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        DWORD written = 0;
        BOOL ok = WriteFile(m_hMagicAap, data, size, &written, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING)
        {
            if (WaitForSingleObject(ov.hEvent, 300) == WAIT_OBJECT_0)
            {
                GetOverlappedResult(m_hMagicAap, &ov, &written, FALSE);
            }
            else
            {
                LOG_ERROR("MagicAAP raw write timed out after 300ms");
                CancelIoEx(m_hMagicAap, &ov);
            }
        }
        CloseHandle(ov.hEvent);
        return (written == size);
    }

    void closeMagicAap()
    {
        m_initialSyncInProgress = true;
        m_magicAapRunning = false;
        if (m_hMagicAap != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(m_hMagicAap, NULL);
        }
        if (m_magicAapThread.joinable())
        {
            m_magicAapThread.join();
        }
        if (m_hMagicAap != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hMagicAap);
            m_hMagicAap = INVALID_HANDLE_VALUE;
        }
        if (mediaController)
        {
            mediaController->cancelAutoResume();
            mediaController->resetConversationalAwareness();
        }
        if (m_deviceInfo && m_deviceInfo->getSpatialTracker())
        {
            m_deviceInfo->getSpatialTracker()->setConnected(false);
        }
    }

    bool ensureMagicAapOpen()
    {
        // Don't open or hold the driver handle if AirPods are not actively connected to Windows
        if (!IsAirPodsConnectedWin32())
        {
            if (m_hMagicAap != INVALID_HANDLE_VALUE)
            {
                LOG_INFO("AirPods disconnected in Windows Bluetooth, closing MagicAAP handle to release device");
                closeMagicAap();
            }
            return false;
        }

        if (m_hMagicAap != INVALID_HANDLE_VALUE)
        {
            return true;
        }

        if (m_magicAapThread.joinable())
        {
            m_magicAapRunning = false;
            m_magicAapThread.join();
        }

        std::vector<std::wstring> paths = GetMagicAapInterfaces(GUID_DEVINTERFACE_MAGICAAP);
        if (paths.empty())
        {
            paths = GetMagicAapInterfaces(GUID_DEVINTERFACE_AAP_SERVER);
        }
        if (paths.empty())
        {
            return false;
        }

        LOG_INFO("MagicAAP device interface found: " << QString::fromStdWString(paths[0]));
        m_hMagicAap = CreateFileW(paths[0].c_str(),
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED,
                                  NULL);

        if (m_hMagicAap == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            LOG_DEBUG("MagicAAP handle not ready yet. Win32 Error: " << err);
            return false;
        }

        LOG_INFO("MagicAAP handle opened successfully!");
        m_initialSyncInProgress = true;
        if (m_deviceInfo && m_deviceInfo->getSpatialTracker())
        {
            m_deviceInfo->getSpatialTracker()->setConnected(true);
        }

        // Send AAP Handshake
        static const BYTE Handshake[] = {
            0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        writeMagicAapRaw(Handshake, sizeof(Handshake));

        // Send feature notification requests
        writeMagicAapRaw(AirPodsPackets::Connection::SET_SPECIFIC_FEATURES.constData(),
                         static_cast<DWORD>(AirPodsPackets::Connection::SET_SPECIFIC_FEATURES.size()));
        writeMagicAapRaw(AirPodsPackets::Connection::REQUEST_NOTIFICATIONS.constData(),
                         static_cast<DWORD>(AirPodsPackets::Connection::REQUEST_NOTIFICATIONS.size()));

        // Restore saved preferences after handshake
        QTimer::singleShot(600, this, [this]() {
            restoreSavedPreferences();
        });

        // Conclude initial sync window after 1.5s
        QTimer::singleShot(1500, this, [this]() {
            m_initialSyncInProgress = false;
            LOG_INFO("Initial connection sync completed, saved preferences active");
        });

        m_deviceInfo->setBluetoothAddress("34:0E:22:56:BA:38");
        mediaController->setConnectedDeviceMacAddress("34_0E_22_56_BA_38");

        if (m_deviceInfo->deviceName().isEmpty())
        {
            m_deviceInfo->setDeviceName("AirPods Pro");
            if (m_deviceInfo->model() == AirPodsModel::Unknown) {
                m_deviceInfo->setModel(AirPodsModel::AirPodsPro2USBC);
            }
        }

        m_isAirPodsConnectedFallback = true;
        emit airPodsStatusChanged();

        // Start background reader thread with non-blocking overlapped I/O
        m_magicAapRunning = true;
        m_magicAapThread = std::thread([this]() {
            BYTE buf[1024];
            OVERLAPPED ov = {0};
            ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            while (m_magicAapRunning && m_hMagicAap != INVALID_HANDLE_VALUE)
            {
                ResetEvent(ov.hEvent);
                DWORD bytesRead = 0;
                BOOL ok = ReadFile(m_hMagicAap, buf, sizeof(buf), &bytesRead, &ov);
                if (!ok)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING)
                    {
                        bool gotData = false;
                        while (m_magicAapRunning && m_hMagicAap != INVALID_HANDLE_VALUE)
                        {
                            DWORD waitRes = WaitForSingleObject(ov.hEvent, 200);
                            if (waitRes == WAIT_OBJECT_0)
                            {
                                if (m_hMagicAap != INVALID_HANDLE_VALUE && GetOverlappedResult(m_hMagicAap, &ov, &bytesRead, FALSE))
                                {
                                    gotData = true;
                                }
                                break;
                            }
                        }
                        if (!gotData) continue;
                    }
                    else
                    {
                        break;
                    }
                }

                if (bytesRead > 0)
                {
                    QByteArray data(reinterpret_cast<const char*>(buf), static_cast<int>(bytesRead));
                    QMetaObject::invokeMethod(this, [this, data]() {
                        parseData(data);
                    }, Qt::QueuedConnection);
                }
            }

            if (ov.hEvent)
            {
                CloseHandle(ov.hEvent);
            }
            m_magicAapRunning = false;
        });

        return true;
    }

    void checkBluetoothConnection()
    {
        bool isConnected = IsAirPodsConnectedWin32();
        if (!isConnected)
        {
            if (m_hMagicAap != INVALID_HANDLE_VALUE)
            {
                LOG_INFO("AirPods disconnected, releasing MagicAAP handle");
                closeMagicAap();
            }
            if (m_isAirPodsConnectedFallback)
            {
                m_isAirPodsConnectedFallback = false;
                if (mediaController)
                {
                    if (mediaController->getEarDetectionBehavior() != MediaController::Disabled &&
                        mediaController->getCurrentMediaState() == MediaController::Playing)
                    {
                        LOG_INFO("AirPods disconnected in Bluetooth monitor while media was playing; pausing playback");
                        mediaController->pause();
                    }
                }
                emit airPodsStatusChanged();
            }
            return;
        }

        if (ensureMagicAapOpen())
        {
            if (!m_isAirPodsConnectedFallback)
            {
                m_isAirPodsConnectedFallback = true;
                emit airPodsStatusChanged();
            }
            return;
        }

        if (!m_isAirPodsConnectedFallback)
        {
            LOG_INFO("AirPods detected as connected in Windows Bluetooth!");
            m_isAirPodsConnectedFallback = true;
            if (m_deviceInfo->deviceName().isEmpty())
            {
                m_deviceInfo->setDeviceName("AirPods Pro");
                m_deviceInfo->setBluetoothAddress("34:0E:22:56:BA:38");
                if (m_deviceInfo->model() == AirPodsModel::Unknown) {
                    m_deviceInfo->setModel(AirPodsModel::AirPodsPro2USBC);
                }
            }
            emit airPodsStatusChanged();
        }
    }
#endif

signals:
    void noiseControlModeChanged(NoiseControlMode mode);
    void earDetectionStatusChanged(const QString &status);
    void batteryStatusChanged(const QString &status);
    void conversationalAwarenessChanged(bool enabled);
    void adaptiveNoiseLevelChanged(int level);
    void deviceNameChanged(const QString &name);
    void modelChanged();
    void primaryChanged();
    void airPodsStatusChanged();
    void earDetectionBehaviorChanged(int behavior);
    void crossDeviceEnabledChanged(bool enabled);
    void notificationsEnabledChanged(bool enabled);
    void retryAttemptsChanged(int attempts);
    void oneBudANCModeChanged(bool enabled);
    void phoneMacStatusChanged();
    void hearingAidEnabledChanged(bool enabled);

private:
    QBluetoothSocket *socket = nullptr;
    QBluetoothSocket *phoneSocket = nullptr;
    QByteArray lastBatteryStatus;
    QByteArray lastEarDetectionStatus;
    MediaController* mediaController;
    TrayIconManager *trayManager;
    BluetoothMonitor *monitor;
    QSettings *m_settings;
    AutoStartManager *m_autoStartManager;
    int m_retryAttempts = 3;
    bool m_hideOnStart = false;
    DeviceInfo *m_deviceInfo;
    BleManager *m_bleManager;
#ifdef Q_OS_LINUX
    SystemSleepMonitor *m_systemSleepMonitor = nullptr;
#elif defined(Q_OS_WIN)
    WindowsSleepMonitor *m_systemSleepMonitor = nullptr;
    HANDLE m_hMagicAap = INVALID_HANDLE_VALUE;
    std::thread m_magicAapThread;
    std::atomic<bool> m_magicAapRunning{false};
    QTimer *m_magicAapPollTimer = nullptr;
    bool m_isAirPodsConnectedFallback = false;
#endif
    QString m_phoneMacStatus;
    bool m_initialSyncInProgress = true;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Install message handler to ensure logs are printed to console on Windows
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
        QByteArray localMsg = msg.toLocal8Bit();
        FILE *output = (type == QtDebugMsg || type == QtInfoMsg) ? stdout : stderr;
        
        switch (type) {
        case QtDebugMsg:
            fprintf(output, "[DEBUG] %s\n", localMsg.constData());
            break;
        case QtInfoMsg:
            fprintf(output, "[INFO] %s\n", localMsg.constData());
            break;
        case QtWarningMsg:
            fprintf(output, "[WARNING] %s\n", localMsg.constData());
            break;
        case QtCriticalMsg:
            fprintf(output, "[ERROR] %s\n", localMsg.constData());
            break;
        case QtFatalMsg:
            fprintf(output, "[FATAL] %s\n", localMsg.constData());
            abort();
        }
        fflush(output);
    });

    // Check if app is already open via IPC socket
    {
        QLocalSocket socket;
        socket.connectToServer("app_server");
        if (socket.waitForConnected(500)) {
            LOG_INFO("Another instance already running! Opening App Window Instead");
            socket.write("reopen");
            socket.flush();
            socket.waitForBytesWritten(500);
            socket.disconnectFromServer();
            return 0;
        }
    }
    app.setApplicationName("LibrePods");
    app.setDesktopFileName("me.kavishdevar.librepods");
    app.setQuitOnLastWindowClosed(false);

    bool debugMode = false;
    bool hideOnStart = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--debug")
            debugMode = true;

        if (QString(argv[i]) == "--hide")
            hideOnStart = true;
    }

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QQmlApplicationEngine engine;
    qmlRegisterType<Battery>("me.kavishdevar.Battery", 1, 0, "Battery");
    qmlRegisterType<DeviceInfo>("me.kavishdevar.DeviceInfo", 1, 0, "DeviceInfo");
    qmlRegisterType<SpatialAudioTracker>("me.kavishdevar.SpatialAudioTracker", 1, 0, "SpatialAudioTracker");
    AirPodsTrayApp *trayApp = new AirPodsTrayApp(debugMode, hideOnStart, &engine);
    engine.rootContext()->setContextProperty("airPodsTrayApp", trayApp);

    // Expose PHONE_MAC_ADDRESS environment variable to QML for placeholder in settings
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString phoneMacEnv = env.value("PHONE_MAC_ADDRESS", "");
        engine.rootContext()->setContextProperty("PHONE_MAC_ADDRESS", phoneMacEnv);
        // Initialize the visible status in the GUI
        trayApp->updatePhoneMacStatus(phoneMacEnv.isEmpty() ? QStringLiteral("No phone MAC set") : phoneMacEnv);
    }

    engine.addImageProvider("qrcode", new QRCodeImageProvider());
    trayApp->loadMainModule();
    
    // Verify QML loaded successfully
    if (engine.rootObjects().isEmpty())
    {
        LOG_ERROR("Failed to load QML module - root objects list is empty");
        LOG_ERROR("Critical error: Unable to load main QML interface. The application cannot start.");
        return 1;
    }
    LOG_INFO("Main QML module loaded successfully");

    QLocalServer server;
    QLocalServer::removeServer("app_server");

    if (!server.listen("app_server"))
    {
        LOG_ERROR("Unable to start the listening server");
        LOG_DEBUG("Server error: " << server.errorString());
    }
    else
    {
        LOG_DEBUG("Server started, waiting for connections...");
    }
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket* socket = server.nextPendingConnection();
        // Handles Proper Connection
        QObject::connect(socket, &QLocalSocket::readyRead, [socket, trayApp]() {
            QString msg = QString::fromUtf8(socket->readAll()).trimmed();
            if (msg == "reopen") {
                LOG_INFO("Reopening app window");
                trayApp->reopenOrLoadModule("app");
            }
            else if (msg == "noise:off") {
                LOG_INFO("IPC command received: noise:off");
                trayApp->setNoiseControlModeInt(0);
            }
            else if (msg == "noise:anc") {
                LOG_INFO("IPC command received: noise:anc");
                trayApp->setNoiseControlModeInt(1);
            }
            else if (msg == "noise:transparency") {
                LOG_INFO("IPC command received: noise:transparency");
                trayApp->setNoiseControlModeInt(2);
            }
            else if (msg == "noise:adaptive") {
                LOG_INFO("IPC command received: noise:adaptive");
                trayApp->setNoiseControlModeInt(3);
            }
            else if (msg == "spatial:on" || msg == "spatial:enable") {
                LOG_INFO("IPC command received: spatial:on");
                trayApp->deviceInfo()->getSpatialTracker()->setEnabled(true);
            }
            else if (msg == "spatial:off" || msg == "spatial:disable") {
                LOG_INFO("IPC command received: spatial:off");
                trayApp->deviceInfo()->getSpatialTracker()->setEnabled(false);
            }
            else if (msg == "spatial:recenter") {
                LOG_INFO("IPC command received: spatial:recenter");
                trayApp->deviceInfo()->getSpatialTracker()->recenter();
            }
            else
            {
                LOG_ERROR("Unknown message received: " << msg);
            }
            socket->disconnectFromServer();
        });
        // Handles connection errors
        QObject::connect(socket, &QLocalSocket::errorOccurred, [socket]() {
            LOG_ERROR("Failed to connect to the duplicate app instance");
            LOG_DEBUG("Connection error: " << socket->errorString());
        });
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        LOG_DEBUG("Application is about to quit. Cleaning up...");
        QLocalServer::removeServer("app_server");
    });
    return app.exec();
}

#include "main.moc"
