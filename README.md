# esp-microsleep

A replacement for FreeRTOS `vTaskDelay` with subtick granularity.

## Why?

Due to the way FreeRTOS works, it is impossible to achieve fine-grained
delays in the millisecond or sub-millisecond region.
Busy waiting is no alternative, since you will a) burn cycles and b) might
trigger the FreeRTOS task watchdog.

## How?

This little ESP-IDF component fixes this by leveraging the `esp_timer`
subsystem. The currently running task starts a timer and waits
for a notification from the timer's ISR.

## Installation

1. Adjust your `idf_component.yml` file to depend on this component:

```yml
dependencies:
    mickeyl/esp_microsleep:
        git: "https://github.com/mickeyl/esp-microsleep.git"
```

2. Configure your project to allow for esp_timer dispatching via ISR:

   `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y`
3. Configure the proper amount of thread local storage.
   If you're not using thread local storage elsewhere in your app, 2 will be enough:

   `CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS=2`
4. Configure the appropriate thread local storage index:

   `CONFIG_ESP_MICROSLEEP_TLS_INDEX=1`

## Usage

```c
// Include the header file.
#include <esp_microsleep.h>

// Calibrate the compensation value (for more accurate sleep times).
esp_microsleep_calibrate();

// Call to delay the currently running task for 400 µs.
esp_microsleep_delay(400);
```

## Implementation Notes

While the task is "waiting" for the notification to arrive,
it is suspended via `xTaskNotifyWait`.

Since it takes a while from the timer alarm
to get the task notification processed, you
may achieve slightly longer sleep times than requested.

To compensate for that, you should call `esp_microsleep_calibrate()`
which computes a value suitable for your system.

## License

MIT.

## Maintainer

This component is maintained by Dr. Michael 'Mickey' Lauer <mlauer@vanille-media.de>
