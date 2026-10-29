#include "mediacontroller.h"
#include "logger.h"
#include "eardetection.hpp"
#include "playerstatuswatcher.h"

#ifdef Q_OS_LINUX
#include "pulseaudiocontroller.h"
#elif defined(Q_OS_WIN)
#include "windowsaudiocontroller.h"
#include <windows.h>
#endif

#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QRegularExpression>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

MediaController::MediaController(QObject *parent) : QObject(parent) {
#ifdef Q_OS_LINUX
  m_pulseAudio = new PulseAudioController(this);
  if (!m_pulseAudio->initialize())
  {
    LOG_ERROR("Failed to initialize PulseAudio controller");
  }
#elif defined(Q_OS_WIN)
  m_windowsAudio = new WindowsAudioController(this);
  if (!m_windowsAudio->initialize())
  {
    LOG_ERROR("Failed to initialize Windows Audio controller");
  }
#endif
}

void MediaController::handleEarDetection(EarDetection *earDetection)
{
  if (earDetectionBehavior == Disabled)
  {
    LOG_DEBUG("Ear detection is disabled, ignoring status");
    return;
  }

  bool primaryInEar = earDetection->isPrimaryInEar();
  bool secondaryInEar = earDetection->isSecondaryInEar();
  bool primaryAvailable = (earDetection->getprimaryStatus() != EarDetection::EarDetectionStatus::InCase &&
                           earDetection->getprimaryStatus() != EarDetection::EarDetectionStatus::Disconnected);
  bool secondaryAvailable = (earDetection->getsecondaryStatus() != EarDetection::EarDetectionStatus::InCase &&
                             earDetection->getsecondaryStatus() != EarDetection::EarDetectionStatus::Disconnected);

  LOG_DEBUG("Ear detection status: primaryInEar="
            << primaryInEar << " (avail=" << primaryAvailable << "), secondaryInEar="
            << secondaryInEar << " (avail=" << secondaryAvailable << ")");

  // First handle playback pausing based on selected behavior and available earbuds
  bool shouldPause = false;
  bool shouldResume = false;

  if (primaryAvailable && secondaryAvailable)
  {
    // Both earbuds are in active use
    if (earDetectionBehavior == PauseWhenOneRemoved)
    {
      shouldPause = !primaryInEar || !secondaryInEar;
      shouldResume = primaryInEar && secondaryInEar;
    }
    else // PauseWhenBothRemoved
    {
      shouldPause = !primaryInEar && !secondaryInEar;
      shouldResume = primaryInEar || secondaryInEar;
    }
  }
  else if (primaryAvailable)
  {
    // Single earbud in use (primary)
    shouldPause = !primaryInEar;
    shouldResume = primaryInEar;
  }
  else if (secondaryAvailable)
  {
    // Single earbud in use (secondary)
    shouldPause = !secondaryInEar;
    shouldResume = secondaryInEar;
  }
  else
  {
    // Neither earbud is in the ear
    shouldPause = true;
    shouldResume = false;
  }

#ifdef Q_OS_WIN
  if (shouldPause)
  {
    if (getCurrentMediaState() == Playing)
    {
      LOG_INFO("Ear detection: earbud(s) removed, pausing media playback");
      pause();
    }
  }
  else if (shouldResume && m_pausedByLibrePods)
  {
    LOG_INFO("Ear detection: earbud(s) inserted, resuming media playback");
    play();
  }
#else
  if (shouldPause && isActiveOutputDeviceAirPods())
  {
    if (getCurrentMediaState() == Playing)
    {
      LOG_DEBUG("Pausing playback for ear detection");
      pause();
    }
  }
#endif

  // Then handle device profile switching
  if (primaryInEar || secondaryInEar)
  {
    LOG_INFO("At least one AirPod is in ear");
    activateA2dpProfile();

#ifndef Q_OS_WIN
    if (shouldResume && !pausedByAppServices.isEmpty() && isActiveOutputDeviceAirPods())
    {
      play();
    }
#endif
  }
  else
  {
    LOG_INFO("Both AirPods are out of ear");
    removeAudioOutputDevice();
  }
}

void MediaController::setEarDetectionBehavior(EarDetectionBehavior behavior)
{
  earDetectionBehavior = behavior;
  LOG_INFO("Set ear detection behavior to: " << behavior);
}

void MediaController::followMediaChanges() {
  playerStatusWatcher = new PlayerStatusWatcher("", this);
  connect(playerStatusWatcher, &PlayerStatusWatcher::playbackStatusChanged,
          this, [this](const QString &status)
          {
            LOG_DEBUG("Playback status changed: " << status);
            MediaState state = mediaStateFromPlayerctlOutput(status);
            emit mediaStateChanged(state);
          });
}

