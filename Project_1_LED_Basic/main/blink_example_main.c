/* LEDC (LED Controller) basic example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"

#define LEDC_TIMER              LEDC_TIMER_0 //Se elige el timer 0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE //Se elige el modo de baja velocidad
#define LEDC_OUTPUT_IO          (2) // Define the output GPIO //Se elige el pin GPIO2 (D2 en la placa)
#define LEDC_CHANNEL            LEDC_CHANNEL_0 //Se elige el canal 0 (el canal es el que se usa para generar la señal PWM)
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits (ciclo de trabajo el cual define la intensidad de la luz)
#define LEDC_DUTY               (2048) // Set duty to 50%. (2 ** 13) * 50% = 4096 (Intensidad de la luz, siendo 8192 el máximo y 2048 el 25%)
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz (frecuencia de parpadeo de la luz, siendo 4kHz porque es un valor alto y no se nota el parpadeo)

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2 (rev < 1.2), ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */
