#include <pico/stdlib.h>
#include <hardware/rtc.h>
#include <pico/util/datetime.h>

#define LED_GREEN 21
#define LED_RED 22

static void alarm_callback()
{
	datetime_t t = {0};
	rtc_get_datetime(&t);

	if ( (t.hour == 7) && (t.min >= 15) ) {
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 1);
	} else if ( (t.hour >= 8) && (t.hour < 12) ) {
		gpio_put(LED_RED, 1);
		gpio_put(LED_GREEN, 1);
	} else {
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 0);
	}
}


static void start_demo()
{
	int i;
	for ( i=0; i<10; i++ ) {
		gpio_put(LED_RED, 1);
		gpio_put(LED_GREEN, 0);
		sleep_ms(250);
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 1);
		sleep_ms(250);
	}
}


int main()
{
	gpio_init(LED_GREEN);
	gpio_init(LED_RED);
	gpio_set_dir(LED_GREEN, GPIO_OUT);
	gpio_set_dir(LED_RED, GPIO_OUT);

	datetime_t t = {
		.year  = 2023,
		.month = 2,
		.day   = 5,
		.dotw  = 0, /* 0=Sunday */
		.hour  = 22,
		.min   = 25,
		.sec   = 0
	};
	rtc_init();
	rtc_set_datetime(&t);

	/* Prove we're awake, then reset everything */
	start_demo();
	gpio_put(LED_RED, 0);
	gpio_put(LED_GREEN, 0);

	datetime_t alarm = {
		.year  = -1,
		.month = -1,
		.day   = -1,
		.dotw  = -1,
		.hour  = -1,
		.min   = -1,
		.sec   = 0
	};

	rtc_set_alarm(&alarm, &alarm_callback);

	while (1) {
		sleep_ms(1000);
	}
}
