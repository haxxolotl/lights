#include <FastLED.h>
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
int brightness = 8;
int counter = 0;
int current_program = 0;
int program_parameter_x = 0;
int program_parameter_y = 0;
int verbosity = 0;

#pragma region PhysicalSegment

struct PhysicalSegment {
  int count;
  CRGB *leds;
  CLEDController *controller;
  const char *name;
};

#define num_physical_segments 3
#define num_strip_segment_leds 100
#define num_dual_segment_leds 357
#define num_fairy_segment_leds 400
CRGB strip_segment_leds[num_strip_segment_leds];
CRGB dual_segment_leds[num_dual_segment_leds];
CRGB fairy_segment_leds[num_fairy_segment_leds];

PhysicalSegment strip_segment = {num_strip_segment_leds, strip_segment_leds, &FastLED.addLeds<WS2811, 7, BRG>(strip_segment_leds, num_strip_segment_leds), "strip"};  // strip 1*100
PhysicalSegment dual_segment = {num_dual_segment_leds, dual_segment_leds, &FastLED.addLeds<WS2811, 8, BRG>(dual_segment_leds, num_dual_segment_leds),
                                "dual"};  // fairy 2x50 + strip 2*100 + 56
PhysicalSegment fairy_segment = {num_fairy_segment_leds, fairy_segment_leds, &FastLED.addLeds<WS2811, 10, RGB>(fairy_segment_leds, num_fairy_segment_leds), "fairy"};  // fairy 8*50
PhysicalSegment physical_segments[num_physical_segments] = {strip_segment, dual_segment, fairy_segment};

#pragma endregion PhysicalSegment

#pragma region LogicalSegment

// these map to some region of a physical segment, these are elemental and do not overlap
struct LogicalSegment {
  PhysicalSegment *physical_segment;
  int start;
  int end;
  bool pixel_shift;
  const char *name;
};

LogicalSegment strip_segment_up = {&strip_segment, 0, 40, false, "strip_segment_up"};
LogicalSegment strip_segment_over = {&strip_segment, 40, num_strip_segment_leds, false, "strip_segment_over"};
LogicalSegment dual_segment_up = {&dual_segment, 0, 32, true, "dual_segment_up"};
LogicalSegment dual_segment_window = {&dual_segment, 32, 100, true, "dual_segment_window"};
LogicalSegment dual_segment_right_corner = {&dual_segment, 100, 117, false, "dual_segment_right_corner"};
LogicalSegment dual_segment_back_ceiling = {&dual_segment, 117, 197, false, "dual_segment_back_ceiling"};
LogicalSegment dual_segment_left_ceiling = {&dual_segment, 197, 274, false, "dual_segment_left_ceiling"};
LogicalSegment dual_segment_front_ceiling = {&dual_segment, 274, num_dual_segment_leds, false, "dual_segment_front_ceiling"};
LogicalSegment fairy_segment_up = {&fairy_segment, 0, 26, false, "fairy_segment_up"};
LogicalSegment fairy_segment_1 = {&fairy_segment, 26, 87, false, "fairy_segment_1"};
LogicalSegment fairy_segment_2 = {&fairy_segment, 87, 148, false, "fairy_segment_2"};
LogicalSegment fairy_segment_3 = {&fairy_segment, 148, 210, false, "fairy_segment_3"};
LogicalSegment fairy_segment_4 = {&fairy_segment, 210, 272, false, "fairy_segment_4"};
LogicalSegment fairy_segment_5 = {&fairy_segment, 272, 335, false, "fairy_segment_5"};
LogicalSegment fairy_segment_6 = {&fairy_segment, 335, num_fairy_segment_leds, false, "fairy_segment_6"};

#pragma endregion LogicalSegment

#pragma region LogicalSegmentGroup

// groups can have strip combination that overlap other groups
struct LogicalSegmentGroup {
  LogicalSegment **logical_segments;
  int num_segments;
};
LogicalSegment *strip_right_segments[6] = {&strip_segment_over, &dual_segment_right_corner};
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
LogicalSegmentGroup strip_right_group = {strip_right_segments, 6};
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
  LightSequence(LightFunction a, LogicalSegment *b = nullptr, LogicalSegmentGroup *c = nullptr, bool d = true, bool e = false)
      : light_function(a), logical_segment(b), logical_segment_group(c), clear_first(d), spread_function_over_group(e) {}
  LightFunction light_function;
  LogicalSegment *logical_segment;
  LogicalSegmentGroup *logical_segment_group;
  bool clear_first;
  bool spread_function_over_group;
};

