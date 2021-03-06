/*
 * yadl.c
 *
 * Copyright (C) 2016-2017 Brian Masney <masneyb@onstation.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include "yadl.h"

#define DEFAULT_SLEEP_MILLIS_BETWEEN_RETRIES 500
#define DEFAULT_SLEEP_MILLIS_BETWEEN_SAMPLES 0
#define DEFAULT_SLEEP_MILLIS_BETWEEN_RESULTS 0
#define DEFAULT_MAX_RETRIES                 20
#define DEFAULT_NUM_SAMPLES_PER_RESULT      1
#define DEFAULT_NUM_RESULTS                 1
#define DEFAULT_REMOVE_N_SAMPLES_FROM_ENDS  0
#define DEFAULT_COUNTER_MULTIPLIER          1.0
#define DEFAULT_INTERRUPT_EDGE              "rising"
#define DEFAULT_ADC_MILLIVOLTS              3300
#define DEFAULT_ADC_MULTIPLIER              1.0
#define DEFAULT_ANALOG_SCALING_FACTOR       500

void usage(void)
{
	printf("usage: yadl --sensor <digital|counter|analog|dht11|dht22|ds18b20|tmp36|bmp180|bme280|argent_80422>\n");
	printf("\t--output <json|yaml|csv|xml|rrd|single_json> [ --output <...> ]\n");
	printf("\t[ --outfile <optional output filename. Defaults to stdout> [ --outfile <...> ] ]\n");
	printf("\t[ --only_log_value_changes ]\n");
	printf("\t[ --num_results <# results returned (default %d). Set to -1 to poll indefinitely.> ]\n", DEFAULT_NUM_RESULTS);
	printf("\t[ --sleep_millis_between_results <milliseconds (default %d)> ]\n", DEFAULT_SLEEP_MILLIS_BETWEEN_RESULTS);
	printf("\t[ --num_samples_per_result <# samples (default %d). See --filter for aggregation.> ]\n", DEFAULT_NUM_SAMPLES_PER_RESULT);
	printf("\t[ --sleep_millis_between_samples <milliseconds (default %d)> ]\n", DEFAULT_SLEEP_MILLIS_BETWEEN_SAMPLES);
	printf("\t[ --filter <median|mean|mode|sum|min|max|range (default median)> ]\n");
	printf("\t[ --remove_n_samples_from_ends <# samples (default %d)> ]\n",
	       DEFAULT_REMOVE_N_SAMPLES_FROM_ENDS);
	printf("\t[ --max_retries <# retries (default %d)> ]\n",
	       DEFAULT_MAX_RETRIES);
	printf("\t[ --sleep_millis_between_retries <milliseconds (default %d)> ]\n", DEFAULT_SLEEP_MILLIS_BETWEEN_RETRIES);
	printf("\t[ --debug ]\n");
	printf("\t[ --logfile <path to debug logs. Uses stderr if not specified.> ]\n");
	printf("\t[ --daemon ]\n");
	printf("\n");
	printf("Sensor Specific Options\n");
	printf("\n");
	printf("* digital - Reads from a digital pin\n");
	printf("\t--gpio_pin <wiringPi pin #. See http://wiringpi.com/pins/>\n");
	printf("\n");
	printf("* counter - Counts the number of times the digital pin state changes.\n");
	printf("\t--gpio_pin <wiringPi pin #. See http://wiringpi.com/pins/>\n");
	printf("\t[ --counter_multiplier <multiplier to convert the requests per second to some other value. (default %.1f)> ]\n", DEFAULT_COUNTER_MULTIPLIER);
	printf("\t[ --interrupt_edge <rising|falling|both (default %s)> ]\n",
	       DEFAULT_INTERRUPT_EDGE);
	printf("\n");
	printf("* analog\n");
	printf("\t--adc <see ADC options below>\n");
	printf("\n");
	printf("* dht11 / dht22 - Temperature and Humidity sensors\n");
	printf("\t--gpio_pin <wiringPi pin #. See http://wiringpi.com/pins/>\n");
	printf("\t--temperature_unit <celsius|fahrenheit|kelvin|rankine>\n");
	printf("\n");
	printf("* ds18b20 - Temperature sensor that uses the Dallas 1-Wire protocol.\n");
	printf("\t--w1_slave <w1 slave device. See /sys/bus/w1/devices/28-*/w1_slave>\n");
	printf("\t--temperature_unit <celsius|fahrenheit|kelvin|rankine>\n");
	printf("\n");
	printf("\tYou need to have the w1-gpio and w1-therm kernel modules loaded.\n");
	printf("\tYou'll also need to have 'dtoverlay=w1-gpio' in your /boot/config.txt\n");
	printf("\tand reboot if it was not already present.\n");
	printf("\n");
	printf("* tmp36 - Analog temperature sensor.\n");
	printf("\t--adc <see ADC options below>\n");
	printf("\t--temperature_unit <celsius|fahrenheit|kelvin|rankine>\n");
	printf("\t[ --analog_scaling_factor <value> (default %d) ]\n",
	       DEFAULT_ANALOG_SCALING_FACTOR);
	printf("\n");
	printf("* bmp180 - Temperature, pressure and altitude sensor.\n");
	printf("\t--i2c_address <I2C hex address. Use i2cdetect command to look up.>\n");
	printf("\t--temperature_unit <celsius|fahrenheit|kelvin|rankine>\n");
	printf("\n");
	printf("* bme280 - Temperature, humidity, pressure and altitude sensor.\n");
	printf("\t--i2c_address <I2C hex address. Use i2cdetect command to look up.>\n");
	printf("\t--temperature_unit <celsius|fahrenheit|kelvin|rankine>\n");
	printf("\n");
	printf("* argent_80422 - Wind vane, anemometer, and rain gauge.\n");
	printf("\t--wind_speed_pin <wiringPi pin #.>\n");
	printf("\t--wind_speed_unit <mph|kmh>\n");
	printf("\t--rain_gauage_pin <wiringPi pin #.>\n");
	printf("\t--rain_gauage_unit <in|mm>\n");
	printf("\t--adc <see ADC options below. This is for the wind vane.>\n");
	printf("\n");
	printf("ADC Options and Supported Types\n");
	printf("\t[ --adc_millivolts <value (default %d)> ]\n",
	       DEFAULT_ADC_MILLIVOLTS);
	printf("\t[ --adc_multiplier <value (default %.1f)> ]\n",
	       DEFAULT_ADC_MULTIPLIER);
	printf("\n");
	printf("\tThe --adc_multiplier can be used if have a voltage divider and\n");
	printf("\tand want to convert the reading back to the original value.\n");
	printf("\n");
	printf("* mcp3002 / mcp3004 / mcp3008 - 10-bit ADCs with a SPI interface.\n");
	printf("\t--spi_channel <spi channel. Either 0 or 1 for the Pi.>\n");
	printf("\t--analog_channel <analog channel>\n");
	printf("\n");
	printf("\tYou need to have the proper spi_bcmXXXX kernel module loaded on the Pi.\n");
	printf("\n");
	printf("* pcf8591 - 8-bit ADC with an I2C interface.\n");
	printf("\t--i2c_address <I2C hex address. Use i2cdetect command to look up.>\n");
	printf("\t--analog_channel <analog channel>\n");
	printf("\n");
	printf("Examples\n");
	printf("\n");
	printf("* Poll a DHT22 temperature sensor on BCM pin 17 (wiringPi pin 0) as JSON.\n");
	printf("  $ sudo yadl --gpio_pin 0 --sensor dht22 --temperature_unit fahrenheit --output json\n");
	printf("  { \"result\": [  { \"temperature\": 68.18, \"humidity\": 55.30, \"dew_point\": 51.55, \"temperature_unit\": \"F\", \"timestamp\": 1467648942 } ] }\n");
	printf("\n");
	printf("* Show 5 averaged results from an ADC. 1000 samples are taken for each result\n");
	printf("  shown. 200 samples from each end are removed and the mean is taken of the middle\n");
	printf("  600 samples. This is useful for removing noise from analog sensors. Wait 2 seconds\n");
	printf("  between each result shown.\n");
	printf("  $ yadl --sensor analog --adc mcp3008 --spi_channel 0 --analog_channel 0 \\\n");
	printf("	--output csv --num_results 5 --sleep_millis_between_results 2000 \\\n");
	printf("	--num_samples_per_result 1000 --remove_n_samples_from_ends 200 --filter mean\n");
	printf("  reading_number,timestamp,reading,millivolts\n");
	printf("  0,1465685840,96.0,309.0\n");
	printf("  1,1465685842,96.0,309.0\n");
	printf("  2,1465685844,96.0,309.0\n");
	printf("  3,1465685846,96.0,309.0\n");
	printf("  4,1465685848,96.0,309.0\n");
	printf("\n");
	printf("* See the files in the examples/ directory for more examples.\n");
	printf("\n");
	printf("* See systemd/pi-yadl-gatherer.service for an example writing the data to\n");
	printf("  a RRD database.\n");
	printf("\n");

	exit(1);
}

