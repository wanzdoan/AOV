#pragma once
// =============================================================================
//  AOV Zygisk — Runtime Diagnostics (one-shot, logcat only)
//  Runs inside the game process: enumerates libunity.so dynamic symbols and
//  reflects Unity player classes to find the REAL touch-input entry point.
//  Read results with: adb logcat -s aov_zygisk -d | grep diag
// =============================================================================
namespace Diag {
    void run();
}
