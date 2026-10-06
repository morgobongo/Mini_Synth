# Mini Synth (Oskitone Scout ESP32 Port)

## 1. Introduction

This project originates from the [Oskitone Scout](https://github.com/oskitone/scout), a fantastic open-source 3D-printed synthesizer. 

The original Scout has been heavily modified and ported to be powered by an **ESP32 microcontroller** instead of its original analog-style hardware. Migrating to the ESP32 required a complete redesign of the PCB from scratch and extensive structural modifications to the 3D-printed enclosure.

Furthermore, the audio synthesis is now entirely digital, built on top of the powerful **[Mozzi](https://sensorium.github.io/Mozzi/)** audio synthesis framework for Arduino/ESP32, which allows for advanced digital signal processing, polyphony, and complex modulations directly on the microcontroller.

---

## 2. Synth Engine & Presets

The custom C++ firmware (`ESP32/src/SynthEngine.cpp`) transforms the ESP32 into a highly capable digital synthesizer.

### Features
- **4-Voice Polyphony**: Play up to 4 notes simultaneously. The engine automatically handles voice allocation and note stealing.
- **Monophonic Mode**: Classic last-note priority monophonic mode with legato tolerance (prevents envelopes from re-triggering if playing fast).
- **ADSR Envelopes**: Each voice features a fully independent Attack-Decay-Sustain-Release envelope.
- **Portamento / Glide**: Smoothly glide between notes. In polyphonic mode, glide can be assigned to all voices, only the lowest note, or only the highest note (`GLIDE_ALL`, `GLIDE_LOWEST`, `GLIDE_HIGHEST`).
- **Modulation (LFOs)**: Two independent Low-Frequency Oscillators control **Vibrato** (pitch modulation) and **Tremolo** (amplitude modulation).
- **Dynamic Gain Compensation (Ducking)**: The engine automatically scales the master volume depending on how many voices are actively playing to prevent digital clipping.
- **Waveforms**: Built-in support for Square (anti-aliased), Sawtooth, and Sine waves.

### Creating Presets (`Presets.cpp`)
Presets are incredibly easy to configure. All parameters in `Presets.cpp` are defined using intuitive **0 to 100%** values (which are then mapped to their respective milliseconds, depth limits, or rates in the background). 

Here is how a preset is structured:
```cpp
Preset SOUND_2 = makePreset(
  4,                            // numVoices (Polyphonic)
  square_wave,                  // waveform
  0,                            // glidePct
  2,                            // attackPct (2% = ~40ms)
  2,                            // decayPct (2% = ~40ms)
  100,                          // sustainPct (100% = full volume)
  1,                            // releasePct (1% = ~50ms)
  50,                           // vibratoDepthPct
  30,                           // vibratoSpeedPct
  0,                            // tremoloDepthPct
  20,                           // tremoloSpeedPct
  GLIDE_LOWEST,                 // polyGlideMode
  31                            // masterVolumePct (~31% to prevent clipping)
);
```
*You can cycle through the 4 built-in presets using the physical "Mode (M)" button on the synth.*

---

## 3. Bluetooth MIDI Controller Mode (BLE MIDI)

The synthesizer can act as a Bluetooth MIDI controller for your Mac, PC, or iOS device. While the MIDI data connection is wireless via Bluetooth, please note that **the synthesizer must remain plugged into a USB-C power source** to operate. This mode runs completely independently of the built-in sound engine.

**How to use:**
1. **Activate MIDI Mode:** Hold down both the **Mode (M)** and **Octave (O)** buttons simultaneously for 2 seconds.
2. **Visual Feedback:** The blue LED on the ESP32 will turn ON and stay solid, confirming you are in MIDI Mode. *(Note: The standard ESP32 flashing during normal synth play has been disabled for a cleaner look).* The internal Mozzi synthesizer is automatically muted.
3. **Connection:** 
   - On your device, open your Bluetooth settings or your DAW's MIDI settings (e.g., *Audio MIDI Setup* on Mac).
   - Search for a Bluetooth MIDI device named **"Mini Synth"** and connect to it. No additional drivers are required.
4. **Usage:** Play the keys! The physical `Octave` button remains functional and will dynamically shift the MIDI notes being sent (`NoteOn` / `NoteOff`).
5. **Deactivate:** Hold the **Mode** and **Octave** buttons again for 2 seconds. The LED will turn off, the MIDI mode will exit, and the internal synthesizer will be reactivated.

---

## 4. Roadmap & Upcoming Features

- [ ] **LiPo Battery Integration**: Replace the original AAA battery holder with a rechargeable 900mAh LiPo battery (603048).
- [ ] **USB-C Charging**: Integrate a TP4056 charging module and a 5V Boost converter to safely charge the LiPo and power the ESP32 via the USB-C port.
- [ ] **Physical Power Switch**: Add a DPDT toggle switch on the back of the enclosure to properly cut off both the battery and the USB power paths.

---

## 5. Differences vs. Original Oskitone Scout

Because the project relies on a completely different PCB, significant modifications were made to the original OpenSCAD 3D models to accommodate the new hardware and aesthetic choices.

### Hardware & Enclosure Modifications
- **Overall Case Size**: The PCB length (`PCB_LENGTH`) was increased by `7.62` mm (3 perfboard holes), lengthening the entire device towards the back to accommodate the ESP32 footprint.
- **Activation of Hidden Elements**: The original speaker and LED components were uncommented and integrated into the enclosure.
- **Addition of New Buttons**: Two new cylinders were modeled on top of the enclosure for the **Octave** and **Mode** features, offset from the volume knob.
- **USB-C Port Hole**: The former power switch hole was repurposed and repositioned for the ESP32 USB-C programming port (Z position moved up by 2mm, Y position shifted by 2.54mm).
- **Potentiometer Position**: Shifted away from the keys (towards the back) by one standard perfboard hole grid unit (2.54mm).
- **LED Adjustments**: The LED barrel inside the enclosure was shortened by 3mm. An optional 3-slit mini grill was modeled above the LED.
- **Support Clearances**: The Z position of the back corner reinforcements was increased by 5-8mm to provide more vertical clearance over the PCB.

### PCB Fixtures Modifications
- **Back Support Pillars**: Repositioned to fall exactly below the new potentiometer position and symmetrically on the other side.
- **Button Support Rail**: Shifted on the Y-axis to avoid screw holes, and width increased from 3mm to 5mm.
- **Support Beams & Posts**: Removed the unused front-left corner reinforcement beam and reactivated the original PCB support posts next to the screw holes.

### Labels, Engravings, and Branding
- **Custom Branding**: The original text branding ("SCOUT") on the enclosure and the maker logo were permanently removed from the code to make way for a clean, brand-less design.
- **Volume Label**: The "VOL" label was made hollow (placard removed) and the text size increased to 5 for better 3D printing legibility. It was also shifted slightly on the Y-axis.
- **New Labels**: Added "M" (Mode) and "O" (Octave) labels under their respective new button holes.
- **Engraving Logic Fix**: Modified `enclosure_engraving.scad` to correctly output geometry when `placard` is false instead of returning an empty geometry.

### OpenSCAD Code Refactoring & Cleanup
- **Grid Pitch Abstraction**: The hardcoded `2.54` (perfboard hole spacing in mm) was abstracted into a global `GRID_PITCH` variable. All positions for buttons, LEDs, and USB ports in the enclosure and PCB files were refactored to use this semantic variable.
- **Removal of Unused Legacy Code**: Features from the original Scout that were deactivated or commented out for STL generation were entirely removed to keep the codebase clean. This includes the UART header footprint, the Pencil Stand module, the physical Switch Clutch mechanism, and legacy side/bottom engravings.

---

## 6. Other Info & Build Scripts

- **Custom 3D Printed Buttons**: A custom "top hat" button (`openscad/custom_button.scad`) was designed for the Mode and Octave switches (7.6mm top diameter, 12mm base). The hole underneath is 4mm wide to tightly fit a standard `6x6x7 mm` tactile switch.
- **Button Adapters**: A utility (`openscad/button_adapter.scad`) is included to print an array of `4x4x1 mm` square spacers, used to adapt standard `6x6x5 mm` switches to the required height if you don't have 7mm switches on hand.
- **Preview Configuration**: Adjusted global visibility parameters in `scout.scad` for cleaner visual debugging.
- **STL Generation Script**: The `make_stls.sh` script was updated to name output directories using the current date and time (`YYYY-MM-DD_HHhMMmSSs`) rather than Git commit hashes, making iteration tracking easier during development.
