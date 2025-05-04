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
#include "esp_microsleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

#if defined(CONFIG_ESP_MICROSLEEP_TLS_INDEX) && defined(CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD)

static uint64_t esp_microsleep_compensation = 0;

/**
 * @brief ISR handler called when the one-shot timer expires.
 *        Notifies the task that initiated the delay.
 */
static void IRAM_ATTR esp_microsleep_isr_handler(void* arg) {
    TaskHandle_t task = (TaskHandle_t)(arg);
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(task, &higherPriorityTaskWoken);
    esp_timer_isr_dispatch_need_yield();
}

uint64_t esp_microsleep_calibrate() {

    const int calibration_loops = 10;
    const uint64_t calibration_usec = 100;
    uint64_t compensation = 0;

    esp_microsleep_delay(0); // to preheat the timer for this task
    for (int i = 0; i < calibration_loops; i++) {
        uint64_t start = esp_timer_get_time();
        esp_microsleep_delay(calibration_usec);
        uint64_t diff = esp_timer_get_time() - start - calibration_usec;
        compensation += diff;
    }
    esp_microsleep_compensation = compensation / calibration_loops;
    return esp_microsleep_compensation;
}

void esp_microsleep_delay(uint64_t us) {

    // Retrieve the timer handle associated with the calling task from Task Local Storage (TLS).
    // This allows each task to have its own timer instance without needing global variables
    // or complex management, especially crucial since timers are associated with specific task notifications.
    esp_timer_handle_t timer = (esp_timer_handle_t) pvTaskGetThreadLocalStoragePointer(NULL, CONFIG_ESP_MICROSLEEP_TLS_INDEX);
    if (!timer) {
        // If no timer exists for this task, create one.
        const esp_timer_create_args_t oneshot_timer_args = {
            .callback = esp_microsleep_isr_handler,
            .arg = (void*) xTaskGetCurrentTaskHandle(), // Pass task handle to ISR
            .dispatch_method = ESP_TIMER_ISR, // Use ISR dispatch for minimal latency
        };
        ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &timer));
        // Store the newly created timer handle in the task's TLS for future use.
        vTaskSetThreadLocalStoragePointer(NULL, CONFIG_ESP_MICROSLEEP_TLS_INDEX, (void*) timer);
        // Consider adding a task deletion callback here to delete the timer when the task exits.
    }

    if (us == 0) { return; } // No delay requested.

    // Compensation Logic:
    // Setting up and handling the esp_timer introduces a small overhead.
    // For very short delays, this overhead can be significant compared to the requested delay.
    // 'esp_microsleep_compensation' (calculated by esp_microsleep_calibrate) estimates this overhead.
    // If the requested delay 'us' is less than or equal to the compensation value,
    // it's more efficient to use the busy-waiting ets_delay_us function directly.
    if (us <= esp_microsleep_compensation) {
        ets_delay_us(us);
        return;
    }

    // For delays longer than the compensation overhead, use the esp_timer.
    // Start the one-shot timer for the requested duration minus the compensation value.
    ESP_ERROR_CHECK(esp_timer_start_once(timer, us - esp_microsleep_compensation));
    // Wait for notification from the ISR handler indicating the timer has expired.
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY); // or ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

#endif // CONFIG_ESP_MICROSLEEP_TLS_INDEX
