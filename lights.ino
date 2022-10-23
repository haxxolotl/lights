#include <FastLED.h>

int brightness = 8;
int counter = 0;
int current_program = 0;
int program_parameter_x = 0;
int program_parameter_y = 0;

#pragma region PhysicalSegment

struct PhysicalSegment {
  int count;
  CRGB *leds;
  CLEDController *controller;
};

#define num_physical_segments 3
#define num_strip_segment_leds 100
#define num_dual_segment_leds 356
#define num_fairy_segment_leds 400
CRGB strip_segment_leds[num_strip_segment_leds];
CRGB dual_segment_leds[dual_segment_leds];
CRGB fairy_segment_leds[num_fairy_segment_leds];

PhysicalSegment strip_segment = {num_strip_segment_leds, strip_segment_leds, &FastLED.addLeds<WS2811, 7, BRG>(strip_segment_leds, num_strip_segment_leds)};   // strip 1*100
PhysicalSegment dual_segment = {num_dual_segment_leds, dual_segment_leds, &FastLED.addLeds<WS2811, 8, BRG>(dual_segment_leds, num_dual_segment_leds)};        // fairy 2x50 + strip 2*100 + 56
PhysicalSegment fairy_segment = {num_fairy_segment_leds, fairy_segment_leds, &FastLED.addLeds<WS2811, 10, RGB>(fairy_segment_leds, num_fairy_segment_leds)};  // fairy 8*50
PhysicalSegment physical_segments[num_physical_segments] = {strip_segment, dual_segment, fairy_segment};

#pragma endregion PhysicalSegment

#pragma region LogicalSegment

// these map to some region of a physical segment, these are elemental and do not overlap
struct LogicalSegment {
  PhysicalSegment *physical_segment;
  int start;
  int end;
  bool pixel_shift;
};

LogicalSegment strip_segment_up = {&strip_segment, 0, 70, false};
LogicalSegment strip_segment_over = {&strip_segment, 70, 100, false};
LogicalSegment dual_segment_up = {&dual_segment, 0, 32, true};
LogicalSegment dual_segment_window = {&dual_segment, 32, 100, true};
LogicalSegment dual_segment_right_corner = {&dual_segment, 100, 117, false};
LogicalSegment dual_segment_back_ceiling = {&dual_segment, 117, 207, false};
LogicalSegment dual_segment_left_ceiling = {&dual_segment, 207, 287, false};
LogicalSegment dual_segment_front_ceiling = {&dual_segment, 287, 356, false};
LogicalSegment fairy_segment_up = {&fairy_segment, 0, 26, false};
LogicalSegment fairy_segment_1 = {&fairy_segment, 26, 76, false};
LogicalSegment fairy_segment_2 = {&fairy_segment, 76, 127, false};
LogicalSegment fairy_segment_3 = {&fairy_segment, 127, 177, false};
LogicalSegment fairy_segment_4 = {&fairy_segment, 227, 277, false};
LogicalSegment fairy_segment_5 = {&fairy_segment, 327, 377, false};
LogicalSegment fairy_segment_6 = {&fairy_segment, 377, 400, false};

#pragma endregion LogicalSegment

#pragma region LogicalSegmentGroup

