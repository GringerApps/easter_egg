#include <pebble.h>

#define d(string, ...) APP_LOG (APP_LOG_LEVEL_DEBUG, string, ##__VA_ARGS__)

typedef struct GifContext{
  bool enlighten;
  bool on_shake;
} GifContext;

#define SEQUENCES_COUNT 4
const uint32_t SEQUENCES[SEQUENCES_COUNT] = {
  RESOURCE_ID_EGG_1,
  RESOURCE_ID_EGG_2,
  RESOURCE_ID_EGG_3,
  RESOURCE_ID_EGG_4
};
static const uint32_t FIRST_DELAY_MS = 10;
static const uint32_t MIN_TAP_EASTER_EGG = 10;
static const uint32_t MAX_TAP_EASTER_EGG = 15;
static const uint32_t EASTER_EGG_TIMEOUT = 5000;
static GifContext SHAKE_GIF_CONTEXT = { .enlighten = false, .on_shake = true };
static GifContext TICK_GIF_CONTEXT = { .enlighten = true, .on_shake = false };
static GifContext INIT_GIF_CONTEXT = { .enlighten = true, .on_shake = false };
static const GSize BITMAP_SIZE = { .w = PBL_IF_ROUND_ELSE(180, 144), .h = 132 };

static Window *s_main_window;
static TextLayer *s_time_layer;
static GBitmap *s_bitmap;
static GBitmap *s_easter_egg_bitmap;
static BitmapLayer *s_bitmap_layer;
static uint32_t s_tap_count;
static AppTimer *s_tap_timer;
static bool s_easter_egg_active;
static uint32_t s_next_delay;
static bool s_playing = false;
static GBitmapSequence * s_sequence;

static void reset_gif_to_default(){
  s_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_EGG_4);
  gbitmap_sequence_update_bitmap_next_frame(s_sequence, s_bitmap, &s_next_delay);
  gbitmap_sequence_destroy(s_sequence);
  s_sequence = NULL;
}

static void reset_gif(){
  gbitmap_sequence_restart(s_sequence);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  gbitmap_sequence_destroy(s_sequence);
  s_sequence = NULL;
}

static void play_gif(void *context){
  if(gbitmap_sequence_update_bitmap_next_frame(s_sequence, s_bitmap, &s_next_delay) && !s_easter_egg_active) {
    bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
    app_timer_register(s_next_delay, play_gif, NULL);
  }else{
    reset_gif();
    s_playing = false;
  }
}

static void try_play_gif(void *context) {
  GifContext * gif_context = (GifContext *) context;
  if(!s_playing){
    s_playing = true;
    if(gif_context->enlighten){
      light_enable_interaction();
    }
    uint32_t sequence_idx = SEQUENCES_COUNT - 1;
    if(gif_context->on_shake){
      sequence_idx = rand() % SEQUENCES_COUNT;
    }
    s_sequence = gbitmap_sequence_create_with_resource(SEQUENCES[sequence_idx]);
    gbitmap_sequence_set_play_count(s_sequence, 1);
    play_gif(NULL);
  }
}

static void reset_tap_timer(void * context){
  s_tap_timer = NULL;
  s_tap_count = 0;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if(s_tap_count >= MIN_TAP_EASTER_EGG && s_tap_count < MAX_TAP_EASTER_EGG) {
    if(s_tap_timer){
      app_timer_cancel(s_tap_timer);
      s_tap_timer = NULL;
    }
    if(!s_easter_egg_active){
      s_easter_egg_active = true;
      bitmap_layer_set_alignment(s_bitmap_layer, GAlignBottom);
      bitmap_layer_set_bitmap(s_bitmap_layer, s_easter_egg_bitmap);
    }
  }else if(s_tap_count >= MAX_TAP_EASTER_EGG){
    s_easter_egg_active = false;
    s_tap_count = 0;
    bitmap_layer_set_alignment(s_bitmap_layer, GAlignCenter);
    reset_gif_to_default();
  }else if(s_tap_count < MIN_TAP_EASTER_EGG){
    if(s_tap_timer == NULL || s_tap_count == 0 || !app_timer_reschedule(s_tap_timer, EASTER_EGG_TIMEOUT)){
      s_tap_timer = app_timer_register(EASTER_EGG_TIMEOUT, reset_tap_timer, NULL);
    }
  }
  s_tap_count++;
  app_timer_register(FIRST_DELAY_MS, try_play_gif, &SHAKE_GIF_CONTEXT);
}

static void update_time(const bool play_gif) {
  const time_t temp = time(NULL);
  const struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
  if(tick_time->tm_min % 5 == 0 && play_gif){
    app_timer_register(FIRST_DELAY_MS, try_play_gif, &TICK_GIF_CONTEXT);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(true);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  const GSize window_size = bounds.size;

  const int x = (window_size.w - BITMAP_SIZE.w) / 2;
  const GRect bitmap_layer_bounds = (GRect){ .origin = GPoint(x,0), .size = BITMAP_SIZE  };
  s_bitmap_layer = bitmap_layer_create(bitmap_layer_bounds);
  bitmap_layer_set_background_color(s_bitmap_layer, GColorImperialPurple);
  bitmap_layer_set_compositing_mode(s_bitmap_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_bitmap_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));

  const int text_height = window_size.h - BITMAP_SIZE.h;
  s_time_layer = text_layer_create(GRect(0, window_size.h - text_height, window_size.w, text_height));
  text_layer_set_background_color(s_time_layer, GColorImperialPurple);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  bitmap_layer_destroy(s_bitmap_layer);
}

static void init() {
  srand(time(NULL));
  s_tap_count = 0;
  s_tap_timer = NULL;
  s_easter_egg_active = false;
  s_easter_egg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_EASTER_EGG);

  s_bitmap = gbitmap_create_blank(BITMAP_SIZE, GBitmapFormat8Bit);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorImperialPurple);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time(false);
  app_timer_register(FIRST_DELAY_MS, try_play_gif, &INIT_GIF_CONTEXT);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_tap_service_subscribe(accel_tap_handler);
}

static void deinit() {
  if(s_sequence != NULL){
    gbitmap_sequence_destroy(s_sequence);
  }
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
  gbitmap_destroy(s_bitmap);
  gbitmap_destroy(s_easter_egg_bitmap);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
