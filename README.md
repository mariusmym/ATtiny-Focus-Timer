# ATTiny Focus Timer

A minimal, distraction-free focus timer built around the **ATtiny1614** microcontroller. Press a button, pick a duration, and get back to work — 10 WS2812B LEDs show your progress as a filling bar, then gently pulse and beep when time is up.

Created by **@marius.builds**

---

## How It Works

The timer cycles through four states:

| State | LEDs | Sound |
|---|---|---|
| **Idle** | All LEDs glow white at low brightness | — |
| **Running** | Green progress bar fills one LED at a time | — |
| **Completed** | All LEDs pulse green (20 s grace period) | Finish melody (bee-beep-bee-beep) |
| **Alerting** | All LEDs pulse red | Short beep every 2 seconds |

**Buttons:**
- **In Idle** — each of the 4 buttons starts a preset timer (10 / 20 / 30 / 60 min).
- **In Running** — any button cancels and resets.
- **In Completed or Alerting** — any button acknowledges and returns to Idle.

## Hardware

| Component | Qty | Notes |
|---|---|---|
| ATtiny1614 | 1 | Programmed via megaTinyCore + UPDI |
| WS2812B / NeoPixel LEDs | 10 | Directly driven from PA3 |
| Tactile push buttons | 4 | Connected to PB0–PB3, using internal pull-ups |
| Passive piezo buzzer | 1 | Driven from PA6 via `tone()` |

### Pin Mapping

```
PA3  →  NeoPixel data
PA6  →  Piezo buzzer
PB0  →  Button 1  (10 min)
PB1  →  Button 2  (20 min)
PB2  →  Button 3  (30 min)
PB3  →  Button 4  (60 min)
```

## Software

### Dependencies

- **[megaTinyCore](https://github.com/SpenceKonde/megaTinyCore)** — Arduino core for modern tinyAVR (ATtiny x04/x14/x24 series)
- **tinyNeoPixel_Static** — included with megaTinyCore; lightweight NeoPixel driver for tinyAVR

### Building & Flashing

1. Install **megaTinyCore** via the Arduino IDE Boards Manager.
2. Select **ATtiny1614** as the target board.
3. Set the programmer to **SerialUPDI** (or whichever UPDI method you use).
4. Open `ATTiny-FocusTimer-V2_1.ino` and upload.

### Configuration

All tunable parameters live at the top of the sketch — no need to dig through logic:

```c
// Timer durations (minutes)
TIMER_1_MINUTES  = 10;
TIMER_2_MINUTES  = 20;
TIMER_3_MINUTES  = 30;
TIMER_4_MINUTES  = 60;

// Behavior
GRACE_MS         = 20000;   // completed → alerting delay (ms)
BEEP_EVERY_MS    = 2000;    // alert beep interval (ms)

// LED brightness (0–255)
IDLE_BRIGHTNESS  = 20;
RUN_BRIGHTNESS   = 120;
PULSE_MIN        = 30;
PULSE_MAX        = 160;
```

For quick bench testing, uncomment the shorter timer values (1 / 2 / 3 / 5 min) already provided in the sketch.

## State Machine

```
         ┌──────────────────────────────────┐
         │           any button             │
         ▼                                  │
      ┌──────┐   button press   ┌─────────┐ │
      │ IDLE │ ───────────────▶│ RUNNING  │ │
      └──────┘                  └────┬────┘ │
         ▲                          │       │
         │ any button        timer done     │
         │                          │       │
    ┌────┴─────┐  grace timeout ┌───▼───────┤
    │ ALERTING │◀──────────────│ COMPLETED  │
    └──────────┘               └────────────┘
```

## Enclosure

The project includes a 3D-printed case designed to house the PCB, LEDs, and buttons in a compact, desk-friendly form factor. The STL files are located in the `case/` folder of this repository. They were designed for FDM printing with no supports needed.

If you'd like to tweak the design, the original CAD source file is included as well.

## PCB

Full fabrication files are provided in the `pcb/` folder so you can order your own boards:

- **Gerber files** — ready to upload to JLCPCB, PCBWay, or any other fab house.
- **BOM (Bill of Materials)** — a spreadsheet listing every component with values, footprints, and reference designators.
- **Schematic**

## License

Feel free to use, modify, and share. Attribution appreciated.