// groups can have strip combination that overlap other groups
struct LogicalSegmentGroup {
  LogicalSegment **logical_segments;
  int segments;
};
LogicalSegment *strip_segments[6] = {&strip_segment_up, &strip_segment_over, &dual_segment_right_corner, &dual_segment_back_ceiling, &dual_segment_left_ceiling, &dual_segment_front_ceiling};
LogicalSegment *strip_ceiling_segments[5] = {&strip_segment_over, &dual_segment_right_corner, &dual_segment_back_ceiling, &dual_segment_left_ceiling, &dual_segment_front_ceiling};
LogicalSegment *fairy_segments[8] = {&dual_segment_up, &dual_segment_window, &fairy_segment_1, &fairy_segment_2, &fairy_segment_3, &fairy_segment_4, &fairy_segment_5, &fairy_segment_6};
LogicalSegment *fairy_ceiling_segments[6] = {&fairy_segment_1, &fairy_segment_2, &fairy_segment_3, &fairy_segment_4, &fairy_segment_5, &fairy_segment_6};
LogicalSegment *all_segments[15] = {&strip_segment_up,
                                    &strip_segment_over,
                                    &dual_segment_up,
                                    &dual_segment_window,
                                    &dual_segment_right_corner,
                                    &dual_segment_back_ceiling,
                                    &dual_segment_left_ceiling,
                                    &dual_segment_front_ceiling,
                                    &fairy_segment_up,
                                    &fairy_segment_1,
                                    &fairy_segment_2,
                                    &fairy_segment_3,
                                    &fairy_segment_4,
                                    &fairy_segment_5,
                                    &fairy_segment_6};
LogicalSegmentGroup strip_group = {strip_segments, 6};
LogicalSegmentGroup strip_ceiling_group = {strip_segments, 5};
LogicalSegmentGroup fairy_group = {fairy_segments, 8};
LogicalSegmentGroup fairy_ceiling_group = {fairy_ceiling_segments, 6};
LogicalSegmentGroup all_group = {all_segments, 15};

#pragma endregion LogicalSegmentGroup

#pragma region LightProgram

typedef void (*LightFunction)(CRGB *leds, int count);
typedef void (*SequenceGroupSideEffect)();

// A program is looping sequence groups, each group is made of a list of sequences
// a sequence is a light function applied to some range of logical RGB

// apply this light function to either a segment or a group, other one is nullptr
// both being nullptr indicate all lights
struct LightSequence {
  LightFunction light_function;
  LogicalSegment *logical_segment;
  LogicalSegmentGroup *logical_segment_group;
};

struct LightSequenceGroup {
  LightSequence *sequences;
  int num_sequences;
  int duration;  // ms
  SequenceGroupSideEffect side_effect;
};

struct LightProgram {
  LightSequenceGroup *light_sequence_groups;
  int num_groups;
};

void black(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Black); }
void rainbow(CRGB *leds, int count) { fill_rainbow(leds, count, counter, program_parameter_x * 2 % 10); }

LightSequence reset_sequence = {&black, nullptr, nullptr};
LightSequence rainbow_sequence = {&rainbow, nullptr, nullptr};
LightSequenceGroup reset_group = {&reset_sequence, 1, 1000, []() {}};
LightSequenceGroup rainbow_group = {&rainbow_sequence, 1, 1000, []() { counter += program_parameter_y; }};

#define num_programs 5
LightProgram programs[num_programs] = {
    {&reset_group, 1},
    {&rainbow_group, 1},
};

#pragma endregion LightProgram

#pragma region Button

typedef void (*ButtonCallback)();
struct Button {
  int pin;
  bool pressed;
  ButtonCallback callback;
};

#define num_buttons 4
Button buttons[4] = {
    {3, false,
     []() {
       // next brightness setting
       if (brightness == 0)
         brightness = 1;
       else
         brightness *= 2;
       if (brightness > 128) brightness = 0;
     }},
    {2, false,
     []() {  // next lighting program
       current_program = (num_programs + current_program + 1) % num_programs;
       program_parameter_x = 0;
       program_parameter_y = 0;
       counter = 0;
     }},
    {1, false, []() { program_parameter_x++; }},
    {0, false, []() { program_parameter_y++; }},
};

#pragma endregion Button

