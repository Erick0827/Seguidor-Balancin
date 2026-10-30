#include "encoder.h"
#include "config.h"
#include "driver/gpio.h"

pcnt_unit_handle_t crearEncoderPCNT(int pinA, int pinB)
{
  pcnt_unit_config_t configUnidad = {};
  configUnidad.high_limit = PCNT_LIMITE_ALTO;
  configUnidad.low_limit  = PCNT_LIMITE_BAJO;
  configUnidad.flags.accum_count = 1;

  pcnt_unit_handle_t unidad = NULL;
  ESP_ERROR_CHECK(pcnt_new_unit(&configUnidad, &unidad));

  pcnt_glitch_filter_config_t configFiltro = {};
  configFiltro.max_glitch_ns = PCNT_FILTRO_NS;
  ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unidad, &configFiltro));

  // Canal A: cuenta flancos de la fase A, la fase B define la dirección
  pcnt_chan_config_t configCanalA = {};
  configCanalA.edge_gpio_num  = pinA;
  configCanalA.level_gpio_num = pinB;
  pcnt_channel_handle_t canalA = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(unidad, &configCanalA, &canalA));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalA,
                  PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalA,
                  PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  // Canal B: cuenta flancos de la fase B, la fase A define la dirección
  pcnt_chan_config_t configCanalB = {};
  configCanalB.edge_gpio_num  = pinB;
  configCanalB.level_gpio_num = pinA;
  pcnt_channel_handle_t canalB = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(unidad, &configCanalB, &canalB));
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(canalB,
                  PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(canalB,
                  PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

  // Puntos de observación en los límites, necesarios para acumular el conteo
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unidad, PCNT_LIMITE_ALTO));
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unidad, PCNT_LIMITE_BAJO));

  // Pull-ups internos en las fases (el driver configura los pines como entrada)
  gpio_pullup_en((gpio_num_t)pinA);
  gpio_pullup_en((gpio_num_t)pinB);

  ESP_ERROR_CHECK(pcnt_unit_enable(unidad));
  ESP_ERROR_CHECK(pcnt_unit_clear_count(unidad));
  ESP_ERROR_CHECK(pcnt_unit_start(unidad));
  return unidad;
}

long leerConteo(pcnt_unit_handle_t unidad)
{
  int c = 0;
  pcnt_unit_get_count(unidad, &c);
  return c;
}
