#include <tinyNeoPixel_Static.h>

// =====================================================
// Focus Timer for ATtiny1614 + megaTinyCore v2.1
// by @marius.builds
// 06.04.2026
// -----------------------------------------------------
// Hardware:
//   - 10x WS2812B / NeoPixel LEDs
//   - 4 buttons
//   - 1 passive piezo buzzer
//
// States:
//   IDLE       = all LEDs are in idle color
//   RUNNING    = green progress bar fills over time
//   COMPLETED  = all LEDs pulse green for a grace period
//   ALERTING   = all LEDs pulse red + short beep every 2 seconds
//
// Button behavior:
//   - In IDLE: buttons start timers
//   - In RUNNING: any button cancels and resets
//   - In COMPLETED / ALERTING: any button acknowledges and resets
//
// Sounds:
//   - Button press: short click
//   - Cancel/reset: longer "beeeep"
//   - Timer finished: bee - beep - bee - beep
//   - Alerting: repeating short beep every 2 seconds
// =====================================================


// ---------- Hardware ----------
#define NUMLEDS     10
#define LED_PIN     PIN_PA3
#define BUZZER_PIN  PIN_PA6

#define BTN_1       PIN_PB0
#define BTN_2       PIN_PB1
#define BTN_3       PIN_PB2
#define BTN_4       PIN_PB3


// ---------- Timer values ----------
static const uint32_t TIMER_1_MINUTES = 10;   // button 1 - SW1
static const uint32_t TIMER_2_MINUTES = 20;   // button 2 - SW2
static const uint32_t TIMER_3_MINUTES = 30;   // button 3 - SW3
static const uint32_t TIMER_4_MINUTES = 60;   // button 4 - SW4

// For quick testing, you can temporarily use:
//static const uint32_t TIMER_1_MINUTES = 1;
//static const uint32_t TIMER_2_MINUTES = 2;
//static const uint32_t TIMER_3_MINUTES = 3;
//static const uint32_t TIMER_4_MINUTES = 5;


// ---------- Behavior ----------
static const uint32_t GRACE_MS      = 20000UL; // completed/grace period
static const uint16_t BEEP_EVERY_MS = 2000;    // alert beep interval


// ---------- LED brightness ----------
static const uint8_t IDLE_BRIGHTNESS = 20;   // low idle brightness
static const uint8_t RUN_BRIGHTNESS  = 120;
static const uint8_t PULSE_MIN       = 30;
static const uint8_t PULSE_MAX       = 160;


// ---------- Colors (R,G,B) ----------
static const uint8_t IDLE_R   = 74;
static const uint8_t IDLE_G   = 238;
static const uint8_t IDLE_B   = 227;

static const uint8_t RUN_R    = 0;
static const uint8_t RUN_G    = 140;
static const uint8_t RUN_B    = 0;

static const uint8_t ALERT_R  = 140;
static const uint8_t ALERT_G  = 0;
static const uint8_t ALERT_B  = 0;


// ---------- Buzzer tones ----------
static const uint16_t CLICK_FREQ      = 3500;
static const uint16_t CLICK_LEN_MS    = 40;

static const uint16_t CANCEL_FREQ     = 1800;
static const uint16_t CANCEL_LEN_MS   = 300;

// Finish alarm: bee - beep - bee - beep
static const uint16_t FINISH_FREQ_LONG   = 1800;
static const uint16_t FINISH_FREQ_SHORT  = 2600;
static const uint16_t FINISH_LONG_MS     = 260;
static const uint16_t FINISH_SHORT_MS    = 80;
static const uint16_t FINISH_GAP_MS      = 70;

static const uint16_t ALERT_FREQ      = 2200;
static const uint16_t ALERT_LEN_MS    = 120;


// ---------- NeoPixel buffer ----------
byte pixels[NUMLEDS * 3];
tinyNeoPixel leds(NUMLEDS, LED_PIN, NEO_GRB + NEO_KHZ800, pixels);


// ---------- State machine ----------
enum TimerState : uint8_t {
  STATE_IDLE = 0,
  STATE_RUNNING,
  STATE_COMPLETED,
  STATE_ALERTING
};

TimerState state = STATE_IDLE;


// ---------- Timer state variables ----------
uint32_t timerStartMs     = 0;
uint32_t timerDurationMs  = 0;
uint32_t completedStartMs = 0;


