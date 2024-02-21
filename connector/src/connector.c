#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <kos_net.h>

#define NK_USE_UNQUALIFIED_NAMES
#include "traffic_light/Connector.edl.h"

#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>

#define BUFFER_SIZE 1024
#define MAX_MODE_LEN 128

char serverIp[] = "172.28.65.87";
uint16_t serverPort = 8081;
bool isConnected = false;

struct traffic_light_mode {
    char mode_id[MAX_MODE_LEN];
    int direction;
    int red_duration;
    int yellow_duration;
    int green_duration;
    int disabled;
};

int get_traffic_light_configuration(char* response)
{
    char request[BUFFER_SIZE] = {0};
    snprintf(request, BUFFER_SIZE, "GET /mode/1 HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", serverIp);

    int socketDesc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketDesc < 0) {
        fprintf(stderr, "Error: failed to create a socket\n");
        return 0;
    }

    struct sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    serverAddress.sin_addr.s_addr = inet_addr(serverIp);
    if (connect(socketDesc, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
	    fprintf(stderr, "Error: failed to connect to the socket\n");
		close(socketDesc);
        return 0;
	}

    if (send(socketDesc, request, sizeof(request), 0) < 0) {
		fprintf(stderr, "Error: failed to send the message through the socket\n");
	    close(socketDesc);
		return 0;
	}

    ssize_t responseLength;
    char buffer[BUFFER_SIZE] = {0};
    while ((responseLength = recv(socketDesc, buffer, sizeof(buffer), 0)) > 0)
        strncat(response, buffer, responseLength);
    close(socketDesc);

    return 1;
}

int parse_field(char* str, char* fieldName, bool parseValue, char* result) {
    char* fieldStart = strstr(str, fieldName);
    if (!fieldStart)
        return 0;
    if (!parseValue)
        return 1;
    fieldStart += strlen(fieldName);
    bool isString = false;
    while (true) {
        if (fieldStart[0] == '\n')
            return 0;
        if (fieldStart[0] == '\"') {
            isString = true;
            fieldStart++;
            break;
        }
        if (fieldStart[0] >= '0' && fieldStart[0] <= '9')
            break;
        fieldStart++;
    }
    int i = 0;
    while(true) {
        if (fieldStart[0] == '\n')
            return 0;
        if (isString) {
            if (fieldStart[0] == '\"')
                break;
        }
        else {
            if (fieldStart[0] < '0' || fieldStart[0] > '9')
                break;
        }
        result[i] = fieldStart[0];
        i++;
        fieldStart++;
    }
    result[i] = '\0';
    return 1;
}

int parse_traffic_light_configuration(char* response, struct traffic_light_mode* mode) {
    char* json = strstr(response, "{");
    char value[MAX_MODE_LEN] = {0};

    if (parse_field(json, "\"request_id\"", true, value))
        strcpy(mode->mode_id, value);
    else
        strcpy(mode->mode_id, "");

    if (parse_field(json, "\"direction_1\"", false, value))
        mode->direction = 1;
    else if (parse_field(json, "\"direction_2\"", false, value))
        mode->direction = 2;
    else
        mode->direction = -1;

    if (parse_field(json, "\"red_duration_sec\"", true, value))
        mode->red_duration = atoi(value);
    else
        mode->red_duration = -1;

    if (parse_field(json, "\"yellow_duration_sec\"", true, value))
        mode->yellow_duration = atoi(value);
    else
        mode->yellow_duration = -1;

    if (parse_field(json, "\"green_duration_sec\"", true, value))
        mode->green_duration = atoi(value);
    else
        mode->green_duration = -1;

    if (parse_field(json, "\"disabled\"", false, value))
        mode->disabled = 1;
    else
        mode->disabled = 0;

    return 1;
}

static nk_err_t ConfigurationMessageImpl(__rtl_unused struct ConfigurationMessage *self,
    const ConfigurationMessage_SendConfiguration_req *req, const struct nk_arena *reqArena,
    ConfigurationMessage_SendConfiguration_res *res, struct nk_arena *resArena) {
    if (!isConnected) {
        if (!wait_for_network()) {
            fprintf(stderr, "Failed to connect...\n");
            return NK_EAGAIN;
        }
        isConnected = true;
    }

    char serverResponse[BUFFER_SIZE] = {0};
    if (!get_traffic_light_configuration(serverResponse)) {
        fprintf(stderr, "Failed to get configuration from the server...\n");
        return NK_EAGAIN;
    }

    struct traffic_light_mode mode;
    if (!parse_traffic_light_configuration(serverResponse, &mode)) {
        fprintf(stderr, "Failed to parse configuration...\n");
        return NK_EAGAIN;
    }

    res->config.configDirection = mode.direction;
    res->config.configRedDuration = mode.red_duration;
    res->config.configYellowDuration = mode.yellow_duration;
    res->config.configGreenDuration = mode.green_duration;
    res->config.configDisabled = mode.disabled;

    return NK_EOK;
}

static struct ConfigurationMessage *CreateIConfigurationMessageImpl(void) {
    static const struct ConfigurationMessage_ops Ops = {
        .SendConfiguration = ConfigurationMessageImpl
    };

    static ConfigurationMessage obj = {
        .ops = &Ops
    };

    return &obj;
}

int main(int argc, const char *argv[]) {
    ServiceId iid;
    Handle handle = ServiceLocatorRegister("connector_connection", NULL, 0, &iid);
    assert(handle != INVALID_HANDLE);

    NkKosTransport transport;
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    Connector_entity_req req;
    Connector_entity_res res;
    Connector_entity entity;
    Connector_entity_init(&entity, CreateIConfigurationMessageImpl());
    char reqBuffer[Connector_entity_req_arena_size];
    struct nk_arena reqArena = NK_ARENA_INITIALIZER(reqBuffer, reqBuffer + sizeof(reqBuffer));
    char resBuffer[Connector_entity_res_arena_size];
    struct nk_arena resArena = NK_ARENA_INITIALIZER(resBuffer, resBuffer + sizeof(resBuffer));

    fprintf(stderr, "Hello I'm Connector\n");

    while (1) {
        nk_req_reset(&req);
        nk_arena_reset(&reqArena);
        if (nk_transport_recv(&transport.base, &req.base_, &reqArena) == NK_EOK)
            Connector_entity_dispatch( &entity, &req.base_, &reqArena, &res.base_, RTL_NULL);

        nk_transport_reply(&transport.base, &res.base_, &resArena);
    }

    return EXIT_SUCCESS;
}