int get_num_values(yadl_config *config)
{
	char **header_names = config->sens->get_value_header_names(config);
	int i = 0;

	for (; header_names[i] != NULL; i++)
		;
	return i;
}

static void _free_result(yadl_result *result)
{
	free(result->value);
	if (result->unit != NULL)
		free(result->unit);
	free(result);
}

static yadl_result *_perform_reading(yadl_config *config)
{
	yadl_result *result;

	int success = 0;

	for (int i = 0; i < config->max_retries; i++) {
		if (i > 0 && config->sleep_millis_between_retries > 0)
			delay(config->sleep_millis_between_retries);

		result = config->sens->read(config);
		if (result == NULL) {
			/* bad reading */
			continue;
		}

		success = 1;
		break;
	}

	if (!success) {
		fprintf(stderr, "Error: Reached maximum retries.\n");
		exit(1);
	}

	return result;
}

static float_node *_add_sample_to_sorted_list(float value, float_node *list)
{
	float_node *newnode = malloc(sizeof(*newnode));

	newnode->value = value;
	newnode->next = NULL;

	if (list == NULL)
		return newnode;

	/* Add to head of list */
	if (list->value > value) {
		newnode->next = list;
		return newnode;
	}

	float_node *lastnode = list;

	/* Add to middle of list */
	for (float_node *cur = list->next; cur != NULL; cur = cur->next) {
		if (cur->value > value) {
			newnode->next = cur;
			lastnode->next = newnode;
			return list;
		}
		lastnode = cur;
	}

	/* Add to end of list */
	lastnode->next = newnode;
	return list;
}

