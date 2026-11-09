/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <limits.h>
#include <stdint.h>
#include <sys/time.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct BMXESP8266CalendarDateTime {
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t millisecond;
    int32_t utc;
    int32_t offset;
    int32_t dst;
} BMXESP8266CalendarDateTime;

static esp_timer_handle_t bmx_esp8266_calendar_alarm;
static volatile uint32_t bmx_esp8266_calendar_alarm_events;
static volatile int32_t bmx_esp8266_calendar_alarm_active;
static volatile int32_t bmx_esp8266_calendar_running;

static int32_t bmx_esp8266_calendar_days_in_month(int32_t year, int32_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    int32_t result = days[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) ++result;
    return result;
}

static int32_t bmx_esp8266_calendar_valid(const BMXESP8266CalendarDateTime *value) {
    return value && value->year >= 1970 && value->year <= 2038 &&
        value->month >= 1 && value->month <= 12 && value->day >= 1 &&
        value->day <= bmx_esp8266_calendar_days_in_month(value->year, value->month) &&
        value->hour >= 0 && value->hour <= 23 && value->minute >= 0 &&
        value->minute <= 59 && value->second >= 0 && value->second <= 59 &&
        value->millisecond >= 0 && value->millisecond <= 999;
}

static int64_t bmx_esp8266_calendar_days_from_civil(
        int32_t year, uint32_t month, uint32_t day) {
    year -= month <= 2;
    int32_t era = (year >= 0 ? year : year - 399) / 400;
    uint32_t year_of_era = (uint32_t)(year - era * 400);
    uint32_t day_of_year =
        (153u * (month + (month > 2 ? (uint32_t)-3 : 9u)) + 2u) / 5u + day - 1u;
    uint32_t day_of_era = year_of_era * 365u + year_of_era / 4u -
        year_of_era / 100u + day_of_year;
    return (int64_t)era * 146097 + (int64_t)day_of_era - 719468;
}

static void bmx_esp8266_calendar_civil_from_days(
        int64_t days, int32_t *year, int32_t *month, int32_t *day) {
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    uint32_t day_of_era = (uint32_t)(days - era * 146097);
    uint32_t year_of_era = (day_of_era - day_of_era / 1460u +
        day_of_era / 36524u - day_of_era / 146096u) / 365u;
    int32_t y = (int32_t)year_of_era + (int32_t)era * 400;
    uint32_t day_of_year = day_of_era - (365u * year_of_era +
        year_of_era / 4u - year_of_era / 100u);
    uint32_t month_prime = (5u * day_of_year + 2u) / 153u;
    uint32_t d = day_of_year - (153u * month_prime + 2u) / 5u + 1u;
    uint32_t m = month_prime + (month_prime < 10u ? 3u : (uint32_t)-9);
    y += m <= 2u;
    *year = y;
    *month = (int32_t)m;
    *day = (int32_t)d;
}

static int32_t bmx_esp8266_calendar_to_timeval(
        const BMXESP8266CalendarDateTime *value, struct timeval *result) {
    if (!result || !bmx_esp8266_calendar_valid(value)) return 0;
    int64_t seconds = bmx_esp8266_calendar_days_from_civil(value->year,
        (uint32_t)value->month, (uint32_t)value->day) * 86400;
    seconds += value->hour * 3600 + value->minute * 60 + value->second;
    if (!value->utc) {
        seconds -= (int64_t)value->offset * 60;
        if (value->dst == 1) seconds -= 3600;
    }
    if (seconds < 0 || seconds > INT32_MAX) return 0;
    result->tv_sec = (time_t)seconds;
    result->tv_usec = value->millisecond * 1000;
    return 1;
}

static void bmx_esp8266_calendar_alarm_callback(void *argument) {
    (void)argument;
    taskENTER_CRITICAL();
    bmx_esp8266_calendar_alarm_active = 0;
    if (bmx_esp8266_calendar_alarm_events != UINT32_MAX)
        ++bmx_esp8266_calendar_alarm_events;
    taskEXIT_CRITICAL();
}

static int32_t bmx_esp8266_calendar_alarm_create(void) {
    if (bmx_esp8266_calendar_alarm) return 1;
    const esp_timer_create_args_t arguments = {
        .callback = bmx_esp8266_calendar_alarm_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "bmx_calendar"
    };
    return esp_timer_create(&arguments, &bmx_esp8266_calendar_alarm) == ESP_OK;
}