bool MediaController::isActiveOutputDeviceAirPods() {
#ifdef Q_OS_WIN
  if (!m_windowsAudio) {
    return false;
  }
  QString friendly = m_windowsAudio->getDefaultSinkFriendlyName();
  LOG_DEBUG("Default sink friendly name: " << friendly);
  if (friendly.contains("AirPods", Qt::CaseInsensitive)) {
    return true;
  }
  if (!m_deviceOutputName.isEmpty() && friendly.contains(m_deviceOutputName, Qt::CaseInsensitive)) {
    return true;
  }
  return false;
#else
  QString defaultSink = getDefaultSink();
  LOG_DEBUG("Default sink: " << defaultSink);
  return defaultSink.contains(connectedDeviceMacAddress);
#endif
}

void MediaController::resetConversationalAwareness() {
  if (initialVolume != -1) {
    QString defaultSink = getDefaultSink();
    if (!defaultSink.isEmpty()) {
      if (setSinkVolume(defaultSink, initialVolume)) {
        LOG_INFO("Volume restored upon CA reset to " << initialVolume << "%");
      }
    }
    initialVolume = -1;
  }
}

void MediaController::handleConversationalAwareness(const QByteArray &data) {
  LOG_DEBUG("Handling conversational awareness data: " << data.toHex());
  if (data.size() < 10) {
    LOG_ERROR("Invalid conversational awareness packet");
    return;
  }

  uint8_t flag = static_cast<uint8_t>(data[9]);

  switch (flag) {
  case 0x01:
    LOG_INFO("Conversational awareness event: voice detected");
    if (initialVolume == -1 && isActiveOutputDeviceAirPods()) {
      QString defaultSink = getDefaultSink();
      initialVolume = getSinkVolume(defaultSink);
      if (initialVolume == -1) {
        LOG_ERROR("Failed to get initial volume");
        return;
      }
      LOG_DEBUG("Initial volume: " << initialVolume << "%");
    }
    if (initialVolume != -1) {
      QString defaultSink = getDefaultSink();
      int targetVolume = initialVolume * 0.20;
      if (setSinkVolume(defaultSink, targetVolume)) {
        LOG_INFO("Volume lowered to 0.20 of initial which is " << targetVolume << "%");
      } else {
        LOG_ERROR("Failed to lower volume");
      }
    }
    break;

  case 0x08:
    LOG_INFO("Conversational awareness disabled");
    initialVolume = -1;
    break;

  case 0x09:
    LOG_INFO("Conversational awareness enabled");
    break;

  default:
    LOG_INFO("Conversational awareness event: voice ended");
    if (initialVolume != -1 && isActiveOutputDeviceAirPods()) {
      QString defaultSink = getDefaultSink();
      if (setSinkVolume(defaultSink, initialVolume)) {
        LOG_INFO("Volume restored to " << initialVolume << "%");
      } else {
        LOG_ERROR("Failed to restore volume");
      }
      initialVolume = -1;
    }
    break;
  }
}

bool MediaController::isA2dpProfileAvailable() {
  if (m_deviceOutputName.isEmpty()) {
    return false;
  }

  return isProfileAvailable(m_deviceOutputName, "a2dp-sink-sbc_xq") || 
         isProfileAvailable(m_deviceOutputName, "a2dp-sink-sbc") ||
         isProfileAvailable(m_deviceOutputName, "a2dp-sink");
}

QString MediaController::getPreferredA2dpProfile() {
  if (m_deviceOutputName.isEmpty()) {
    return QString();
  }

  if (!m_cachedA2dpProfile.isEmpty() && 
      isProfileAvailable(m_deviceOutputName, m_cachedA2dpProfile)) {
    return m_cachedA2dpProfile;
  }

  QStringList profiles = {"a2dp-sink-sbc_xq", "a2dp-sink-sbc", "a2dp-sink"};

  for (const QString &profile : profiles) {
    if (isProfileAvailable(m_deviceOutputName, profile)) {
      LOG_INFO("Selected best available A2DP profile: " << profile);
      m_cachedA2dpProfile = profile;
      return profile;
    }
  }

  m_cachedA2dpProfile.clear();
  return QString();
}

bool MediaController::restartWirePlumber() {
  LOG_INFO("Restarting WirePlumber to rediscover A2DP profiles");
  int result = QProcess::execute("systemctl", QStringList() << "--user" << "restart" << "wireplumber");
  if (result == 0) {
    LOG_INFO("WirePlumber restarted successfully");
    QThread::sleep(2);
    return true;
  } else {
    LOG_ERROR("Failed to restart WirePlumber. Do you use wireplumber?");
    return false;
  }
}

