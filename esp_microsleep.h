/*
 * Copyright (c) 2024 Dr. Michael 'Mickey' Lauer <mlauer@vanille-media.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file esp_microsleep.h
 * @brief Provides a microsecond-level delay function for ESP32 using esp_timer and FreeRTOS task notifications.
 *
 * @note Configuration Requirements:
 * This module relies on specific ESP-IDF Kconfig settings:
 *  - `CONFIG_ESP_MICROSLEEP_TLS_INDEX`: Defines the FreeRTOS Task Local Storage (TLS) index used to store
 *    per-task timer handles. Ensure this index is reserved and not used by other components.
 *  - `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD`: Must be enabled to allow the timer callback
 *    (`esp_microsleep_isr_handler`) to run directly from the timer ISR context for minimal latency.
 *    This typically requires `CONFIG_ESP_TIMER_ISR_AFFINITY` to be configured appropriately
 *    (e.g., `ESP_TIMER_ISR_AFFINITY_CPU0` or `ESP_TIMER_ISR_AFFINITY_CPU1`).
 *
 * @details
 * This implementation uses a one-shot `esp_timer` per task, managed via TLS.
 * It includes a calibration function (`esp_microsleep_calibrate`) to measure the overhead
 * of the timer mechanism and uses `ets_delay_us` for very short delays where the timer
 * overhead would be significant.
 */

#pragma once
#ifndef ESP_MICROSLEEP_H
#define ESP_MICROSLEEP_H

#include "stdint.h" // for uint64_t
#include "sdkconfig.h" // for CONFIG_*

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_ESP_MICROSLEEP_TLS_INDEX) && defined(CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)

/**
 * @brief Calibrate the microsleep compensation value.
 *
 * Adjusts the global microsleep compensation value for your system.
 *
 * On an ESP32S3 with a 240Mhz CPU clock, the compensation value is 15.
 * It may be higher, if you have more tasks running microsleep at the same time.
 *
 * It's advisable to calibrate when the system is under typical load, i.e.
 * not necessarily when the system is idle or booting.
*/
uint64_t esp_microsleep_calibrate();

/**
 * @brief Delay the current task for a specified number of microseconds.
 *
 * This function delays the current task for the specified number of microseconds.
 * It is a fine grained replacement for `vTaskDelay`, since due to the way FreeRTOS
 * works, you can not achieve reliable delays for small amounts of ticks.
 *
 * This has been inspired by the discussion at https://esp32.com/viewtopic.php?f=13&t=38644.
 *
 * To achieve concurrency safety when calling this from multiple tasks, FreeRTOS task local storage is used.
 * If you are already using FreeRTOS' task local storage in your program, you need to ensure that
 * a) your configuration includes support for FreeRTOS task local storage, and
 * b) the task-local-storage-index you give is not used by another part of your program.
 *
 * @param us Microseconds to delay.
 *
 * @return None.
 */
void esp_microsleep_delay(uint64_t us);
#else
#warning esp_microsleep not available due to missing configuration
#endif // CONFIG_ESP_MICROSLEEP_TLS_INDEX && CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_MICROSLEEP_H