int32_t bmx_esp8266_calendar_start(const BMXESP8266CalendarDateTime *value) {
    struct timeval time;
    if (!bmx_esp8266_calendar_to_timeval(value, &time) ||
            settimeofday(&time, NULL) != 0) return 0;
    bmx_esp8266_calendar_running = 1;
    return 1;
}

void bmx_esp8266_calendar_disable_alarm(void);

void bmx_esp8266_calendar_stop(void) {
    bmx_esp8266_calendar_disable_alarm();
    bmx_esp8266_calendar_running = 0;
}

int32_t bmx_esp8266_calendar_set(const BMXESP8266CalendarDateTime *value) {
    return bmx_esp8266_calendar_start(value);
}

int32_t bmx_esp8266_calendar_is_running(void) {
    return bmx_esp8266_calendar_running;
}

int32_t bmx_esp8266_calendar_get(BMXESP8266CalendarDateTime *value) {
    if (!value || !bmx_esp8266_calendar_running) return 0;
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0 || now.tv_sec < 0) return 0;
    int64_t seconds = now.tv_sec;
    int64_t days = seconds / 86400;
    int64_t within_day = seconds % 86400;
    BMXESP8266CalendarDateTime result = {0};
    bmx_esp8266_calendar_civil_from_days(days,
        &result.year, &result.month, &result.day);
    result.hour = (int32_t)(within_day / 3600);
    result.minute = (int32_t)((within_day / 60) % 60);
    result.second = (int32_t)(within_day % 60);
    result.millisecond = (int32_t)(now.tv_usec / 1000);
    result.utc = 1;
    result.offset = 0;
    result.dst = 0;
    *value = result;
    return 1;
}

uint64_t bmx_esp8266_calendar_resolution_nanoseconds(void) {
    return 1000u;
}

int32_t bmx_esp8266_calendar_set_alarm(
        const BMXESP8266CalendarDateTime *value, int32_t wake_from_low_power) {
    struct timeval target;
    struct timeval now;
    if (wake_from_low_power || !bmx_esp8266_calendar_running ||
            !bmx_esp8266_calendar_to_timeval(value, &target) ||
            gettimeofday(&now, NULL) != 0 ||
            !bmx_esp8266_calendar_alarm_create()) return 0;
    int64_t delay = ((int64_t)target.tv_sec - (int64_t)now.tv_sec) * 1000000 +
        ((int64_t)target.tv_usec - (int64_t)now.tv_usec);
    if (delay <= 0) return 0;
    /* Unlike ESP-IDF's newer esp_timer, the ESP8266 SDK implementation is
       backed by a FreeRTOS software timer and rejects sub-tick remainders. */
    const uint64_t tick_us = (uint64_t)portTICK_PERIOD_MS * 1000u;
    delay = (int64_t)(((uint64_t)delay + tick_us - 1u) / tick_us * tick_us);
    taskENTER_CRITICAL();
    int32_t active = bmx_esp8266_calendar_alarm_active;
    bmx_esp8266_calendar_alarm_active = 0;
    bmx_esp8266_calendar_alarm_events = 0;
    taskEXIT_CRITICAL();
    if (active) (void)esp_timer_stop(bmx_esp8266_calendar_alarm);
    taskENTER_CRITICAL();
    bmx_esp8266_calendar_alarm_active = 1;
    taskEXIT_CRITICAL();
    if (esp_timer_start_once(bmx_esp8266_calendar_alarm, (uint64_t)delay) != ESP_OK) {
        taskENTER_CRITICAL();
        bmx_esp8266_calendar_alarm_active = 0;
        taskEXIT_CRITICAL();
        return 0;
    }
    return 1;
}

void bmx_esp8266_calendar_disable_alarm(void) {
    taskENTER_CRITICAL();
    int32_t active = bmx_esp8266_calendar_alarm_active;
    bmx_esp8266_calendar_alarm_active = 0;
    bmx_esp8266_calendar_alarm_events = 0;
    taskEXIT_CRITICAL();
    if (active && bmx_esp8266_calendar_alarm)
        (void)esp_timer_stop(bmx_esp8266_calendar_alarm);
}

uint32_t bmx_esp8266_calendar_pending_alarm_events(void) {
    taskENTER_CRITICAL();
    uint32_t result = bmx_esp8266_calendar_alarm_events;
    taskEXIT_CRITICAL();
    return result;
}

uint32_t bmx_esp8266_calendar_take_alarm_events(void) {
    taskENTER_CRITICAL();
    uint32_t result = bmx_esp8266_calendar_alarm_events;
    bmx_esp8266_calendar_alarm_events = 0;
    taskEXIT_CRITICAL();
    return result;
}
