# esp-microsleep — agent notes

## Aborted-delay handling: reconsider once the IDF floor is 6.x

The `xTaskAbortDelay` path in `esp_microsleep_delay()` (esp_microsleep.c) contains a
workaround that only exists because esp_timer (up to and including IDF 5.x) offers no way
to wait out an in-flight callback:

- On expiry, esp_timer removes the one-shot from its list (disarming it) and releases its
  lock *before* invoking the callback. A concurrent `esp_timer_stop()` therefore fails
  with `ESP_ERR_INVALID_STATE` while the callback — and with it the task notification —
  is still on its way. (IDF 6.x's `esp_timer_stop()` header doc acknowledges this
  explicitly: it "does NOT preempt an already-dispatched or currently-running callback".)
- The workaround consumes that notification
  (`while (xTaskNotifyWait(...) != pdTRUE) {}`) before returning `ESP_ERR_TIMEOUT`, so
  the notification can neither hit an already-deleted task (use-after-free of the TCB)
  nor truncate the task's next delay.

IDF 6.x adds the public API `esp_timer_stop_blocking(timer, timeout_ticks)`
(components/esp_timer/include/esp_timer.h): it disarms the timer *and* waits for a
running callback to finish (the `FL_CALLBACK_IS_RUNNING` flag in esp_timer.c). It
returns `ESP_OK` only once no callback is running or will run.

**When `idf_component.yml` bumps `compatible` from `>=5.0` to `>=6.0`, replace the
stop-fail/notification-consume dance with:**

```c
if (esp_timer_stop_blocking(timer, portMAX_DELAY) == ESP_OK) {
    xTaskNotifyStateClear(NULL); // discard a notification the expiry already delivered
} else {
    while (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) != pdTRUE) {} // callback still in flight: consume it
}
```

Caveats:

- The drain remains mandatory either way. `esp_timer_stop_blocking()` guarantees no
  notification is still *in flight*, but if the expiry already fired, its notification
  is *pending* (notification state `RECEIVED` at the default index) and would end the
  task's next delay immediately.
- The default-notification-slot (index 0) ownership documented in esp_microsleep.h
  stays relevant with either implementation; see there why the index is fixed at 0 and
  not sdkconfig-configurable.