struct LightSequenceGroup {
  LightSequenceGroup(
      LightSequence **a, int b = 1, int c = 1000, SequenceGroupSideEffect d = []() {}, bool e = true)
      : sequences(a), num_sequences(b), duration(c), side_effect(d), clear_first(e) {}
  LightSequence **sequences;
  int num_sequences;
  int duration;  // ms
  SequenceGroupSideEffect side_effect;
  bool clear_first;
};

struct LightProgram {
  LightSequenceGroup **light_sequence_groups;
  int num_groups;
  const char *name;
};

#pragma endregion LightProgram

#pragma region simple_programs

void black(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Black); }
LightSequence reset_sequence = {&black, nullptr, nullptr, true};
LightSequence *reset_sequences[] = {&reset_sequence};
LightSequenceGroup reset_group = {reset_sequences, 1, 1000};
LightSequenceGroup *reset_groups[] = {&reset_group};

void rainbow(CRGB *leds, int count) { fill_rainbow(leds, count, counter, program_parameter_x % 6 * 4 - 12 + 13); }
LightSequence rainbow_sequence = {&rainbow, nullptr, nullptr, true};
LightSequence *rainbow_sequences[] = {&rainbow_sequence};
LightSequenceGroup rainbow_group = {rainbow_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }, true};
LightSequenceGroup *rainbow_groups[] = {&rainbow_group};

void red(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Red); }
void green(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Green); }
void blue(CRGB *leds, int count) { fill_solid(leds, count, CRGB::Blue); }
void white(CRGB *leds, int count) { fill_solid(leds, count, CRGB::White); }
LightSequence red_sequence = {&red, nullptr, nullptr, true};
LightSequence green_sequence = {&green, nullptr, nullptr, true};
LightSequence blue_sequence = {&blue, nullptr, nullptr, true};
LightSequence white_sequence = {&white, nullptr, nullptr, true};
LightSequence *red_sequences[] = {&red_sequence};
LightSequence *green_sequences[] = {&green_sequence};
LightSequence *blue_sequences[] = {&blue_sequence};
LightSequence *white_sequences[] = {&white_sequence};
LightSequenceGroup red_group = {red_sequences, 1, 500};
LightSequenceGroup green_group = {green_sequences, 1, 500};
LightSequenceGroup blue_group = {blue_sequences, 1, 500};
LightSequenceGroup white_group = {white_sequences, 1, 500};
LightSequenceGroup *rgbw_groups[] = {&red_group, &green_group, &blue_group, &white_group};

#pragma endregion simple_programs

#pragma region debug

