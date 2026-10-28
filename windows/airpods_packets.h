// airpods_packets.h
#ifndef AIRPODS_PACKETS_H
#define AIRPODS_PACKETS_H

#include <QByteArray>
#include <optional>
#include <climits>

#include "enums.h"
#include "BasicControlCommand.hpp"

namespace AirPodsPackets
{
    // Noise Control Mode Packets
    namespace NoiseControl
    {
        using NoiseControlMode = AirpodsTrayApp::Enums::NoiseControlMode;
        static const QByteArray HEADER = ControlCommand::HEADER + 0x0D;
        static const QByteArray OFF = ControlCommand::createCommand(0x0D, 0x01);
        static const QByteArray NOISE_CANCELLATION = ControlCommand::createCommand(0x0D, 0x02);
        static const QByteArray TRANSPARENCY = ControlCommand::createCommand(0x0D, 0x03);
        static const QByteArray ADAPTIVE = ControlCommand::createCommand(0x0D, 0x04);

        static const QByteArray getPacketForMode(AirpodsTrayApp::Enums::NoiseControlMode mode)
        {
            switch (mode)
            {
            case NoiseControlMode::Off:
                return OFF;
            case NoiseControlMode::NoiseCancellation:
                return NOISE_CANCELLATION;
            case NoiseControlMode::Transparency:
                return TRANSPARENCY;
            case NoiseControlMode::Adaptive:
                return ADAPTIVE;
            default:
                return QByteArray();
            }
        }

        inline std::optional<NoiseControlMode> parseMode(const QByteArray &data)
        {
            char mode = ControlCommand::parseActive(data).value_or(CHAR_MAX) - 1;
            if (mode < static_cast<quint8>(NoiseControlMode::MinValue) ||
                mode > static_cast<quint8>(NoiseControlMode::MaxValue))
            {
                return std::nullopt;
            }
            return static_cast<NoiseControlMode>(mode);
        }
    }