void MediaController::activateA2dpProfile() {
  if (connectedDeviceMacAddress.isEmpty() || m_deviceOutputName.isEmpty()) {
    LOG_WARN("Connected device MAC address or output name is empty, cannot activate A2DP profile");
    return;
  }

  if (!isA2dpProfileAvailable()) {
    LOG_WARN("A2DP profile not available, attempting to restart WirePlumber");
    if (restartWirePlumber()) {
      m_deviceOutputName = getAudioDeviceName();
      if (!isA2dpProfileAvailable()) {
        LOG_ERROR("A2DP profile still not available after WirePlumber restart");
        return;
      }
    } else {
      LOG_ERROR("Could not restart WirePlumber, A2DP profile unavailable");
      return;
    }
  }

  QString preferredProfile = getPreferredA2dpProfile();
  if (preferredProfile.isEmpty()) {
    LOG_ERROR("No suitable A2DP profile found");
    return;
  }

  LOG_INFO("Activating A2DP profile for AirPods: " << preferredProfile);
  if (!setCardProfile(m_deviceOutputName, preferredProfile)) {
    LOG_ERROR("Failed to activate A2DP profile: " << preferredProfile);
  }
  LOG_INFO("A2DP profile activated successfully");
}

void MediaController::removeAudioOutputDevice() {
  resetConversationalAwareness();
  if (connectedDeviceMacAddress.isEmpty() || m_deviceOutputName.isEmpty()) {
    LOG_WARN("Connected device MAC address or output name is empty, cannot remove audio output device");
    return;
  }
  
  LOG_INFO("Removing AirPods as audio output device");
  if (!setCardProfile(m_deviceOutputName, "off")) {
    LOG_ERROR("Failed to remove AirPods as audio output device");
  }
}

void MediaController::setConnectedDeviceMacAddress(const QString &macAddress) {
  connectedDeviceMacAddress = macAddress;
  m_deviceOutputName = getAudioDeviceName();
  m_cachedA2dpProfile.clear();
  LOG_INFO("Device output name set to: " << m_deviceOutputName);
}

MediaController::MediaState MediaController::mediaStateFromPlayerctlOutput(
    const QString &output) const {
  if (output == "Playing") {
    return MediaState::Playing;
  } else if (output == "Paused") {
    return MediaState::Paused;
  } else {
    return MediaState::Stopped;
  }
}

MediaController::MediaState MediaController::getCurrentMediaState() const
{
#ifdef Q_OS_WIN
  if (m_windowsAudio && m_windowsAudio->isAudioPlaying())
  {
    return MediaState::Playing;
  }
  return MediaState::Paused;
#else
  return mediaStateFromPlayerctlOutput(PlayerStatusWatcher::getCurrentPlaybackStatus(""));
#endif
}

QStringList MediaController::getPlayingMediaPlayers()
{
  QStringList playingServices;
#ifndef Q_OS_WIN
  QDBusConnection bus = QDBusConnection::sessionBus();

  QStringList services = bus.interface()->registeredServiceNames().value();
  for (const QString &service : services)
  {
    if (!service.startsWith("org.mpris.MediaPlayer2."))
    {
      continue;
    }

    QDBusInterface playerInterface(
        service,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        bus);

    if (!playerInterface.isValid())
    {
      continue;
    }

    QVariant playbackStatus = playerInterface.property("PlaybackStatus");
    if (playbackStatus.isValid() && playbackStatus.toString() == "Playing")
    {
      playingServices << service;
      LOG_DEBUG("Found playing service: " << service);
    }
  }
#else
  // On Windows, media control is handled differently
  // This functionality requires Windows Media Session API integration
  LOG_DEBUG("getPlayingMediaPlayers not implemented on Windows yet");
#endif

  return playingServices;
}