void debug(CRGB *leds, int count) {
  fill_solid(leds + count / 4 * 0, count / 4, CRGB::Red);
  fill_solid(leds + count / 4 * 1, count / 4, CRGB::Green);
  fill_solid(leds + count / 4 * 2, count / 4, CRGB::Blue);
  fill_solid(leds + count / 4 * 3, count / 4 + count % 4, CRGB::White);
}
LightSequence debug_sequence1 = {&debug, &strip_segment_up};
LightSequence debug_sequence2 = {&debug, &strip_segment_over};
LightSequence debug_sequence3 = {&debug, &dual_segment_up};
LightSequence debug_sequence4 = {&debug, &dual_segment_window};
LightSequence debug_sequence5 = {&debug, &dual_segment_right_corner};
LightSequence debug_sequence6 = {&debug, &dual_segment_back_ceiling};
LightSequence debug_sequence7 = {&debug, &dual_segment_left_ceiling};
LightSequence debug_sequence8 = {&debug, &dual_segment_front_ceiling};
LightSequence debug_sequence9 = {&debug, &fairy_segment_up};
LightSequence debug_sequence10 = {&debug, &fairy_segment_1};
LightSequence debug_sequence11 = {&debug, &fairy_segment_2};
LightSequence debug_sequence12 = {&debug, &fairy_segment_3};
LightSequence debug_sequence13 = {&debug, &fairy_segment_4};
LightSequence debug_sequence14 = {&debug, &fairy_segment_5};
LightSequence debug_sequence15 = {&debug, &fairy_segment_6};
LightSequence debug_sequence16 = {&debug, nullptr, &strip_group};
LightSequence debug_sequence17 = {&debug, nullptr, &strip_ceiling_group};
LightSequence debug_sequence18 = {&debug, nullptr, &fairy_group};
LightSequence debug_sequence19 = {&debug, nullptr, &fairy_ceiling_group};
LightSequence debug_sequence20 = {&debug, nullptr, &all_group};
LightSequence *debug_sequences1[] = {&debug_sequence1};
LightSequence *debug_sequences2[] = {&debug_sequence2};
LightSequence *debug_sequences3[] = {&debug_sequence3};
LightSequence *debug_sequences4[] = {&debug_sequence4};
LightSequence *debug_sequences5[] = {&debug_sequence5};
LightSequence *debug_sequences6[] = {&debug_sequence6};
LightSequence *debug_sequences7[] = {&debug_sequence7};
LightSequence *debug_sequences8[] = {&debug_sequence8};
LightSequence *debug_sequences9[] = {&debug_sequence9};
LightSequence *debug_sequences10[] = {&debug_sequence10};
LightSequence *debug_sequences11[] = {&debug_sequence11};
LightSequence *debug_sequences12[] = {&debug_sequence12};
LightSequence *debug_sequences13[] = {&debug_sequence13};
LightSequence *debug_sequences14[] = {&debug_sequence14};
LightSequence *debug_sequences15[] = {&debug_sequence15};
LightSequence *debug_sequences16[] = {&debug_sequence16};
LightSequence *debug_sequences17[] = {&debug_sequence17};
LightSequence *debug_sequences18[] = {&debug_sequence18};
LightSequence *debug_sequences19[] = {&debug_sequence19};
LightSequence *debug_sequences20[] = {&debug_sequence20};
LightSequenceGroup debug_group1 = {debug_sequences1};
LightSequenceGroup debug_group2 = {debug_sequences2};
LightSequenceGroup debug_group3 = {debug_sequences3};
LightSequenceGroup debug_group4 = {debug_sequences4};
LightSequenceGroup debug_group5 = {debug_sequences5};
LightSequenceGroup debug_group6 = {debug_sequences6};
LightSequenceGroup debug_group7 = {debug_sequences7};
LightSequenceGroup debug_group8 = {debug_sequences8};
LightSequenceGroup debug_group9 = {debug_sequences9};
LightSequenceGroup debug_group10 = {debug_sequences10};
LightSequenceGroup debug_group11 = {debug_sequences11};
LightSequenceGroup debug_group12 = {debug_sequences12};
LightSequenceGroup debug_group13 = {debug_sequences13};
LightSequenceGroup debug_group14 = {debug_sequences14};
LightSequenceGroup debug_group15 = {debug_sequences15};
LightSequenceGroup debug_group16 = {debug_sequences16};
LightSequenceGroup debug_group17 = {debug_sequences17};
LightSequenceGroup debug_group18 = {debug_sequences18};
LightSequenceGroup debug_group19 = {debug_sequences19};
LightSequenceGroup debug_group20 = {debug_sequences20};
LightSequenceGroup *debug_groups[] = {&debug_group1,  &debug_group2,  &debug_group3,  &debug_group4,  &debug_group5,  &debug_group6,  &debug_group7,
                                      &debug_group8,  &debug_group9,  &debug_group10, &debug_group11, &debug_group12, &debug_group13, &debug_group14,
                                      &debug_group15, &debug_group16, &debug_group17, &debug_group18, &debug_group19, &debug_group20};

#pragma endregion debug

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

CRGBPalette16 pacifica_palette_alt_1 = {0xBB0507, 0x880409, 0x00030B, 0x88030D, 0x880210, 0x880212, 0x880114, 0x000117,
                                        0x000019, 0x88001C, 0x000026, 0x880031, 0x88003B, 0x880046, 0x88554B, 0x28AA50};
CRGBPalette16 pacifica_palette_alt_2 = {0x000507, 0x880409, 0x00030B, 0x88030D, 0x880210, 0x880212, 0x880114, 0x000117,
                                        0x000019, 0x88001C, 0x000026, 0x880031, 0x88003B, 0x880046, 0x885F52, 0x19BE5F};
