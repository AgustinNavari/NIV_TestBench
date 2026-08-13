/*
 * breath_refference.h
 *
 *  Created on: 13 ago 2026
 *      Author: agusn
 */

 #ifndef BREATH_REFERENCE_H
 #define BREATH_REFERENCE_H

 #include <stdint.h>

 #define BREATH_REFERENCE_MAX_SAMPLES 1000

 typedef struct
 {
     int32_t size;

     float max_time_s;
     float max_volume_ml;

     float time_s[BREATH_REFERENCE_MAX_SAMPLES];
     float volume_ml[BREATH_REFERENCE_MAX_SAMPLES];

 } breath_reference_t;


 /*
  * Devuelve el volumen deseado en el instante t,
  * escalando la curva de referencia según:
  *
  * respiratory_rate_bpm -> respiraciones/min
  * tidal_volume_ml      -> mL
  * time_s               -> segundos
  */
 float breath_reference_get_volume(
     float respiratory_rate_bpm,
     float tidal_volume_ml,
     float time_s
 );

 #endif