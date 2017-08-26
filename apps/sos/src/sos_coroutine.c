#include <clock/clock.h>
#include <sos_coroutine.h>
#include <stdint.h>
#include <utils/picoro.h>

void coro_sleep_awaken(uint32_t id, void* data) { resume((coro)data, 0); }

int coro_sleep(uint64_t delay) {
	if (!register_timer(delay, coro_sleep_awaken, current_coro())) {
		return -1;
	}
	yield(0);
	return 0;
}