#pragma once

#include <QObject>
#include <QByteArray>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QtMath>
#include <cmath>
#include "airpods_packets.h"
#include "logger.h"

class SpatialAudioTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY orientationChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY orientationChanged)
    Q_PROPERTY(double roll READ roll NOTIFY orientationChanged)
    Q_PROPERTY(bool oscBroadcast READ isOscBroadcast WRITE setOscBroadcast NOTIFY oscBroadcastChanged)
    Q_PROPERTY(int oscPort READ oscPort WRITE setOscPort NOTIFY oscPortChanged)

public:
    struct SampleTriple {
        double o1;
        double o2;
        double o3;
    };

    explicit SpatialAudioTracker(QObject *parent = nullptr)
        : QObject(parent),
          m_udpSocket(new QUdpSocket(this)),
          m_watchdogTimer(new QTimer(this))
    {
        m_watchdogTimer->setInterval(3500);
        connect(m_watchdogTimer, &QTimer::timeout, this, &SpatialAudioTracker::onWatchdogTimeout);
    }

    ~SpatialAudioTracker() override
    {
        if (m_enabled)
        {
            emit sendPacketRequested(AirPodsPackets::SpatialAudio::STOP_TRACKING);
        }
    }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled)
    {
        if (m_enabled != enabled)
        {
            m_enabled = enabled;
            emit enabledChanged(enabled);

            if (enabled)
            {
                LOG_INFO("Activating AirPods spatial audio head-tracking stream");
                m_calibrated = false;
                m_calibSamples.clear();
                m_hasReceivedSensorPacket = false;
                m_activeVariant = 0; // 0 = ALT, 1 = DEF
                m_lastHtPacketTime = QDateTime::currentMSecsSinceEpoch();

                if (m_connected)
                {
                    // Send STOP first to clear any stale state, then START_ALT
                    emit sendPacketRequested(AirPodsPackets::SpatialAudio::STOP_TRACKING);
                    emit sendPacketRequested(AirPodsPackets::SpatialAudio::START_TRACKING_ALT);
                    m_watchdogTimer->start();
                }
            }
            else
            {
                LOG_INFO("Deactivating AirPods spatial audio head-tracking stream");
                m_watchdogTimer->stop();
                if (m_connected)
                {
                    emit sendPacketRequested(AirPodsPackets::SpatialAudio::STOP_TRACKING);
                }

                m_yaw = 0.0;
                m_pitch = 0.0;
                m_roll = 0.0;
                m_calibrated = false;
                m_calibSamples.clear();
                emit orientationChanged();
            }
        }
    }

    bool isConnected() const { return m_connected; }
    void setConnected(bool connected)
    {
        if (m_connected != connected)
        {
            m_connected = connected;
            if (!connected)
            {
                m_watchdogTimer->stop();
            }
            else if (m_enabled)
            {
                m_calibrated = false;
                m_calibSamples.clear();
                m_hasReceivedSensorPacket = false;
                m_lastHtPacketTime = QDateTime::currentMSecsSinceEpoch();
                emit sendPacketRequested(AirPodsPackets::SpatialAudio::START_TRACKING_ALT);
                m_watchdogTimer->start();
            }
        }
    }

    double yaw() const { return m_yaw; }
    double pitch() const { return m_pitch; }
    double roll() const { return m_roll; }

    bool isOscBroadcast() const { return m_oscBroadcast; }
    void setOscBroadcast(bool broadcast)
    {
        if (m_oscBroadcast != broadcast)
        {
            m_oscBroadcast = broadcast;
            emit oscBroadcastChanged(broadcast);
        }
    }

    int oscPort() const { return m_oscPort; }
    void setOscPort(int port)
    {
        if (m_oscPort != port)
        {
            m_oscPort = port;
            emit oscPortChanged(port);
        }
    }

    Q_INVOKABLE void recenter()
    {
        m_calibrated = false;
        m_calibSamples.clear();
        m_yaw = 0.0;
        m_pitch = 0.0;
        m_roll = 0.0;
        LOG_INFO("AirPods head tracker recenter requested - hold still facing forward");
        emit orientationChanged();
    }

    void parsePacket(const QByteArray &data)
    {
        if (!m_enabled)
        {
            return;
        }

        auto sample = AirPodsPackets::SpatialAudio::parseRawSample(data);
        if (!sample.valid)
        {
            // Drop keepalives, timestamps, or non-sensor packets without affecting orientation
            return;
        }

        m_hasReceivedSensorPacket = true;
        m_lastHtPacketTime = QDateTime::currentMSecsSinceEpoch();

        // Capture initial neutral pose (median of CALIB_N samples while head is stationary)
        const int CALIB_N = 10;
        if (!m_calibrated)
        {
            SampleTriple cur{ static_cast<double>(sample.o1),
                              static_cast<double>(sample.o2),
                              static_cast<double>(sample.o3) };
            m_calibSamples.append(cur);

            if (m_calibSamples.size() >= CALIB_N)
            {
                double sum1 = 0.0, sum2 = 0.0, sum3 = 0.0;
                for (const auto &s : m_calibSamples)
                {
                    sum1 += s.o1;
                    sum2 += s.o2;
                    sum3 += s.o3;
                }
                m_ref1 = sum1 / m_calibSamples.size();
                m_ref2 = sum2 / m_calibSamples.size();
                m_ref3 = sum3 / m_calibSamples.size();
                m_calibrated = true;
                m_calibSamples.clear();
                LOG_INFO("AirPods head tracker neutral calibrated: ("
                         << m_ref1 << ", " << m_ref2 << ", " << m_ref3 << ")");
            }
            return;
        }

        // Calibrated tracking calculation
        double o1n = wrap_s16(static_cast<double>(sample.o1) - m_ref1);
        double o2n = wrap_s16(static_cast<double>(sample.o2) - m_ref2);
        double o3n = wrap_s16(static_cast<double>(sample.o3) - m_ref3);

        const double FULL_SCALE = 32000.0;
        double targetPitch = qBound(-90.0, ((o2n + o3n) / 2.0 / FULL_SCALE) * 180.0, 90.0);
        double targetYaw   = qBound(-180.0, ((o2n - o3n) / 2.0 / FULL_SCALE) * 180.0, 180.0);
        double targetRoll  = qBound(-180.0, (((o1n - (o2n + o3n) / 2.0) / 2.0) / FULL_SCALE) * 180.0, 180.0);

        // Deadzone jitter suppression: if change is under 0.2 deg, freeze to eliminate sensor noise
        if (std::abs(targetYaw - m_yaw) < 0.2)
        {
            targetYaw = m_yaw;
        }
        if (std::abs(targetPitch - m_pitch) < 0.2)
        {
            targetPitch = m_pitch;
        }
        if (std::abs(targetRoll - m_roll) < 0.2)
        {
            targetRoll = m_roll;
        }

        // Exponential smoothing filter (alpha = 0.35)
        const double alpha = 0.35;
        m_yaw = (alpha * targetYaw) + ((1.0 - alpha) * m_yaw);
        m_pitch = (alpha * targetPitch) + ((1.0 - alpha) * m_pitch);
        m_roll = (alpha * targetRoll) + ((1.0 - alpha) * m_roll);

        emit orientationChanged();

        static int logCount = 0;
        if (++logCount % 25 == 0)
        {
            LOG_INFO("AirPods Head Orientation -> Yaw: "
                     << QString::number(m_yaw, 'f', 1) << "° | Pitch: "
                     << QString::number(m_pitch, 'f', 1) << "° | Roll: "
                     << QString::number(m_roll, 'f', 1) << "°");
        }

        if (m_oscBroadcast)
        {
            sendOscTelemetry(static_cast<float>(m_yaw),
                             static_cast<float>(m_pitch),
                             static_cast<float>(m_roll));
        }

        if (m_freePieBroadcast)
        {
            sendFreePieTelemetry(static_cast<float>(m_yaw),
                                 static_cast<float>(m_pitch),
                                 static_cast<float>(m_roll));
        }
    }

