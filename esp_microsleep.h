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
#include "esp_err.h" // for esp_err_t

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
 * @brief Delays the calling task for a specified number of microseconds.
 *
 * This function serves as a high-precision replacement for `vTaskDelay` for delays
 * specified in microseconds. Unlike `vTaskDelay`, which has a granularity limited
 * by the FreeRTOS tick period (typically milliseconds), this function uses an `esp_timer`
 * and task notifications to achieve accurate blocking delays at the microsecond level.
 *
 * For very short delays where the overhead of the `esp_timer` mechanism would be
 * significant, it internally uses `ets_delay_us` (a busy-wait) based on a calibrated threshold.
 * This ensures efficiency for short delays while allowing the task to block cooperatively
 * (yielding the CPU) for longer delays.
 *
 * To achieve concurrency safety when calling this from multiple tasks, FreeRTOS task local storage is used.
 * If you are already using FreeRTOS' task local storage in your program, you need to ensure that
 * a) your configuration includes support for FreeRTOS task local storage, and
 * b) the task-local-storage-index you give is not used by another part of your program.
 *
 * @param us Microseconds to delay.
 *
 * @return
 *      - ESP_OK: Delay completed successfully.
 *      - ESP_ERR_INVALID_STATE: Timer is already running (should not typically occur with one-shot timers unless there's a logic error).
 *      - ESP_ERR_NO_MEM: Failed to create the timer due to lack of memory.
 *      - Other error codes returned by `esp_timer_create` or `esp_timer_start_once`.
 */
esp_err_t esp_microsleep_delay(uint64_t us);
#else
#warning esp_microsleep not available due to configuration mismatch. Please adjust CONFIG_ESP_MICROSLEEP_TLS_INDEX and CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD.
#endif // CONFIG_ESP_MICROSLEEP_TLS_INDEX && CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_MICROSLEEP_H