// ---------- Buzzer state ----------
uint32_t lastBeepMs  = 0;
uint32_t beepOffAtMs = 0;
bool beepActive      = false;


// ---------- Finish alarm sequencer ----------
bool finishSequenceActive = false;
uint8_t finishSequenceStep = 0;
uint32_t finishSequenceAtMs = 0;


// ---------- Button debounce ----------
static const uint16_t DEBOUNCE_MS = 25;
uint8_t  lastRawButtons = 0;
uint8_t  stableButtons  = 0;
uint32_t lastBounceMs   = 0;


// =====================================================
// Button helpers
// =====================================================

uint8_t readButtonsRaw() {
  uint8_t m = 0;

  if (!digitalRead(BTN_1)) m |= (1 << 0);
  if (!digitalRead(BTN_2)) m |= (1 << 1);
  if (!digitalRead(BTN_3)) m |= (1 << 2);
  if (!digitalRead(BTN_4)) m |= (1 << 3);

  return m;
}


uint8_t getButtonsPressed() {
  uint32_t now = millis();
  uint8_t raw = readButtonsRaw();

  if (raw != lastRawButtons) {
    lastRawButtons = raw;
    lastBounceMs = now;
  }

  if ((now - lastBounceMs) >= DEBOUNCE_MS) {
    uint8_t prevStable = stableButtons;
    stableButtons = raw;
    return (stableButtons & ~prevStable);
  }

  return 0;
}


// =====================================================
// LED helpers
// =====================================================

static inline uint8_t scale8(uint8_t v, uint8_t scale) {
  return (uint16_t(v) * uint16_t(scale)) >> 8;
}


void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  uint8_t rr = scale8(r, brightness);
  uint8_t gg = scale8(g, brightness);
  uint8_t bb = scale8(b, brightness);

  for (uint8_t i = 0; i < NUMLEDS; i++) {
    leds.setPixelColor(i, rr, gg, bb);
  }

  leds.show();
}


void showIdle() {
  setAll(IDLE_R, IDLE_G, IDLE_B, IDLE_BRIGHTNESS);
}


void showRunningProgress(uint32_t elapsed, uint32_t total) {
  if (elapsed > total) elapsed = total;

  uint8_t lit = (total == 0) ? 0 : (uint32_t(elapsed) * NUMLEDS) / total;

  for (uint8_t i = 0; i < NUMLEDS; i++) {
    leds.setPixelColor(i, 0, 0, 0);
  }

  uint8_t rr = scale8(RUN_R, RUN_BRIGHTNESS);
  uint8_t gg = scale8(RUN_G, RUN_BRIGHTNESS);
  uint8_t bb = scale8(RUN_B, RUN_BRIGHTNESS);

  for (uint8_t i = 0; i < lit && i < NUMLEDS; i++) {
    leds.setPixelColor(i, rr, gg, bb);
  }

  leds.show();
}


uint8_t triangleWave8(uint32_t tMs, uint16_t periodMs) {
  uint32_t p = periodMs;
  uint32_t x = tMs % p;
  uint32_t half = p / 2;

  if (half == 0) return 0;

  if (x < half) {
    return (uint32_t(x) * 255) / half;
  } else {
    return (uint32_t(p - x) * 255) / half;
  }
}


void showPulsingAll(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs) {
  uint8_t wave = triangleWave8(millis(), periodMs);
  uint8_t brightness = PULSE_MIN + (uint16_t(wave) * (PULSE_MAX - PULSE_MIN)) / 255;
  setAll(r, g, b, brightness);
}


void showCompletedPulse() {
  showPulsingAll(RUN_R, RUN_G, RUN_B, 1000);
}


void showAlertingPulse() {
  showPulsingAll(ALERT_R, ALERT_G, ALERT_B, 1000);
}


// =====================================================
// Buzzer helpers
// =====================================================

void buzzerStartBeep(uint16_t freq, uint16_t durationMs) {
  tone(BUZZER_PIN, freq);
  beepActive = true;
  beepOffAtMs = millis() + durationMs;
}


void buzzerUpdate() {
  if (beepActive && (int32_t)(millis() - beepOffAtMs) >= 0) {
    noTone(BUZZER_PIN);
    beepActive = false;
  }
}


void buzzerStop() {
  noTone(BUZZER_PIN);
  beepActive = false;
}


// =====================================================
// Finish alarm sequencer
// Pattern: bee - beep - bee - beep
// =====================================================