signals:
    void enabledChanged(bool enabled);
    void orientationChanged();
    void sendPacketRequested(const QByteArray &packet);
    void oscBroadcastChanged(bool broadcast);
    void oscPortChanged(int port);

private slots:
    void onWatchdogTimeout()
    {
        if (!m_enabled || !m_connected)
        {
            m_watchdogTimer->stop();
            return;
        }

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastHtPacketTime >= 3500)
        {
            // If stream is dry, re-send start command and alternate variants if necessary
            m_activeVariant = (m_activeVariant + 1) % 2;
            const QByteArray &pkt = (m_activeVariant == 0)
                ? AirPodsPackets::SpatialAudio::START_TRACKING_ALT
                : AirPodsPackets::SpatialAudio::START_TRACKING_DEF;
            LOG_INFO("Spatial audio: waiting for sensor stream, re-sending start packet (variant "
                     << (m_activeVariant == 0 ? "ALT" : "DEF") << ")");
            emit sendPacketRequested(pkt);
        }
    }

private:
    static inline double wrap_s16(double d)
    {
        double wrapped = std::fmod(d + 32768.0, 65536.0);
        if (wrapped < 0.0)
        {
            wrapped += 65536.0;
        }
        return wrapped - 32768.0;
    }

    void sendOscTelemetry(float y, float p, float r)
    {
        // Simple OSC packet: "/spatial/head\0\0\0,fff\0\0\0\0[float32 y][float32 p][float32 r]"
        QByteArray osc;
        const char address[] = "/spatial/head\0\0\0";
        osc.append(address, 16);
        const char tags[] = ",fff\0\0\0\0";
        osc.append(tags, 8);

        auto appendBigEndianFloat = [&osc](float val) {
            quint32 raw;
            memcpy(&raw, &val, sizeof(float));
            osc.append(static_cast<char>((raw >> 24) & 0xFF));
            osc.append(static_cast<char>((raw >> 16) & 0xFF));
            osc.append(static_cast<char>((raw >> 8) & 0xFF));
            osc.append(static_cast<char>(raw & 0xFF));
        };

        appendBigEndianFloat(y);
        appendBigEndianFloat(p);
        appendBigEndianFloat(r);

        m_udpSocket->writeDatagram(osc, QHostAddress::LocalHost, static_cast<quint16>(m_oscPort));
    }

    void sendFreePieTelemetry(float yawDeg, float pitchDeg, float rollDeg)
    {
        // FreePIE UDP frame (50 bytes): [pad: 1B=0][flags: 1B=0x02][12x float32 in radians]
        QByteArray packet(50, 0);
        packet[0] = 0;
        packet[1] = 0x02;

        float fl[12] = { 0.0f };
        fl[0] = static_cast<float>(qDegreesToRadians(yawDeg));
        fl[1] = static_cast<float>(qDegreesToRadians(pitchDeg));
        fl[2] = static_cast<float>(qDegreesToRadians(rollDeg));

        memcpy(packet.data() + 2, fl, sizeof(fl));
        m_udpSocket->writeDatagram(packet, QHostAddress::LocalHost, 4242);
    }

    bool m_enabled = false;
    bool m_connected = false;
    double m_yaw = 0.0;
    double m_pitch = 0.0;
    double m_roll = 0.0;

    bool m_calibrated = false;
    double m_ref1 = 0.0;
    double m_ref2 = 0.0;
    double m_ref3 = 0.0;

    QList<SampleTriple> m_calibSamples;

    bool m_hasReceivedSensorPacket = false;
    int m_activeVariant = 0;
    qint64 m_lastHtPacketTime = 0;
    QTimer *m_watchdogTimer = nullptr;

    bool m_oscBroadcast = true;
    int m_oscPort = 9000;
    bool m_freePieBroadcast = true;
    QUdpSocket *m_udpSocket = nullptr;
};
