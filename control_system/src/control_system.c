
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Files required for transport initialization. */
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>

/* Description of the lights gpio interface used by the `ControlSystem` entity. */
#include <traffic_light/IMode.idl.h>

#include <assert.h>

#include <unistd.h>
#include <string.h>
#define NK_USE_UNQUALIFIED_NAMES
#include <traffic_light/ConfigurationMessage.idl.h>

/* Control system entity entry point. */
int main(int argc, const char *argv[]) {
    /* Communication with 'Connector' */
    Handle handleConnector = ServiceLocatorConnect("connector_connection");
    assert(handleConnector != INVALID_HANDLE);

    NkKosTransport transportConnector;
    NkKosTransport_Init(&transportConnector, handleConnector, NK_NULL, 0);

    nk_iid_t riidConnector = ServiceLocatorGetRiid(handleConnector, "traffic_light.Connector.requestConfiguration");
    assert(riidConnector != INVALID_RIID);

    struct ConfigurationMessage_proxy proxyConnector;
    ConfigurationMessage_proxy_init(&proxyConnector, &transportConnector.base, riidConnector);

    ConfigurationMessage_SendConfiguration_req reqConnector;
    ConfigurationMessage_SendConfiguration_res resConnector;

    fprintf(stderr, "Hello I'm ControlSystem\n");

    ConfigurationMessage_SendConfiguration(&(proxyConnector.base), &reqConnector, NULL, &resConnector, NULL);

    if (resConnector.config.configDisabled) {
        fprintf(stderr, "Light is disabled\n");
        return EXIT_SUCCESS;
    }

    uint32_t modes[3];
    switch (resConnector.config.configDirection) {
    case 1:
        modes[0] = traffic_light_IMode_Direction1Red + traffic_light_IMode_Direction2Green;
        modes[1] = traffic_light_IMode_Direction1Yellow + traffic_light_IMode_Direction2Yellow;
        modes[2] = traffic_light_IMode_Direction1Green + traffic_light_IMode_Direction2Red;
        break;
    case 2:
        modes[0] = traffic_light_IMode_Direction1Green + traffic_light_IMode_Direction2Red;
        modes[1] = traffic_light_IMode_Direction1Yellow + traffic_light_IMode_Direction2Yellow;
        modes[2] = traffic_light_IMode_Direction1Red + traffic_light_IMode_Direction2Green;
        break;
    default:
        fprintf(stderr, "Unknown direction\n");
        return EXIT_FAILURE;
    }
    int durations[3] = { resConnector.config.configRedDuration, resConnector.config.configYellowDuration, resConnector.config.configGreenDuration };
    int i = 0;
    int s = 1;

    /**
     * Get the LightsGPIO IPC handle of the connection named
     * "lights_gpio_connection".
     */
    Handle handle = ServiceLocatorConnect("lights_gpio_connection");
    assert(handle != INVALID_HANDLE);

    /* Initialize IPC transport for interaction with the lights gpio entity. */
    NkKosTransport transport;
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    /**
     * Get Runtime Interface ID (RIID) for interface traffic_light.Mode.mode.
     * Here mode is the name of the traffic_light.Mode component instance,
     * traffic_light.Mode.mode is the name of the Mode interface implementation.
     */
    nk_iid_t riid = ServiceLocatorGetRiid(handle, "lightsGpio.mode");
    assert(riid != INVALID_RIID);

    /**
     * Initialize proxy object by specifying transport (&transport)
     * and lights gpio interface identifier (riid). Each method of the
     * proxy object will be implemented by sending a request to the lights gpio.
     */
    struct traffic_light_IMode_proxy proxy;
    traffic_light_IMode_proxy_init(&proxy, &transport.base, riid);

    /* Request and response structures */
    traffic_light_IMode_FMode_req req;
    traffic_light_IMode_FMode_res res;

    while (1) {
        req.value = modes[i];
        if (traffic_light_IMode_FMode(&proxy.base, &req, NULL, &res, NULL) == rcOk) {
            fprintf(stderr, "Result = %0x for %d s\n", (int)res.result, durations[i]);
            req.value = res.result;
        }
        else
            fprintf(stderr, "Failed to call traffic_light.Mode.Mode()\n");
        sleep(durations[i]);
        i += s;
        if (i == 2)
            s = -1;
        else if (i == 0)
            s = 1;
    }

    return EXIT_SUCCESS;
}
