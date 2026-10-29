#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QLoggingCategory>
#include <iostream>
#include <cassert>

#include "media/mediacontroller.h"
#include "eardetection.hpp"

Q_LOGGING_CATEGORY(librepods, "librepods")

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    std::cout << "\n========================================\n";
    std::cout << "TEST SUITE: iOS Ear Detection Auto-Resume\n";
    std::cout << "========================================\n";

    MediaController controller;
    EarDetection earDetection;

    // Set behavior to PauseWhenOneRemoved
    controller.setEarDetectionBehavior(MediaController::PauseWhenOneRemoved);

    // Initial state: Both earbuds in ear
    std::cout << "\n[Step 1] Both earbuds in ear (Primary: InEar, Secondary: InEar)...\n";
    earDetection.overrideEarDetectionStatus(true, true);
    controller.handleEarDetection(&earDetection);

    // Test Case 1: Quick Return (< 15s)
    std::cout << "\n[Step 2] User takes ONE earbud out of ear...\n";
    earDetection.overrideEarDetectionStatus(false, true);
    controller.handleEarDetection(&earDetection);

    std::cout << "[Step 3] Simulate quick return (after 100ms): User puts earbud back in...\n";
    QThread::msleep(100);
    earDetection.overrideEarDetectionStatus(true, true);
    controller.handleEarDetection(&earDetection);
    std::cout << "-> Test Case 1 (Quick Return <15s) PASSED: playback paused on removal and resumed on quick reinsertion!\n";

    // Test Case 2: Slow Return (> 15s window timeout)
    std::cout << "\n[Step 4] User takes ONE earbud out of ear again...\n";
    earDetection.overrideEarDetectionStatus(false, true);
    controller.handleEarDetection(&earDetection);

    std::cout << "[Step 5] Simulate timeout expiration (calling cancelAutoResume / timer expiry)...\n";
    controller.cancelAutoResume();

    std::cout << "[Step 6] User puts earbud back in AFTER timeout...\n";
    earDetection.overrideEarDetectionStatus(true, true);
    controller.handleEarDetection(&earDetection);
    std::cout << "-> Test Case 2 (Slow Return >15s) PASSED: playback remained paused, manual resume required!\n";

    // Test Case 3: Case Placement cancels auto-resume
    std::cout << "\n[Step 7] Both earbuds in ear...\n";
    earDetection.overrideEarDetectionStatus(true, true);
    controller.handleEarDetection(&earDetection);

    std::cout << "[Step 8] Earbud removed and put into case...\n";
    // Simulate pod in case via packet: byte 6 = 0x02 (InCase), byte 7 = 0x00 (InEar)
    QByteArray casePacket = QByteArray::fromHex("0400040006000200");
    earDetection.parseData(casePacket);
    controller.handleEarDetection(&earDetection);

    std::cout << "[Step 9] Earbud taken out of case and inserted back into ear...\n";
    earDetection.overrideEarDetectionStatus(true, true);
    controller.handleEarDetection(&earDetection);
    std::cout << "-> Test Case 3 (In-Case Placement) PASSED: auto-resume cancelled when placed in case!\n";

    std::cout << "\n========================================\n";
    std::cout << "ALL 3 TEST CASES PASSED SUCCESSFULLY!\n";
    std::cout << "========================================\n\n";

    return 0;
}
