#pragma once
#include "../../firmware/src/app/app_events.h"

/* Start een achtergrond-timer die nep HA-events genereert.
   send_fn wordt aangeroepen telkens als er een event klaar is.

   Gesimuleerde entiteiten:
     switch.woonkamer_licht    → toggle
     light.dimmer_bank         → slider brightness
     climate.woonkamer         → slider temperature
     sensor.temperatuur        → label
     binary_sensor.deurbel     → label
     sensor.luchtvochtigheid   → label
*/
void mock_ha_start(void (*send_fn)(const ha_event_t *));

/* Verwerk een commando vanuit de UI (toggle, set_brightness, etc.) */
void mock_ha_handle_cmd(const ha_cmd_t *cmd);
