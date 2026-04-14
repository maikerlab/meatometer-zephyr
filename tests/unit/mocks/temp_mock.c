/* SPDX-License-Identifier: Apache-2.0 */

#include "temp_mock.h"
#include "temperature.h"

static bool started;

int temperature_init(struct k_msgq *queue)
{
	(void)queue;
	started = false;
	return 0;
}

void temperature_start(void)
{
	started = true;
}

void temperature_stop(void)
{
	started = false;
}

void temp_mock_reset(void)
{
	started = false;
}

bool temp_mock_is_started(void)
{
	return started;
}
