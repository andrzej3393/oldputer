# Agents

This repository targets Arduino/ESP32 firmware development.

## Language server

- Use the Arduino Language Server (not clangd).

## Build + tooling

- Preferred build tool: `arduino-cli`.
- Default target settings are in `sketch.yaml` (profile: `weather_station`).
- For manual builds, use the huge app partition profile shown in `README.md`.

## Code conventions

- Keep changes small and incremental; prefer refactors that preserve behavior.
- Centralize board-level constants in `src/core/app_config.h`.
- Avoid introducing Unicode unless the file already uses it.

## UX/display

- For ePaper updates, minimize full refreshes where possible to reduce blinking.
- Respect the invert display setting when rendering UI screens.