static void _dump_list(char *description, float_node *list, yadl_config *config)
{
	config->logger("%s: Sorted values are:", description);
	for (float_node *val = list; val != NULL; val = val->next)
		config->logger(" %.2f", val->value);
	config->logger("\n");
}

static float_node *_remove_outliers(float_node *list, yadl_config *config)
{
	if (config->remove_n_samples_from_ends <= 0)
		return list;

	float_node *last_node_on_head_of_list = NULL;
	float_node *cur = list;

	for (int i = 0;
	     i < config->num_samples_per_result - config->remove_n_samples_from_ends - 1;
	     i++) {
		if (i == (config->remove_n_samples_from_ends - 1))
			last_node_on_head_of_list = cur;
		cur = cur->next;
	}

	/* Chop off the end of the list */
	free_list(cur->next);
	cur->next = NULL;

	/* Chop off the head of the list */
	float_node *new_head_of_list = last_node_on_head_of_list->next;

	last_node_on_head_of_list->next = NULL;
	free_list(list);

	return new_head_of_list;
}

static yadl_result *_perform_all_readings(yadl_config *config)
{
	float_node **value_list;

	char **header_names = config->sens->get_value_header_names(config);
	int num_values = get_num_values(config);

	value_list = calloc(num_values, sizeof(float *));

	char **unit_values = NULL;

	for (int i = 0; i < config->num_samples_per_result; i++) {
		if (i > 0 && config->sleep_millis_between_samples > 0)
			delay(config->sleep_millis_between_samples);

		yadl_result *sample = _perform_reading(config);

		/* Save the units for the returned result */
		if (i == 0 && sample->unit != NULL) {
			unit_values = sample->unit;
			sample->unit = NULL;
		}

		config->logger("Sample #%d", i);
		for (int num = 0; num < num_values; num++) {
			config->logger(", %s=%.2f", header_names[num],
				       sample->value[num]);
			value_list[num] = _add_sample_to_sorted_list(sample->value[num],
								     value_list[num]);
		}
		config->logger("\n");

		_free_result(sample);
	}

	for (int num = 0; num < num_values; num++) {
		_dump_list(header_names[num], value_list[num], config);
		value_list[num] = _remove_outliers(value_list[num], config);
		_dump_list("Values(after removing outliers)", value_list[num],
			   config);
	}

	yadl_result *result = malloc(sizeof(*result));

	result->unit = unit_values;
	result->value = malloc(sizeof(float) * num_values);
	for (int num = 0; num < num_values; num++) {
		result->value[num] = config->filter_func(value_list[num]);
		free_list(value_list[num]);
	}
	free(value_list);

	return result;
}