void startFinishAlarm() {
  finishSequenceActive = true;
  finishSequenceStep = 0;
  finishSequenceAtMs = millis();

  buzzerStartBeep(FINISH_FREQ_LONG, FINISH_LONG_MS);
}


void updateFinishAlarm() {
  if (!finishSequenceActive) return;

  uint32_t now = millis();

  switch (finishSequenceStep) {
    case 0:
      if (!beepActive) {
        finishSequenceStep = 1;
        finishSequenceAtMs = now;
      }
      break;

    case 1:
      if ((now - finishSequenceAtMs) >= FINISH_GAP_MS) {
        buzzerStartBeep(FINISH_FREQ_SHORT, FINISH_SHORT_MS);
        finishSequenceStep = 2;
      }
      break;

    case 2:
      if (!beepActive) {
        finishSequenceStep = 3;
        finishSequenceAtMs = now;
      }
      break;

    case 3:
      if ((now - finishSequenceAtMs) >= FINISH_GAP_MS) {
        buzzerStartBeep(FINISH_FREQ_LONG, FINISH_LONG_MS);
        finishSequenceStep = 4;
      }
      break;

    case 4:
      if (!beepActive) {
        finishSequenceStep = 5;
        finishSequenceAtMs = now;
      }
      break;

    case 5:
      if ((now - finishSequenceAtMs) >= FINISH_GAP_MS) {
        buzzerStartBeep(FINISH_FREQ_SHORT, FINISH_SHORT_MS);
        finishSequenceStep = 6;
      }
      break;

    case 6:
      if (!beepActive) {
        finishSequenceActive = false;
      }
      break;
  }
}


// =====================================================
// Timer state control
// =====================================================

void startTimerMinutes(uint32_t minutes) {
  timerDurationMs = minutes * 60UL * 1000UL;
  timerStartMs = millis();
  state = STATE_RUNNING;
}


void cancelAndReset() {
  finishSequenceActive = false;
  buzzerStartBeep(CANCEL_FREQ, CANCEL_LEN_MS);
  state = STATE_IDLE;
  showIdle();
}


void enterCompleted() {
  state = STATE_COMPLETED;
  completedStartMs = millis();
  startFinishAlarm();
}


void enterAlerting() {
  state = STATE_ALERTING;
  lastBeepMs = millis() - BEEP_EVERY_MS;
  finishSequenceActive = false;
  buzzerStop();
}


// =====================================================
// Setup
// =====================================================

void setup() {
  pinMode(LED_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerStop();

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT_PULLUP);

  leds.begin();
  delay(20);

  showIdle();

  // Optional startup beep
  tone(BUZZER_PIN, 2000);
  delay(80);
  noTone(BUZZER_PIN);
}


// =====================================================
// Main loop
// =====================================================

void loop() {
  uint32_t now = millis();
  uint8_t pressed = getButtonsPressed();

  if (pressed) {
    if (state == STATE_IDLE) {
      buzzerStartBeep(CLICK_FREQ, CLICK_LEN_MS);

      if (pressed & (1 << 0)) {
        startTimerMinutes(TIMER_1_MINUTES);
      }
      else if (pressed & (1 << 1)) {
        startTimerMinutes(TIMER_2_MINUTES);
      }
      else if (pressed & (1 << 2)) {
        startTimerMinutes(TIMER_3_MINUTES);
      }
      else if (pressed & (1 << 3)) {
        startTimerMinutes(TIMER_4_MINUTES);
      }
    } else {
      cancelAndReset();
    }
  }

  switch (state) {
    case STATE_IDLE:
      break;

    case STATE_RUNNING: {
      uint32_t elapsed = now - timerStartMs;
      showRunningProgress(elapsed, timerDurationMs);

      if (elapsed >= timerDurationMs) {
        enterCompleted();
      }
    } break;

    case STATE_COMPLETED:
      showCompletedPulse();

      if ((now - completedStartMs) >= GRACE_MS) {
        enterAlerting();
      }
      break;

    case STATE_ALERTING:
      showAlertingPulse();

      if ((now - lastBeepMs) >= BEEP_EVERY_MS) {
        lastBeepMs = now;
        buzzerStartBeep(ALERT_FREQ, ALERT_LEN_MS);
      }
      break;
  }

  buzzerUpdate();
  updateFinishAlarm();

  delay(10);
}