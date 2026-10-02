# Heaven Saw Synth 1.2.1 — Header text correction

The 1.2.0 version label ended at logical x=334 while the vertical divider was at x=330. This caused the line to cross the version text at every scale. Version 1.2.1 gives the label its own field ending at x=314, leaving 16 pixels before the divider. The brand uses Helvetica Neue with font tracking instead of inserted space characters. Header, About, and saved-state version strings now follow the build version.

## Verification

- The added pixel check failed against 1.2.0 with “Version text overlaps divider clearance”.
- All 24 checks passed after the correction.
- Rendered and visually inspected the header at 80%, 100%, and 120%, each at 1× and 2× pixel density, plus the complete interface.
- pluginval 1.0.4 strictness 8 passed against the final signed bundle at 44.1/48/88.2/96 kHz and 32–1024 sample buffers.
- Review of the complete changes found no remaining actionable defects.

Installation checks also caught an invalid bundle signature: JUCE wrote its plugin description after signing the bundle. The build now signs and verifies the completed bundle after that final resource is generated.

Audio processing and parameter identifiers are unchanged. The separate Steinberg validator remains unavailable. Restart or rescan the DAW and confirm v1.2.1.

Historical 1.2.0 records are in validation/v1.2.0. Current results are in validation/.