void MediaController::play()
{
#ifdef Q_OS_WIN
  if (m_pausedByLibrePods)
  {
    LOG_INFO("Windows: Resuming media playback via VK_MEDIA_PLAY_PAUSE");
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    m_pausedByLibrePods = false;
  }
  else
  {
    LOG_DEBUG("Windows: play called but playback was not paused by LibrePods");
  }
  return;
#else
  if (pausedByAppServices.isEmpty())
  {
    LOG_INFO("No services to resume");
    return;
  }
#endif

#ifndef Q_OS_WIN
  QDBusConnection bus = QDBusConnection::sessionBus();
  int resumedCount = 0;

  for (const QString &service : pausedByAppServices)
  {
    QDBusInterface playerInterface(
        service,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        bus);

    if (!playerInterface.isValid())
    {
      LOG_WARN("Service no longer available: " << service);
      continue;
    }

    QDBusReply<void> reply = playerInterface.call("Play");
    if (reply.isValid())
    {
      LOG_INFO("Resumed playback for: " << service);
      resumedCount++;
    }
    else
    {
      LOG_ERROR("Failed to resume " << service << ": " << reply.error().message());
    }
  }

  if (resumedCount > 0)
  {
    LOG_INFO("Resumed " << resumedCount << " media player(s) via DBus");
    pausedByAppServices.clear();
  }
  else
  {
    LOG_ERROR("Failed to resume any media players via DBus");
  }
#else
  // On Windows, use Windows Media Control API
  LOG_DEBUG("Media playback resume not fully implemented on Windows yet");
  pausedByAppServices.clear();
#endif
}

void MediaController::pause()
{
#ifdef Q_OS_WIN
  LOG_INFO("Windows: Pausing media playback via VK_MEDIA_PLAY_PAUSE");
  INPUT inputs[2] = {};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
  inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, inputs, sizeof(INPUT));
  m_pausedByLibrePods = true;
  return;
#endif

#ifndef Q_OS_WIN
  QDBusConnection bus = QDBusConnection::sessionBus();
  QStringList services = bus.interface()->registeredServiceNames().value();

  pausedByAppServices.clear();
  int pausedCount = 0;

  for (const QString &service : services)
  {
    if (!service.startsWith("org.mpris.MediaPlayer2."))
    {
      continue;
    }

    QDBusInterface playerInterface(
        service,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        bus);

    if (!playerInterface.isValid())
    {
      continue;
    }

    QVariant playbackStatus = playerInterface.property("PlaybackStatus");
    LOG_DEBUG("PlaybackStatus for " << service << ": " << playbackStatus.toString());
    if (!playbackStatus.isValid() || playbackStatus.toString() != "Playing")
    {
      continue;
    }

    QDBusReply<void> reply = playerInterface.call("Pause");
    LOG_DEBUG("Pausing service: " << service);
    if (reply.isValid())
    {
      LOG_INFO("Paused playback for: " << service);
      pausedByAppServices << service;
      pausedCount++;
    }
    else
    {
      LOG_ERROR("Failed to pause " << service << ": " << reply.error().message());
    }
  }

  if (pausedCount > 0)
  {
    LOG_INFO("Paused " << pausedCount << " media player(s) via DBus");
  }
  else
  {
    LOG_INFO("No playing media players found to pause");
  }
#else
  // On Windows, use Windows Media Control API
  LOG_DEBUG("Media playback pause not fully implemented on Windows yet");
#endif
}

MediaController::~MediaController() {
  resetConversationalAwareness();
}

QString MediaController::getAudioDeviceName()
{
  QString cardName = getCardNameForDevice(connectedDeviceMacAddress);
  if (cardName.isEmpty()) {
    LOG_ERROR("No matching Bluetooth card found for MAC address: " << connectedDeviceMacAddress);
  }
  return cardName;
}

// Platform-independent wrapper methods
QString MediaController::getDefaultSink()
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->getDefaultSink() : QString();
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->getDefaultSink() : QString();
#else
  return QString();
#endif
}

int MediaController::getSinkVolume(const QString &sinkName)
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->getSinkVolume(sinkName) : -1;
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->getSinkVolume(sinkName) : -1;
#else
  return -1;
#endif
}

bool MediaController::setSinkVolume(const QString &sinkName, int volumePercent)
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->setSinkVolume(sinkName, volumePercent) : false;
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->setSinkVolume(sinkName, volumePercent) : false;
#else
  return false;
#endif
}

QString MediaController::getCardNameForDevice(const QString &macAddress)
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->getCardNameForDevice(macAddress) : QString();
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->getCardNameForDevice(macAddress) : QString();
#else
  return QString();
#endif
}

bool MediaController::setCardProfile(const QString &cardName, const QString &profileName)
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->setCardProfile(cardName, profileName) : false;
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->setCardProfile(cardName, profileName) : true;
#else
  return false;
#endif
}

bool MediaController::isProfileAvailable(const QString &cardName, const QString &profileName)
{
#ifdef Q_OS_LINUX
  return m_pulseAudio ? m_pulseAudio->isProfileAvailable(cardName, profileName) : false;
#elif defined(Q_OS_WIN)
  return m_windowsAudio ? m_windowsAudio->isProfileAvailable(cardName, profileName) : true;
#else
  return false;
#endif
}