#pragma once

#include <QMetaType>
#include <QHash>

namespace AirpodsTrayApp
{
    namespace Enums
    {
        Q_NAMESPACE

        enum class NoiseControlMode : quint8
        {
            Off = 0,
            NoiseCancellation = 1,
            Transparency = 2,
            Adaptive = 3,

            MinValue = Off,
            MaxValue = Adaptive,
        };
        Q_ENUM_NS(NoiseControlMode)

        enum class AirPodsModel
        {
            Unknown,
            AirPods1,
            AirPods2,
            AirPods3,
            AirPodsPro,
            AirPodsPro2Lightning,
            AirPodsPro2USBC,
            AirPodsMaxLightning,
            AirPodsMaxUSBC,
            AirPods4,
            AirPods4ANC,
            AirPods5,
            AirPods5WCC
        };
        Q_ENUM_NS(AirPodsModel)

        // Get model enum from model number
        inline AirPodsModel parseModelNumber(const QString &modelNumber)
        {
            // Model numbers taken from https://support.apple.com/en-us/109525
            QHash<QString, AirPodsModel> modelNumberMap = {
                {"A1523", AirPodsModel::AirPods1},
                {"A1722", AirPodsModel::AirPods1},
                {"A2032", AirPodsModel::AirPods2},
                {"A2031", AirPodsModel::AirPods2},
                {"A2084", AirPodsModel::AirPodsPro},
                {"A2083", AirPodsModel::AirPodsPro},
                {"A2096", AirPodsModel::AirPodsMaxLightning},
                {"A3184", AirPodsModel::AirPodsMaxUSBC},
                {"A2565", AirPodsModel::AirPods3},
                {"A2564", AirPodsModel::AirPods3},
                {"A3047", AirPodsModel::AirPodsPro2USBC},
                {"A3048", AirPodsModel::AirPodsPro2USBC},
                {"A3049", AirPodsModel::AirPodsPro2USBC},
                {"A3063", AirPodsModel::AirPodsPro2USBC},
                {"A3064", AirPodsModel::AirPodsPro2USBC},
                {"A3065", AirPodsModel::AirPodsPro2USBC},
                {"A2968", AirPodsModel::AirPodsPro2USBC},
                {"A2931", AirPodsModel::AirPodsPro2Lightning},
                {"A2699", AirPodsModel::AirPodsPro2Lightning},
                {"A2698", AirPodsModel::AirPodsPro2Lightning},
                {"A3053", AirPodsModel::AirPods4},
                {"A3050", AirPodsModel::AirPods4},
                {"A3054", AirPodsModel::AirPods4},
                {"A3056", AirPodsModel::AirPods4ANC},
                {"A3055", AirPodsModel::AirPods4ANC},
                {"A3057", AirPodsModel::AirPods4ANC},
                {"A3531", AirPodsModel::AirPods5},
                {"A3532", AirPodsModel::AirPods5},
                {"A3533", AirPodsModel::AirPods5},
                {"A3439", AirPodsModel::AirPods5WCC},
                {"A3440", AirPodsModel::AirPods5WCC},
                {"A3441", AirPodsModel::AirPods5WCC}};

            if (modelNumberMap.contains(modelNumber)) {
                return modelNumberMap.value(modelNumber);
            }
            if (modelNumber.startsWith("A306") || modelNumber.startsWith("A304") || modelNumber.startsWith("A269") || modelNumber.startsWith("A208")) {
                return AirPodsModel::AirPodsPro2USBC;
            }
            return AirPodsModel::Unknown;
        }

        inline QString getModelDisplayName(AirPodsModel model) {
            switch (model) {
                case AirPodsModel::AirPods1: return "AirPods (1st gen)";
                case AirPodsModel::AirPods2: return "AirPods (2nd gen)";
                case AirPodsModel::AirPods3: return "AirPods (3rd gen)";
                case AirPodsModel::AirPods4: return "AirPods 4";
                case AirPodsModel::AirPods4ANC: return "AirPods 4 (ANC)";
                case AirPodsModel::AirPods5: return "AirPods 5";
                case AirPodsModel::AirPods5WCC: return "AirPods 5 (Wireless Charging)";
                case AirPodsModel::AirPodsPro: return "AirPods Pro (1st gen)";
                case AirPodsModel::AirPodsPro2Lightning: return "AirPods Pro 2 (Lightning)";
                case AirPodsModel::AirPodsPro2USBC: return "AirPods Pro 2 (USB-C)";
                case AirPodsModel::AirPodsMaxLightning: return "AirPods Max (Lightning)";
                case AirPodsModel::AirPodsMaxUSBC: return "AirPods Max (USB-C)";
                default: return "AirPods Pro";
            }
        }

        // Return icons based on model
        inline QPair<QString, QString> getModelIcon(AirPodsModel model) {
            switch (model) {
                case AirPodsModel::AirPods1:
                case AirPodsModel::AirPods2:
                    return {"pod.png", "pod_case.png"};
                case AirPodsModel::AirPods3:
                    return {"pod3.png", "pod3_case.png"};
                case AirPodsModel::AirPods4:
                case AirPodsModel::AirPods4ANC:
                    return {"pod3.png", "pod4_case.png"};
                case AirPodsModel::AirPods5:
                case AirPodsModel::AirPods5WCC:
                case AirPodsModel::AirPodsPro:
                case AirPodsModel::AirPodsPro2Lightning:
                case AirPodsModel::AirPodsPro2USBC:
                    return {"podpro.png", "podpro_case.png"};
                case AirPodsModel::AirPodsMaxLightning:
                case AirPodsModel::AirPodsMaxUSBC:
                    return {"podmax.png", "max_case.png"};
                default:
                    return {"podpro.png", "podpro_case.png"}; // Default to AirPods Pro
            }
        }

        // TODO: Only used for parseEncryptedPacket for battery status. Is it possible to determine this
        // from the data in the packet rather than by model? i.e number of batteries
        inline bool isModelHeadset(AirPodsModel model) {
            switch (model) {
                case AirPodsModel::AirPodsMaxLightning:
                case AirPodsModel::AirPodsMaxUSBC:
                    return true;
                default:
                    return false;
            }
        }

    }
}