void setup() {
  delay(2000);  // sanity delay
  for (int i = 0; i < num_buttons; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
}

void tick() {
  LightProgram &program = programs[current_program];
  LightSequenceGroup *group = nullptr;
  int program_time = 0;
  for (int i = 0; i < program.num_groups; i++) {
    program_time += program.light_sequence_groups[i].duration;
    if (counter <= program_time) {
      group = &program.light_sequence_groups[i];
    }
  }
  group->side_effect();
  for (int i = 0; i < group->num_sequences; i++) {
    LightSequence &sequence = group->sequences[i];
    if (sequence.logical_segment != nullptr) {
    } else if (sequence.logical_segment_group != nullptr) {
    } else {
      for (int i = 0; i < num_physical_segments; i++) {
        sequence.light_function(physical_segments->leds, physical_segments->count);
      }
    }
  }

  if (counter > program_time) {
    counter = 0;
  }

  for (size_t i = 0; i < num_physical_segments; i++) {
    physical_segments[i].controller->showLeds(brightness);
  }
  for (size_t i = 0; i < num_buttons; i++) {
    int c = digitalRead(buttons[i].pin);
    if (c == HIGH && !buttons[i].pressed) {
      buttons[i].callback();
      buttons[i].pressed = true;
    }
    if (c == LOW) {
      buttons[i].pressed = false;
    }
  }
  counter++;
}

void debug() {
  if (hue < 64) {
    fill_solid(dual, num_dual, CRGB::Blue);
    fill_solid(up_strip, num_up_strip, CRGB::Blue);
    fill_solid(middle, num_middle, CRGB::Blue);
  } else if (hue < 128) {
    fill_solid(dual, num_dual, CRGB::Red);
    fill_solid(up_strip, num_up_strip, CRGB::Red);
    fill_solid(middle, num_middle, CRGB::Red);
  } else if (hue < 196) {
    fill_solid(dual, num_dual, CRGB::Green);
    fill_solid(up_strip, num_up_strip, CRGB::Green);
    fill_solid(middle, num_middle, CRGB::Green);
  } else {
    fill_solid(dual, num_dual, CRGB::White);
    fill_solid(up_strip, num_up_strip, CRGB::White);
    fill_solid(middle, num_middle, CRGB::White);
  }
  hue += 3;
}

// Add one layer of waves into the led array
void pacifica_one_layer(CRGB *leds, int count, CRGBPalette16 &p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff) {
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for (uint16_t i = 0; i < count; i++) {
    waveangle += 250;
    uint16_t s16 = sin16(waveangle) + 32768;
    uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16(ci) + 32768;
    uint8_t sindex8 = scale16(sindex16, 240);
    CRGB c = ColorFromPalette(p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add up_strip 'white' to areas where the four layers of light have lined up
// brightly
void pacifica_add_whitecaps(CRGB *leds, int count) {
  uint8_t basethreshold = beatsin8(9, 55, 65);
  uint8_t wave = beat8(7);

  for (uint16_t i = 0; i < count; i++) {
    uint8_t threshold = scale8(sin8(wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if (l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8(overage, overage);
      leds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors(CRGB *leds, int count) {
  for (uint16_t i = 0; i < count; i++) {
    leds[i].blue = scale8(leds[i].blue, 145);
    leds[i].green = scale8(leds[i].green, 200);
    leds[i] |= CRGB(2, 5, 7);
  }
}

// CRGBPalette16 pacifica_palette_1 = {0x000507, 0x000409, 0x00030B, 0x00030D,
// 0x000210, 0x000212, 0x000114, 0x000117,
//                                     0x000019, 0x00001C, 0x000026, 0x000031,
//                                     0x00003B, 0x000046, 0x14554B, 0x28AA50};
// CRGBPalette16 pacifica_palette_2 = {0x000507, 0x000409, 0x00030B, 0x00030D,
// 0x000210, 0x000212, 0x000114, 0x000117,
//                                     0x000019, 0x00001C, 0x000026, 0x000031,
//                                     0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
// CRGBPalette16 pacifica_palette_3 = {0x000208, 0x00030E, 0x000514, 0x00061A,
// 0x000820, 0x000927, 0x000B2D, 0x000C33,
//                                     0x000E39, 0x001040, 0x001450, 0x001860,
//                                     0x001C70, 0x002080, 0x1040BF, 0x2060FF};

CRGBPalette16 pacifica_palette_1 = {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
                                    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50};
CRGBPalette16 pacifica_palette_2 = {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
                                    0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
CRGBPalette16 pacifica_palette_3 = {0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
                                    0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF};

void pacifica_loop(CRGB *leds, int count) {
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011, 10, 13));
  sCIStart2 -= (deltams21 * beatsin88(777, 8, 11));
  sCIStart3 -= (deltams1 * beatsin88(501, 5, 7));
  sCIStart4 -= (deltams2 * beatsin88(257, 4, 6));

  // Clear out the LED array to a dim background blue-green
  fill_solid(leds, count, CRGB(2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary
  // over time
  pacifica_one_layer(leds, count, pacifica_palette_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0 - beat16(301));
  pacifica_one_layer(leds, count, pacifica_palette_2, sCIStart2, beatsin16(4, 6 * 256, 9 * 256), beatsin8(17, 40, 80), beat16(401));
  pacifica_one_layer(leds, count, pacifica_palette_3, sCIStart3, 6 * 256, beatsin8(9, 10, 38), 0 - beat16(503));
  pacifica_one_layer(leds, count, pacifica_palette_3, sCIStart4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps(leds, count);

  // Deepen the blues and greens a bit
  pacifica_deepen_colors(leds, count);
}

void pacifica() {
  pacifica_loop(dual, num_dual);
  pacifica_loop(up_strip, num_up_strip);
  pacifica_loop(middle, num_middle);
}

void rainbow_glitter() {
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(dual, num_dual, 80);
  addGlitter(up_strip, num_up_strip, 80);
  addGlitter(middle, num_middle, 80);
}

void addGlitter(CRGB *leds, int count, fract8 chanceOfGlitter) {
  if (random8() < chanceOfGlitter) {
    leds[random16(count)] += CRGB::White;
  }
}

void confetti_loop(CRGB *leds, int count) {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, count, 10);
  int pos = random16(count);
  leds[pos] += CHSV(hue + random8(64), 200, 255);
}

void sinelon_loop(CRGB *leds, int count) {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, count, 20);
  int pos = beatsin16(13, 0, count - 1);
  leds[pos] += CHSV(hue, 255, 192);
}

void bpm_loop(CRGB *leds, int count) {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < count; i++) {  // 9948
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }
}

void juggle_loop(CRGB *leds, int count) {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, count, 20);
  uint8_t dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, count - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void confetti() {
  confetti_loop(dual, num_dual);
  confetti_loop(up_strip, num_up_strip);
  confetti_loop(middle, num_middle);
}

void sinelon() {
  sinelon_loop(dual, num_dual);
  sinelon_loop(up_strip, num_up_strip);
  sinelon_loop(middle, num_middle);
}

void bpm() {
  bpm_loop(dual, num_dual);
  bpm_loop(up_strip, num_up_strip);
  bpm_loop(middle, num_middle);
}

void juggle() {
  juggle_loop(dual, num_dual);
  juggle_loop(up_strip, num_up_strip);
  juggle_loop(middle, num_middle);
}

void loop() {
  funcs[mode]();
  if (cycle == 1) {
    fill_solid(middle, num_middle, CRGB::Black);
    fill_solid(dual, num_dual, CRGB::Black);
  }
  if (cycle == 2) {
    fill_solid(dual, num_dual, CRGB::Black);
    fill_solid(up_strip, num_up_strip, CRGB::Black);
  }
  if (cycle == 3) {
    fill_solid(middle, num_middle, CRGB::Black);
    fill_solid(up_strip, num_up_strip, CRGB::Black);
  }
  if (cycle == 4) {
    fill_solid(dual, num_dual, CRGB::Black);
    fill_solid(up_strip, num_up_strip, CRGB::Black);
    fill_solid(middle, num_middle, CRGB::Black);
  }
  present();
  // switch (mode) {
  //   case 0:
  //     present(40);
  //     break;
  //   default:
  //     break;
  // }

  hue++;
  hue %= 256;
}