CRGBPalette16 pacifica_palette_alt_3 = {0x000208, 0x88030E, 0x000514, 0x88061A, 0x880820, 0x880927, 0x880B2D, 0x000C33,
                                        0x000E39, 0x881040, 0x001450, 0x881860, 0x881C70, 0x882080, 0x8840BF, 0x2060FF};

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
  uint32_t deltams = (ms - sLastms) * (program_parameter_y % 6 + 1) / 24;
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
  pacifica_one_layer(leds, count, program_parameter_x % 2 ? pacifica_palette_1 : pacifica_palette_alt_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0 - beat16(301));
  pacifica_one_layer(leds, count, program_parameter_x % 2 ? pacifica_palette_2 : pacifica_palette_alt_2, sCIStart2, beatsin16(4, 6 * 256, 9 * 256), beatsin8(17, 40, 80), beat16(401));
  pacifica_one_layer(leds, count, program_parameter_x % 2 ? pacifica_palette_3 : pacifica_palette_alt_3, sCIStart3, 6 * 256, beatsin8(9, 10, 38), 0 - beat16(503));
  pacifica_one_layer(leds, count, program_parameter_x % 2 ? pacifica_palette_3 : pacifica_palette_alt_3, sCIStart4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps(leds, count);

  // Deepen the blues and greens a bit
  pacifica_deepen_colors(leds, count);
}

LightSequence pacifica_sequence = {&pacifica, nullptr, nullptr, true};
LightSequence *pacifica_sequences[] = {&pacifica_sequence};
LightSequenceGroup pacifica_group = {pacifica_sequences, 1, 1000};
LightSequenceGroup *pacifica_groups[] = {&pacifica_group};
#pragma endregion pacifica

void rainbow_glitter(CRGB *leds, int count) {
  rainbow(leds, count);
  if (random8() < 80) {
    leds[random16(count)] += CRGB::White;
  }
}
LightSequence rainbow_glitter_sequence = {&rainbow_glitter, nullptr, nullptr, true};
LightSequence *rainbow_glitter_sequences[] = {&rainbow_glitter_sequence};
LightSequenceGroup rainbow_glitter_group = {rainbow_glitter_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }};
LightSequenceGroup *rainbow_glitter_groups[] = {&rainbow_glitter_group};

void confetti(CRGB *leds, int count) {
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, count, 10);
  int pos = random16(count);
  leds[pos] += CHSV(counter + random8(64), 200 + (program_parameter_x % 5) * 10, 255);
}

LightSequence confetti_sequence = {&confetti, nullptr, nullptr, true};
LightSequence *confetti_sequences[] = {&confetti_sequence};
LightSequenceGroup confetti_group = {confetti_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }};
LightSequenceGroup *confetti_groups[] = {&confetti_group};

void sinelon(CRGB *leds, int count) {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, count, 20);
  int pos = beatsin16(13 + (program_parameter_x % 5) * 2 - 5, 0, count - 1);
  leds[pos] += CHSV(counter, 255, 192);
}

LightSequence sinelon_sequence = {&sinelon, nullptr, nullptr, true};
LightSequence *sinelon_sequences[] = {&sinelon_sequence};
LightSequenceGroup sinelon_group = {sinelon_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }};
LightSequenceGroup *sinelon_groups[] = {&sinelon_group};

void bpm(CRGB *leds, int count) {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = +(program_parameter_x % 8) * 10 + 30;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < count; i++) {  // 9948
    leds[i] = ColorFromPalette(palette, counter + (i * 2), beat - counter + (i * 10));
  }
}

LightSequence bpm_sequence = {&bpm, nullptr, nullptr, true};
LightSequence *bpm_sequences[] = {&bpm_sequence};
LightSequenceGroup bpm_group = {bpm_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }};
LightSequenceGroup *bpm_groups[] = {&bpm_group};

void juggle(CRGB *leds, int count) {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, count, 20);
  uint8_t dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, count - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

LightSequence juggle_sequence = {&juggle, nullptr, nullptr, true};
LightSequence *juggle_sequences[] = {&juggle_sequence};
LightSequenceGroup juggle_group = {juggle_sequences, 1, 1000, []() { counter += (program_parameter_y % 10 - 5) * 10; }};
LightSequenceGroup *juggle_groups[] = {&juggle_group};

