#include <FastLED.h>
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
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
CRGB dual_segment_leds[num_dual_segment_leds];
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
  int num_segments;
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

// A program is looping sequence groups, each group is made of a list of sequences, and an optional side effect to modify any globals
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
  const char *name;
};

#pragma endregion LightProgram

#pragma region simple_programs

void black(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Black); }
LightSequence reset_sequence = {&black, nullptr, nullptr};
LightSequenceGroup reset_group = {&reset_sequence, 1, 1000, []() {}};

void rainbow(CRGB *leds, int count) { fill_rainbow(leds, count, counter, program_parameter_x * 2 % 10); }
LightSequence rainbow_sequence = {&rainbow, nullptr, nullptr};
LightSequenceGroup rainbow_group = {&rainbow_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

void red(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Red); }
void green(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Green); }
void blue(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Blue); }
void white(CRGB *leds, int count) { fill_solid(leds, count, CRGB::White); }
LightSequence red_sequence = {&red, nullptr, nullptr};
LightSequence green_sequence = {&green, nullptr, nullptr};
LightSequence blue_sequence = {&blue, nullptr, nullptr};
LightSequence white_sequence = {&white, nullptr, nullptr};
LightSequenceGroup red_group = {&red_sequence, 1, 500, []() {}};
LightSequenceGroup green_group = {&green_sequence, 1, 500, []() {}};
LightSequenceGroup blue_group = {&blue_sequence, 1, 500, []() {}};
LightSequenceGroup white_group = {&white_sequence, 1, 500, []() {}};
LightSequenceGroup *rgbw_groups[] = {&red_group, &green_group, &blue_group, &white_group};

#pragma endregion simple_programs

#pragma region pacifica

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

void pacifica(CRGB *leds, int count) {
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

LightSequence pacifica_sequence = {&pacifica, nullptr, nullptr};
LightSequenceGroup pacifica_group = {&pacifica_sequence, 1, 1000, []() { counter += program_parameter_y; }};
#pragma endregion pacifica

void rainbow_glitter(CRGB *leds, int count) {
  rainbow(leds, count);
  if (random8() < 80) {
    leds[random16(count)] += CRGB::White;
  }
}
LightSequence rainbow_glitter_sequence = {&rainbow_glitter, nullptr, nullptr};
LightSequenceGroup rainbow_glitter_group = {&rainbow_glitter_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

void confetti(CRGB *leds, int count) {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, count, 10);
  int pos = random16(count);
  leds[pos] += CHSV(counter + random8(64), 200 + (program_parameter_x % 5) * 10, 255);
}

LightSequence confetti_sequence = {&confetti, nullptr, nullptr};
LightSequenceGroup confetti_group = {&confetti_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

void sinelon(CRGB *leds, int count) {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, count, 20);
  int pos = beatsin16(13 + (program_parameter_x % 5) * 2 - 5, 0, count - 1);
  leds[pos] += CHSV(counter, 255, 192);
}

LightSequence sinelon_sequence = {&sinelon, nullptr, nullptr};
LightSequenceGroup sinelon_group = {&sinelon_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

void bpm(CRGB *leds, int count) {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = +(program_parameter_x % 8) * 10 + 30;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < count; i++) {  // 9948
    leds[i] = ColorFromPalette(palette, counter + (i * 2), beat - counter + (i * 10));
  }
}

LightSequence bpm_sequence = {&bpm, nullptr, nullptr};
LightSequenceGroup bpm_group = {&bpm_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

void juggle(CRGB *leds, int count) {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, count, 20);
  uint8_t dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, count - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

LightSequence juggle_sequence = {&juggle, nullptr, nullptr};
LightSequenceGroup juggle_group = {&juggle_sequence, 1, 1000, []() { counter += program_parameter_y % 6; }};

#define num_programs 5
LightProgram programs[num_programs] = {
    {&reset_group, 1, "reset"}, {&rainbow_group, 1, "rainbow"}, {rgbw_groups[0], 4, "rgbw"}, {&pacifica_group, 1, "pacifica"}, {&rainbow_glitter_group, 1, "glitter"},
};

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

#pragma region main

void setup() {
  delay(2000);  // sanity delay
  Serial.print("start");
  // for (int i = 0; i < num_buttons; i++) {
  //   Serial.print("button" + String(i));
  //   pinMode(buttons[i].pin, INPUT_PULLUP);
  // }
}

void process_logical_segment(LightFunction light_function, LogicalSegment *logical_segment) {
  light_function(logical_segment->physical_segment->leds + logical_segment->start, logical_segment->end - logical_segment->start);
  if (logical_segment->pixel_shift) {
    for (int i = logical_segment->start; i < logical_segment->end; i++) {
      CRGB &color = logical_segment->physical_segment->leds[i];
      uint8_t temp = color.r;
      color.r = color.g;
      color.g = color.b;
      color.b = temp;
    }
  }
}

void loop() {
  Serial.print("loop");
  // LightProgram &program = programs[current_program];
  // Serial.print(program.name);
  // LightSequenceGroup *group = nullptr;
  // int program_time = 0;
  // for (int i = 0; i < program.num_groups; i++) {
  //   program_time += program.light_sequence_groups[i].duration;
  //   if (counter <= program_time) {
  //     group = &program.light_sequence_groups[i];
  //     Serial.print("group");
  //   }
  // }
  // group->side_effect();
  // for (int i = 0; i < group->num_sequences; i++) {
  //   LightSequence &sequence = group->sequences[i];
  //   if (sequence.logical_segment != nullptr) {
  //   } else if (sequence.logical_segment_group != nullptr) {
  //     process_logical_segment(sequence.light_function, sequence.logical_segment);
  //   } else {
  //     for (int i = 0; i < sequence.logical_segment_group->num_segments; i++) {
  //       process_logical_segment(sequence.light_function, sequence.logical_segment_group->logical_segments[i]);
  //     }
  //   }
  // }

  // if (counter > program_time) {
  //   counter = 0;
  // }

  // for (size_t i = 0; i < num_physical_segments; i++) {
  //   physical_segments[i].controller->showLeds(brightness);
  // }
  // for (size_t i = 0; i < num_buttons; i++) {
  //   int c = digitalRead(buttons[i].pin);
  //   if (c == HIGH && !buttons[i].pressed) {
  //     buttons[i].callback();
  //     buttons[i].pressed = true;
  //   }
  //   if (c == LOW) {
  //     buttons[i].pressed = false;
  //   }
  // }
  // counter++;
  delay(100);
}

#pragma endregion main