    // One Bud ANC Mode
    namespace OneBudANCMode
    {
        using Type = BasicControlCommand<0x1B>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }
    }

    // Volume Swipe (partial - still needs custom interval function)
    namespace VolumeSwipe
    {
        using Type = BasicControlCommand<0x25>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }

        // Keep custom interval function
        static QByteArray getIntervalPacket(quint8 interval)
        {
            return ControlCommand::createCommand(0x23, interval);
        }
    }

    // Adaptive Volume Config
    namespace AdaptiveVolume
    {
        using Type = BasicControlCommand<0x26>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }
    }

    // Conversational Awareness
    namespace ConversationalAwareness
    {
        using Type = BasicControlCommand<0x28>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        static const QByteArray DATA_HEADER = QByteArray::fromHex("040004004B00020001");
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }
    }

    // Hearing Assist
    namespace HearingAssist
    {
        using Type = BasicControlCommand<0x33>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }
    }

    // Hearing Aid
    namespace HearingAid
    {
        static const QByteArray HEADER = ControlCommand::HEADER + static_cast<char>(0x2C);
        static const QByteArray ENABLED = ControlCommand::createCommand(0x2C, 0x01, 0x01);
        static const QByteArray DISABLED = ControlCommand::createCommand(0x2C, 0x02, 0x02);

        inline std::optional<bool> parseState(const QByteArray &data)
        {
            if (!data.startsWith(HEADER) || data.size() < HEADER.size() + 2)
                return std::nullopt;

            QByteArray value = data.mid(HEADER.size(), 2);
            if (value.size() != 2)
                return std::nullopt;

            char b1 = value.at(0);
            char b2 = value.at(1);

            if (b1 == 0x01 && b2 == 0x01)
                return true;
            if (b1 == 0x02 || b2 == 0x02)
                return false;

            return std::nullopt;
        }
    }

    // Allow Off Option
    namespace AllowOffOption
    {
        using Type = BasicControlCommand<0x34>;
        static const QByteArray ENABLED = Type::ENABLED;
        static const QByteArray DISABLED = Type::DISABLED;
        static const QByteArray HEADER = Type::HEADER;
        inline std::optional<bool> parseState(const QByteArray &data) { return Type::parseState(data); }
    }

    // Connection Packets
    namespace Connection
    {
        static const QByteArray HANDSHAKE = QByteArray::fromHex("00000400010002000000000000000000");
        static const QByteArray SET_SPECIFIC_FEATURES = QByteArray::fromHex("040004004d00d700000000000000");
        static const QByteArray REQUEST_NOTIFICATIONS = QByteArray::fromHex("040004000f00ffffffffff");
        static const QByteArray AIRPODS_DISCONNECTED = QByteArray::fromHex("00010000");
    }

    // Phone Communication Packets
    namespace Phone
    {
        static const QByteArray NOTIFICATION = QByteArray::fromHex("00040001");
        static const QByteArray CONNECTED = QByteArray::fromHex("00010001");
        static const QByteArray DISCONNECTED = QByteArray::fromHex("00010000");
        static const QByteArray STATUS_REQUEST = QByteArray::fromHex("00020003");
        static const QByteArray DISCONNECT_REQUEST = QByteArray::fromHex("00020000");
    }

    // Adaptive Noise Packets
    namespace AdaptiveNoise
    {
        const QByteArray HEADER = QByteArray::fromHex("0400040009002E");

        inline QByteArray getPacket(int level)
        {
            return HEADER + static_cast<char>(level) + QByteArray::fromHex("000000");
        }
    }

    namespace Rename
    {
        static QByteArray getPacket(const QString &newName)
        {
            QByteArray nameBytes = newName.toUtf8();                   // Convert name to UTF-8
            quint8 size = static_cast<char>(nameBytes.size());         // Name length (1 byte)
            QByteArray packet = QByteArray::fromHex("040004001A0001"); // Header
            packet.append(size);                                       // Append size byte
            packet.append('\0');                                       // Append null byte
            packet.append(nameBytes);                                  // Append name bytes
            return packet;
        }
    }

    namespace MagicPairing {
        static const QByteArray REQUEST_MAGIC_CLOUD_KEYS = QByteArray::fromHex("0400040030000500");
        static const QByteArray MAGIC_CLOUD_KEYS_HEADER = QByteArray::fromHex("04000400310002");

        struct MagicCloudKeys {
            QByteArray magicAccIRK;      // 16 bytes
            QByteArray magicAccEncKey;    // 16 bytes
        };

        inline MagicCloudKeys parseMagicCloudKeysPacket(const QByteArray &data)
        {
            MagicCloudKeys keys;

            if (data.size() < 47 || !data.startsWith(MAGIC_CLOUD_KEYS_HEADER))
            {
                return keys;
            }

            int index = MAGIC_CLOUD_KEYS_HEADER.size();

            // First TLV block (MagicAccIRK)
            if (static_cast<quint8>(data.at(index)) != 0x01)
                return keys;
            index += 1;

            quint16 len1 = (static_cast<quint8>(data.at(index)) << 8) | static_cast<quint8>(data.at(index + 1));
            if (len1 != 16)
                return keys;
            index += 3; // Skip length (2 bytes) and reserved byte (1 byte)

            keys.magicAccIRK = data.mid(index, 16);
            index += 16;

            // Second TLV block (MagicAccEncKey)
            if (static_cast<quint8>(data.at(index)) != 0x04)
                return keys;
            index += 1;

            quint16 len2 = (static_cast<quint8>(data.at(index)) << 8) | static_cast<quint8>(data.at(index + 1));
            if (len2 != 16)
                return keys;
            index += 3; // Skip length (2 bytes) and reserved byte (1 byte)

            keys.magicAccEncKey = data.mid(index, 16);

            return keys;
        }
    }

    // Spatial Audio / Head Tracking
    namespace SpatialAudio
    {
        // START_ALT is confirmed on hardware to stream true 70+ byte motion frames
        static const QByteArray START_TRACKING_ALT = QByteArray::fromHex("040004001700000010000f000873420b081010021a0501409c0000");
        // START_DEF is the alternative start packet
        static const QByteArray START_TRACKING_DEF = QByteArray::fromHex("04000400170000001000100008a102420b080e10021a0501409c0000");
        static const QByteArray START_TRACKING     = START_TRACKING_ALT;
        static const QByteArray STOP_TRACKING      = QByteArray::fromHex("040004001700000010001100087e1002420b084e10021a050100000000");
        static const QByteArray HEADER             = QByteArray::fromHex("040004001700");
        static const QByteArray HT_PREFIX          = QByteArray::fromHex("04000400170000001000");

        struct RawSample {
            qint16 o1 = 0;
            qint16 o2 = 0;
            qint16 o3 = 0;
            bool valid = false;
        };

        inline bool isSensorPacket(const QByteArray &data)
        {
            if (data.size() < 70) return false;
            if (!data.startsWith(HT_PREFIX)) return false;
            quint8 tag = static_cast<quint8>(data.at(10));
            quint8 sub = static_cast<quint8>(data.at(11));
            return (tag == 0x44 || tag == 0x45) && sub == 0x00;
        }

        inline RawSample parseRawSample(const QByteArray &data)
        {
            RawSample s;
            if (!isSensorPacket(data)) {
                return s;
            }
            s.o1 = static_cast<qint16>(static_cast<quint8>(data[43]) | (static_cast<quint8>(data[44]) << 8));
            s.o2 = static_cast<qint16>(static_cast<quint8>(data[45]) | (static_cast<quint8>(data[46]) << 8));
            s.o3 = static_cast<qint16>(static_cast<quint8>(data[47]) | (static_cast<quint8>(data[48]) << 8));
            s.valid = true;
            return s;
        }

        struct Orientation {
            double yaw = 0.0;
            double pitch = 0.0;
            double roll = 0.0;
            bool valid = false;
        };

        inline Orientation parseOrientation(const QByteArray &data)
        {
            Orientation ori;
            RawSample s = parseRawSample(data);
            if (!s.valid) {
                return ori;
            }
            ori.pitch = qBound(-90.0, ((s.o2 + s.o3) / 2.0 / 32000.0) * 180.0, 90.0);
            ori.yaw   = qBound(-180.0, ((s.o2 - s.o3) / 2.0 / 32000.0) * 180.0, 180.0);
            ori.roll  = qBound(-180.0, (((s.o1 - (s.o2 + s.o3) / 2.0) / 2.0) / 32000.0) * 180.0, 180.0);
            ori.valid = true;
            return ori;
        }
    }

    // Parsing Headers
    namespace Parse
    {
        static const QByteArray EAR_DETECTION = QByteArray::fromHex("040004000600");
        static const QByteArray BATTERY_STATUS = QByteArray::fromHex("040004000400");
        static const QByteArray METADATA = QByteArray::fromHex("040004001d");
        static const QByteArray HANDSHAKE_ACK = QByteArray::fromHex("01000400");
        static const QByteArray FEATURES_ACK = QByteArray::fromHex("040004002b00"); // Note: Only tested with airpods pro 2
        static const QByteArray SPATIAL_AUDIO = QByteArray::fromHex("040004001700");
    }
}

#endif // AIRPODS_PACKETS_H