static void _dofork(yadl_config *config)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Error forking: %s\n", strerror(errno));
		exit(1);
	} else if (pid > 0) {
		config->logger("Terminating parent process %d\n", pid);
		/* In parent process */
		exit(0);
	}

	/* Now running in child process */
}

static void _daemonize(yadl_config *config)
{
	config->logger("Note: Running in daemon mode\n");

	if (chdir("/") < 0) {
		fprintf(stderr, "Error changing current path to /: %s\n",
			strerror(errno));
		exit(1);
	}

	_dofork(config);
	_dofork(config);

	if (setsid() < 0) {
		fprintf(stderr, "Error calling setsid(): %s\n",
			strerror(errno));
		exit(1);
	}

	close(0);
	close(1);
	close(2);
}

int main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"gpio_pin", required_argument, 0, 0 },
		{"output", required_argument, 0, 0 },
		{"sensor", required_argument, 0, 0 },
		{"debug", no_argument, 0, 0 },
		{"outfile", required_argument, 0, 0 },
		{"spi_channel", required_argument, 0, 0 },
		{"adc", required_argument, 0, 0 },
		{"max_retries", required_argument, 0, 0 },
		{"sleep_millis_between_retries", required_argument, 0, 0 },
		{"num_samples_per_result", required_argument, 0, 0 },
		{"analog_channel", required_argument, 0, 0 },
		{"sleep_millis_between_samples", required_argument, 0, 0 },
		{"filter", required_argument, 0, 0 },
		{"remove_n_samples_from_ends", required_argument, 0, 0 },
		{"num_results", required_argument, 0, 0 },
		{"sleep_millis_between_results", required_argument, 0, 0 },
		{"i2c_address", required_argument, 0, 0 },
		{"only_log_value_changes", no_argument, 0, 0 },
		{"counter_multiplier", required_argument, 0, 0 },
		{"logfile", required_argument, 0, 0 },
		{"daemon", no_argument, 0, 0 },
		{"interrupt_edge", required_argument, 0, 0 },
		{"adc_millivolts", required_argument, 0, 0 },
		{"temperature_unit", required_argument, 0, 0 },
		{"w1_slave", required_argument, 0, 0 },
		{"analog_scaling_factor", required_argument, 0, 0 },
		{"wind_speed_pin", required_argument, 0, 0 },
		{"rain_gauge_pin", required_argument, 0, 0 },
		{"wind_speed_unit", required_argument, 0, 0 },
		{"rain_gauge_unit", required_argument, 0, 0 },
		{"adc_multiplier", required_argument, 0, 0 },
		{0, 0, 0, 0 }
	};

	char *sensor_name = NULL, *adc_name = NULL;
	char *filter_name = NULL, *logfile = NULL;
	char *temperature_unit = NULL;
	yadl_config config;

	char **output_types = NULL;
	int num_output_types = 0;
	char **output_filenames = NULL;
	int num_output_filenames = 0;

	outputter **output_funcs = NULL;

	int opt = 0, long_index = 0, debug = 0, daemon = 0;

	if (argc <= 1)
		usage();

	memset(&config, 0, sizeof(config));
	config.gpio_pin = -1;
	config.spi_channel = -1;
	config.i2c_address = -1;
	config.max_retries = DEFAULT_MAX_RETRIES;
	config.sleep_millis_between_retries = DEFAULT_SLEEP_MILLIS_BETWEEN_RETRIES;
	config.sleep_millis_between_samples = DEFAULT_SLEEP_MILLIS_BETWEEN_SAMPLES;
	config.analog_channel = -1;
	config.num_results = DEFAULT_NUM_RESULTS;
	config.num_samples_per_result = DEFAULT_NUM_SAMPLES_PER_RESULT;
	config.remove_n_samples_from_ends = DEFAULT_REMOVE_N_SAMPLES_FROM_ENDS;
	config.sleep_millis_between_samples = DEFAULT_SLEEP_MILLIS_BETWEEN_SAMPLES;
	config.only_log_value_changes = 0;
	config.counter_multiplier = DEFAULT_COUNTER_MULTIPLIER;
	config.interrupt_edge = DEFAULT_INTERRUPT_EDGE;
	config.adc_millivolts = DEFAULT_ADC_MILLIVOLTS; /* Raspberry Pi GPIO pins are 3.3V */
	config.adc_multiplier = DEFAULT_ADC_MULTIPLIER;
	config.analog_scaling_factor = DEFAULT_ANALOG_SCALING_FACTOR;
	config.wind_speed_pin = -1;
	config.rain_gauge_pin = -1;

	while ((opt = getopt_long(argc, argv, "", long_options,
				  &long_index)) != -1) {
		if (opt != 0)
			usage();

		errno = 0;
		switch (long_index) {
		case 0:
			config.gpio_pin = strtol(optarg, NULL, 10);
			break;
		case 1:
			num_output_types++;
			output_types = realloc(output_types,
					       sizeof(char *) * num_output_types);
			output_types[num_output_types - 1] = optarg;
			break;
		case 2:
			sensor_name = optarg;
			break;
		case 3:
			debug = 1;
			break;
		case 4:
			num_output_filenames++;
			output_filenames = realloc(output_filenames,
						   sizeof(char *) * num_output_filenames);
			output_filenames[num_output_filenames - 1] = optarg;
			break;
		case 5:
			config.spi_channel = strtol(optarg, NULL, 10);
			break;
		case 6:
			adc_name = optarg;
			break;
		case 7:
			config.max_retries = strtol(optarg, NULL, 10);
			break;
		case 8:
			config.sleep_millis_between_retries = strtol(optarg, NULL, 10);
			break;
		case 9:
			config.num_samples_per_result = strtol(optarg, NULL, 10);
			break;
		case 10:
			config.analog_channel = strtol(optarg, NULL, 10);
			break;
		case 11:
			config.sleep_millis_between_samples = strtol(optarg, NULL, 10);
			break;
		case 12:
			filter_name = optarg;
			break;
		case 13:
			config.remove_n_samples_from_ends = strtol(optarg, NULL, 10);
			break;
		case 14:
			config.num_results = strtol(optarg, NULL, 10);
			break;
		case 15:
			config.sleep_millis_between_results = strtol(optarg, NULL, 10);
			break;
		case 16:
			config.i2c_address = strtol(optarg, NULL, 16);
			break;
		case 17:
			config.only_log_value_changes = 1;
			break;
		case 18:
			config.counter_multiplier = strtof(optarg, NULL);
			break;
		case 19:
			logfile = optarg;
			break;
		case 20:
			daemon = 1;
			break;
		case 21:
			config.interrupt_edge = optarg;
			break;
		case 22:
			config.adc_millivolts = strtol(optarg, NULL, 10);
			break;
		case 23:
			temperature_unit = optarg;
			break;
		case 24:
			config.w1_slave = optarg;
			break;
		case 25:
			config.analog_scaling_factor = strtol(optarg, NULL, 10);
			break;
		case 26:
			config.wind_speed_pin = strtol(optarg, NULL, 10);
			break;
		case 27:
			config.rain_gauge_pin = strtol(optarg, NULL, 10);
			break;
		case 28:
			config.wind_speed_unit = optarg;
			break;
		case 29:
			config.rain_gauge_unit = optarg;
			break;
		case 30:
			config.adc_multiplier = strtof(optarg, NULL);
			break;
		default:
			usage();
		}

		if (errno != 0)
			usage();
	}

	if (config.num_samples_per_result <= 0) {
		fprintf(stderr, "--num_samples_per_result must be > 0\n");
		usage();
	} else if (config.remove_n_samples_from_ends < 0) {
		fprintf(stderr, "--remove_n_samples_from_ends must be >= 0\n");
		usage();
	} else if (config.num_samples_per_result <= (config.remove_n_samples_from_ends * 2)) {
		fprintf(stderr, "--remove_n_samples_from_ends * 2 must be less than --num_samples_per_result\n");
		usage();
	} else if (daemon && debug && logfile == NULL) {
		fprintf(stderr, "You must specify the --logfile argument with --daemon\n");
		usage();
	} else if (daemon && num_output_filenames == 0) {
		fprintf(stderr, "You must specify the --outfile argument with --daemon\n");
		usage();
	}

	config.logger = get_logger(debug, logfile);

	config.sens = get_sensor(sensor_name);
	if (config.sens == NULL) {
		fprintf(stderr, "You must specify the --sensor argument\n");
		usage();
	}

	if (num_output_types == 0) {
		fprintf(stderr, "You must specify at least one --output argument\n");
		usage();
	} else if (num_output_types == 1 && num_output_filenames == 0) {
		// Send output to stdout if no filename was specified
		num_output_filenames++;
		output_filenames = realloc(output_filenames,
					   sizeof(char *) * num_output_filenames);
		output_filenames[num_output_filenames - 1] = NULL;
	} else if (num_output_types != num_output_filenames) {
		fprintf(stderr, "You must specify the same number of --output and --outfile arguments\n");
		usage();
	}

	output_funcs = malloc(sizeof(outputter *) * num_output_types);
	for (int output_idx = 0; output_idx < num_output_types; output_idx++) {
		output_funcs[output_idx] = get_outputter(output_types[output_idx]);
		if (output_funcs[output_idx] == NULL)
			usage();
	}

	config.filter_func = get_filter(filter_name);
	if (config.filter_func == NULL)
		usage();

	populate_temperature_converter(&config, temperature_unit);

	config.adc = get_adc(adc_name);

	config.logger("num_results=%d; sleep_millis_between_samples=%d; num_samples_per_result=%d; sleep_millis_between_samples=%d\n",
			config.num_results, config.sleep_millis_between_samples,
			config.num_samples_per_result,
			config.sleep_millis_between_samples);
	config.logger("max_retries=%d; sleep_millis_between_retries=%d\n",
			config.max_retries, config.sleep_millis_between_retries);

	int num_values = get_num_values(&config);

	config.last_values = malloc(sizeof(float) * num_values);

	if (wiringPiSetup() == -1)
		exit(1);

	if (daemon)
		_daemonize(&config);

	if (config.sens->init != NULL)
		config.sens->init(&config);

	output_metadata **output_metadatas = malloc(sizeof(output_metadata *) * num_output_types);

	for (int output_idx = 0; output_idx < num_output_types; output_idx++) {
		output_metadatas[output_idx] = output_funcs[output_idx]->open(&config,
									      output_filenames[output_idx]);

		if (output_funcs[output_idx]->write_header != NULL)
			output_funcs[output_idx]->write_header(output_metadatas[output_idx],
							       &config);
	}

	for (int i = 0; i < config.num_results || config.num_results < 0; i++) {
		if (i > 0 && config.sleep_millis_between_results > 0)
			delay(config.sleep_millis_between_results);

		yadl_result *result = _perform_all_readings(&config);

		for (int output_idx = 0; output_idx < num_output_types;
		     output_idx++) {
			output_funcs[output_idx]->write_result(output_metadatas[output_idx],
							       i, result,
							       &config);
		}

		_free_result(result);
	}

	for (int output_idx = 0; output_idx < num_output_types; output_idx++) {
		if (output_funcs[output_idx]->write_footer != NULL)
			output_funcs[output_idx]->write_footer(output_metadatas[output_idx]);

		if (output_funcs[output_idx]->close != NULL)
			output_funcs[output_idx]->close(output_metadatas[output_idx], &config);
	}

	close_logger(logfile);

	return 0;
}

