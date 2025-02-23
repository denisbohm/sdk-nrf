/* Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <modem/location.h>
#include <zephyr/logging/log.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <net/nrf_cloud_agps.h>
#include <net/nrf_cloud_pgps.h>

#include "location_tracking.h"

LOG_MODULE_REGISTER(location_tracking, CONFIG_MQTT_MULTI_SERVICE_LOG_LEVEL);

static location_update_cb_t location_update_handler;
static bool location_initialized;

static void location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_DBG("Location Event: Got location");
		if (location_update_handler) {
			/* Pass received location data along to our handler. */
			location_update_handler(&event_data->location);
		}
		break;

	case LOCATION_EVT_TIMEOUT:
		LOG_DBG("Location Event: Timed out");
		break;

	case LOCATION_EVT_ERROR:
		LOG_DBG("Location Event: Error");
		break;

	case LOCATION_EVT_GNSS_ASSISTANCE_REQUEST:
		LOG_DBG("Location Event: GNSS assistance requested");
		break;

	default:
		LOG_DBG("Location Event: Unknown event");
		break;
	}
}

int start_location_tracking(location_update_cb_t handler_cb, int interval)
{
	int err;

	if (!date_time_is_valid()) {
		LOG_WRN("Date and time unknown. Location Services results may suffer");
	}

	/* Update the location update handler. */
	location_update_handler = handler_cb;

	/* Initialize the Location Services Library. */
	err = location_init(location_event_handler);
	if (err) {
		LOG_ERR("Initializing the Location library failed, error: %d", err);
		return err;
	}
	location_initialized = true;

	/* Construct a request for a periodic location report. */
	struct location_config config;

	/* Select methods based on configuration and load default settings */
	if (IS_ENABLED(CONFIG_LOCATION_TRACKING_GNSS) &&
	    IS_ENABLED(CONFIG_LOCATION_TRACKING_CELLULAR)) {
		/* First try GNSS, fallback to cellular. */
		enum location_method methods[] = {LOCATION_METHOD_GNSS, LOCATION_METHOD_CELLULAR};

		/* Load default settings accordingly */
		location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);

		LOG_DBG("Selected GNSS, Cellular positioning methods");
	} else if (IS_ENABLED(CONFIG_LOCATION_TRACKING_GNSS)) {
		/* Only try GNSS. */
		enum location_method methods[] = {LOCATION_METHOD_GNSS};

		/* Load default settings accordingly */
		location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);

		LOG_DBG("Selected GNSS positioning method");
	} else if (IS_ENABLED(CONFIG_LOCATION_TRACKING_CELLULAR)) {
		/* Only try cellular. */
		enum location_method methods[] = {LOCATION_METHOD_CELLULAR};

		/* Load default settings accordingly */
		location_config_defaults_set(&config, ARRAY_SIZE(methods), methods);

		LOG_DBG("Selected Cellular positioning method");
	} else {
		/* No location methods are enabled. Just return without initiating any tracking. */
		LOG_DBG("No positioning methods selected");
		return 0;
	}

	/* Load default settings given the selected methods. */


	/* Set the location report interval. */
	config.interval = interval;

	/* If GNSS location method is enabled, configure it
	 * (note: the GNSS location method is enabled by default in this sample)
	 */
	if (IS_ENABLED(CONFIG_LOCATION_TRACKING_GNSS)) {
		/* Set the GNSS timeout and desired accuracy. */
		config.methods[0].gnss.timeout = CONFIG_GNSS_FIX_TIMEOUT_SECONDS * MSEC_PER_SEC;
		config.methods[0].gnss.accuracy = LOCATION_ACCURACY_NORMAL;
	}

	/* Submit request for periodic location report.
	 * This will cause the configured location_event_handler to start being called with
	 * location data.
	 */
	err = location_request(&config);
	if (err) {
		LOG_ERR("Requesting location failed, error: %d\n", err);
		return err;
	}
	return 0;
}
