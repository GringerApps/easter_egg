#include <pebble.h>

#define d(string, ...) APP_LOG (APP_LOG_LEVEL_DEBUG, string, ##__VA_ARGS__)

typedef struct {
  bool enlighten;
  bool on_shake;
} GifContext;

const uint32_t SEQUENCES[3] = {
  RESOURCE_ID_EGG_2,
  RESOURCE_ID_EGG_3,
  RESOURCE_ID_EGG_4,
};
const uint32_t FIRST_DELAY_MS = 10;
#define BITMAP_SIZE GSize(111,132)
const int TEXT_HEIGHT = 50;

static Window *s_main_window;
static TextLayer *s_time_layer;
static GBitmap *s_bitmap;
static BitmapLayer *s_bitmap_layer;

uint32_t next_delay;
bool playing = false;

static void reset_gif(GBitmapSequence * sequence){
  gbitmap_sequence_restart(sequence);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  gbitmap_sequence_destroy(sequence);
}

static void play_gif(void *context){
  GBitmapSequence * sequence = (GBitmapSequence*) context;
  if(gbitmap_sequence_update_bitmap_next_frame(sequence, s_bitmap, &next_delay)) {
    bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
    app_timer_register(next_delay, play_gif, sequence);
  }else{
    reset_gif(sequence);
    playing = false;
  }
}

static void try_play_gif(void *context) {
  GifContext * gif_context  = (GifContext *) context;
  if(!playing){
    playing = true;
    if(gif_context->enlighten){
      light_enable_interaction();
    }
    int sequence_idx = 2;
    if(gif_context->on_shake){
      sequence_idx = rand() % 3;
    }
    GBitmapSequence * sequence = gbitmap_sequence_create_with_resource(SEQUENCES[sequence_idx]);
    gbitmap_sequence_set_play_count(sequence, 1);
    play_gif(sequence);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  GifContext context = (GifContext) { .enlighten = false, .on_shake = true };
  app_timer_register(FIRST_DELAY_MS, try_play_gif, &context);
}

static void update_time(const bool play_gif) {
  const time_t temp = time(NULL);
  const struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
  if(tick_time->tm_min % 5 == 0 && play_gif){
    GifContext context = (GifContext) { .enlighten = true, .on_shake = false };
    app_timer_register(FIRST_DELAY_MS, try_play_gif, &context);
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

  s_time_layer = text_layer_create(GRect(0, window_size.h - TEXT_HEIGHT, window_size.w, TEXT_HEIGHT));
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

  s_bitmap = gbitmap_create_blank(BITMAP_SIZE, GBitmapFormat8Bit);
  GBitmapSequence * sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_EGG_4);
  gbitmap_sequence_update_bitmap_next_frame(sequence, s_bitmap, &next_delay);
  gbitmap_sequence_destroy(sequence);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorImperialPurple);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time(false);
  GifContext context = (GifContext) { .enlighten = true, .on_shake = false };
  app_timer_register(FIRST_DELAY_MS, try_play_gif, &context);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_tap_service_subscribe(accel_tap_handler);
}

static void deinit() {
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
  gbitmap_destroy(s_bitmap);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