#define num_programs 9
LightProgram programs[num_programs] = {
    {rainbow_groups, 1, "rainbow"},   {pacifica_groups, 1, "pacifica"}, {rainbow_glitter_groups, 1, "glitter"},
    {confetti_groups, 1, "confetti"}, {sinelon_groups, 1, "sinelon"},   {bpm_groups, 1, "bpm"},
    {juggle_groups, 1, "juggle"},     {rgbw_groups, 4, "rgbw"},         {debug_groups, 20, "debug"},
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
       Serial.print("next brightness ");
       // next brightness setting
       if (brightness == 0)
         brightness = 1;
       else
         brightness *= 2;
       if (brightness > 128) brightness = 0;
       Serial.print(brightness);
     }},
    {2, false,
     []() {  // next lighting program
       Serial.print("next program ");
       current_program = (num_programs + current_program + 1) % num_programs;
       program_parameter_x = 0;
       program_parameter_y = 0;
       counter = 0;
       Serial.println(programs[current_program].name);
     }},
    {1, false,
     []() {
       Serial.print("increment program parameter x ");
       program_parameter_x++;
       Serial.println(program_parameter_x);
     }},
    {0, false,
     []() {
       Serial.print("increment program parameter Y ");
       program_parameter_y++;
       Serial.println(program_parameter_y);
     }},
};

#pragma endregion Button

#pragma region main

void setup() {
  delay(2000);  // sanity delay
  Serial.begin(115200);
  Serial.print("start");
  for (int i = 0; i < num_buttons; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }
}

void process_logical_segment(LightFunction light_function, LogicalSegment *logical_segment, bool clear_first) {
  if (verbosity >= 2) {
    Serial.print("writing to logical segment: ");
    Serial.print(clear_first);
    Serial.print(logical_segment->name);
    Serial.print("physical segment: ");
    Serial.print(logical_segment->physical_segment->name);
    Serial.print(" ");
    Serial.print(logical_segment->start);
    Serial.print(" ");
    Serial.println(logical_segment->end - logical_segment->start);
  }
  if (clear_first) {
    black(logical_segment->physical_segment->leds + logical_segment->start, logical_segment->end - logical_segment->start);
  }
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
  LightProgram &program = programs[current_program];
  if (verbosity >= 1) {
    Serial.print("in loop running lighting program: ");
    Serial.print(program.name);
    Serial.print(" num groups: ");
    Serial.println(program.num_groups);
    Serial.print("ct: ");
    Serial.print(counter);
    Serial.print("   ");
  }
  LightSequenceGroup *group = program.light_sequence_groups[program.num_groups - 1];
  int program_time = 0;
  for (int i = 0; i < program.num_groups; i++) {
    program_time += program.light_sequence_groups[i]->duration;
    if (program_time <= counter) {
      group = program.light_sequence_groups[i];
    }
  }
  if (counter > program_time) {
    counter = 0;
  }
  if (verbosity >= 1) {
    Serial.print(" program total duration: ");
    Serial.println(program_time);
  }
  group->side_effect();
  if (group->clear_first) {
    for (int i = 0; i < all_group.num_segments; i++) {
      process_logical_segment(black, all_group.logical_segments[i], true);
    }
  }
  for (int i = 0; i < group->num_sequences; i++) {
    LightSequence &sequence = *group->sequences[i];
    if (sequence.logical_segment != nullptr) {
      process_logical_segment(sequence.light_function, sequence.logical_segment, sequence.clear_first);
    } else if (sequence.logical_segment_group != nullptr) {
      for (int i = 0; i < sequence.logical_segment_group->num_segments; i++) {
        process_logical_segment(sequence.light_function, sequence.logical_segment_group->logical_segments[i], sequence.clear_first);
      }
    } else {
      for (int i = 0; i < all_group.num_segments; i++) {
        process_logical_segment(sequence.light_function, all_group.logical_segments[i], sequence.clear_first);
      }
    }
  }

  for (size_t i = 0; i < num_physical_segments; i++) {
    physical_segments[i].controller->showLeds(brightness);
  }
  for (size_t i = 0; i < num_buttons; i++) {
    int c = digitalRead(buttons[i].pin);
    if (c == HIGH && !buttons[i].pressed) {
      buttons[i].callback();
      buttons[i].pressed = true;
      Serial.print("Button pressed: ");
      Serial.println(i);
    }
    if (c == LOW) {
      buttons[i].pressed = false;
    }
  }
  counter += 33;
  // delay(1);
}

#pragma endregion main