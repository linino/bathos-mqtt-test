/*
 * Publish jiffies every 0.5 seconds
 *
 * Copyright (c) DogHunter LLC and the Linino organization 2019
 * Author Davide Ciminaghi 2019
 */

#include <bathos/bathos.h>
#include <bathos/event.h>
#include <bathos/pipe.h>
#include <bathos/dev_ops.h>
#include <bathos/jiffies.h>
#include <ets_sys.h>
#include <osapi.h>
#include <user_interface.h>
#include <bathos/mqtt-client.h>
#include <bathos/netif.h>
#include <bathos/wifi.h>

enum program_state {
	WAITING_FOR_NETWORK_UP = 0,
	NETWORK_UP,
	BROKER_CONNECTION_REQUESTED,
	BROKER_CONNECTED,
};

static struct {
	enum program_state state;
	struct mqtt_bathos_client *client;
} global_data;

declare_event(publisher_ready);
declare_event(publisher_error);

static struct mqtt_bathos_client *new_client(vod)
{
	struct mqtt_bathos_client *out;
	static char client_id[32];
	uint8_t mac[6];
	static struct mqtt_client_data mcd = {
		.tcp_cdata = {
			.port = 1883,
		},
		.client_id = client_id,
		.will_topic = NULL,
		.will_message = NULL,
		.will_message_size = 0,
		.user_name = NULL,
		.password = NULL,
		.topic = NULL,
		.connect_flags = 0,
		.keep_alive = 400,
		.nbufs = 2,
		.bufsize = 32,
	};

	wifi_get_macaddr(STATION_IF, mac);
	/*
	 * clientid must be made of an even number of bytes otherwise
	 * we got a crash
	 */
	sprintf(client_id, "modulino-%2x-%2x-%2x ", mac[3], mac[4], mac[5]);

	/* test.mosquitto.org */
	IP4_ADDR(&mcd.tcp_cdata.remote_addr, 37,187,106,16);

	printf("initializing mqtt publisher, client id = %s\n", client_id);
	out = bathos_mqtt_publisher_init(&mcd,
					 &event_name(publisher_ready),
					 &event_name(publisher_error));
	if (!out)
		printf("error initializing publisher\n");
	return out;
}

static void *mqtt_test_periodic(void *arg)
{
	static char str[32];
	struct mqtt_bathos_client *client;

	switch (global_data.state) {
	case WAITING_FOR_NETWORK_UP:
	case BROKER_CONNECTION_REQUESTED:
		/* Just wait a little bit more */
		break;
	case NETWORK_UP:
		client = new_client();
		if (!client)
			break;
		global_data.client = NULL;
		global_data.state = BROKER_CONNECTION_REQUESTED;
		break;
	case BROKER_CONNECTED:
		sprintf(str, "%08lu", jiffies);
		if (bathos_mqtt_publish(global_data.client, "bathos-jiffies",
					str, strlen(str), 0) < 0) {
			bathos_mqtt_publisher_fini(global_data.client);
			global_data.client = NULL;
			global_data.state = NETWORK_UP;
			break;
		}
	}
	return NULL;
}

static void publisher_ready_handler(struct event_handler_data *d)
{
	struct mqtt_bathos_client *client = d->data;

	printf("publisher ready (client %p)\n", client);
	global_data.state = BROKER_CONNECTED;
	global_data.client = client;
}
declare_event_handler(publisher_ready, NULL,
		      publisher_ready_handler, NULL);

static void publisher_error_handler(struct event_handler_data *d)
{
	struct mqtt_bathos_client *client = d->data;

	printf("publisher error (client %p)\n", client);
	bathos_mqtt_publisher_fini(client);
	global_data.client = NULL;
	global_data.state = NETWORK_UP;
}
declare_event_handler(publisher_error, NULL,
		      publisher_error_handler, NULL);

static void netif_up_handler(struct event_handler_data *ed)
{
	if (global_data.state != WAITING_FOR_NETWORK_UP) {
		printf("unexpected netif up event\n");
		return;
	}
	if (global_data.client)
		printf("%s: unexpected non null client\n", __func__);
	global_data.state = NETWORK_UP;
}
declare_event_handler(netif_up, NULL, netif_up_handler, NULL);

static void netif_down_handler(struct event_handler_data *ed)
{
	printf("network down\n");
	global_data.state = WAITING_FOR_NETWORK_UP;
}
declare_event_handler(netif_down, NULL, netif_down_handler, NULL);


static int mqtt_test_init(void *arg)
{
}

static struct bathos_task __task t_mqtt_test = {
	.name = "mqtt-test", .period = HZ/2,
	.job = mqtt_test_periodic,
	.arg = NULL,
	.init = mqtt_test_init,
	.release = 1,
};
