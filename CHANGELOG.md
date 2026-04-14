# Changelog

All notable changes to this project will be documented in this file.

## [1.2.0] - 2026-04-14
### Added
- **Live Telemetry Interface**: New UI component that displays real-time hardware status (SRAM usage, active line, and delay countdowns) without annoying toast notifications for constant updates.
- **H8P V2 Telemetry Extension**: Introduced new Hex codes (`C1`, `D0`, `B3`) in the Arduino firmware to stream state data to the frontend.
- **UI/UX Overhaul**: Migrated to Tailwind CSS for a professional, technical-minimalist layout. Replaced the static terminal with a dynamic, non-intrusive Toast notification system.
- **Micro-interactions**: Added active button scaling and smooth transitions to improve the "Wow factor" and tactile feel of the web interface.

### Changed
- **Parser Intelligence**: The web interface now interpretively decodes hexadecimal packages into human-readable data (e.g., converting "C0" data into Slot ID and Step details).

## [1.1.0] - 2026-04-13
### Added
- **Safety Clamp**: Implemented a 50µs minimum interval limit for motor pulses to prevent MCU starvation and freezing.
- **Atomic Operations**: Critical state changes (e.g., STOP command) are now wrapped in `cli()`/`sei()` blocks for hardware thread safety.
- **Developer Environment**: Added `.vscode` configurations (`arduino.json`, `c_cpp_properties.json`) and `.clangd` for a seamless IntelliSense experience on macOS.
- **AI-Friendly Docs**: Added `llms.txt` and formatted project structure for better understanding by LLM agents.

### Fixed
- **Timer Race Conditions**: Ensured `TCCR1B` manipulations and flag clearing (`TIFR1`) are atomic and safe during motor initialization.
- **IntelliSense Errors**: Fixed missing header errors in VS Code by adding explicit `#include <Arduino.h>` and forward declarations in `stepcontrol.ino`.
- **Compiler Warnings**: Resolved several C++ assignment warnings in the serial parsing logic.

## [1.0.0] - 2026-04-12
### Added
- Initial release of the Stepper Control system.
- High-precision Timer1 (16-bit) motor control.
- Web Serial API dashboard for queue management.
- H8P Hexadecimal communication protocol.
