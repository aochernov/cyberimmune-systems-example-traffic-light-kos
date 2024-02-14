
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Files required for transport initialization. */
#include <coresrv/nk/transport-kos.h>
#include <coresrv/sl/sl_api.h>

/* EDL description of the LightsGPIO entity. */
#include <traffic_light/LightsGPIO.edl.h>

#include <assert.h>

#include <string.h>
#define NK_USE_UNQUALIFIED_NAMES
#include <traffic_light/DiagnosticsMessage.idl.h>

/* Type of interface implementing object. */
typedef struct IModeImpl {
    struct traffic_light_IMode base;     /* Base interface of object */
    rtl_uint32_t step;                   /* Extra parameters */
} IModeImpl;

/* Mode method implementation. */
static nk_err_t FMode_impl(struct traffic_light_IMode *self,
                          const struct traffic_light_IMode_FMode_req *req,
                          const struct nk_arena *req_arena,
                          traffic_light_IMode_FMode_res *res,
                          struct nk_arena *res_arena)
{
    IModeImpl *impl = (IModeImpl *)self;
    /**
     * Increment value in control system request by
     * one step and include into result argument that will be
     * sent to the control system in the lights gpio response.
     */
    res->result = req->value + impl->step;
    return NK_EOK;
}

/**
 * IMode object constructor.
 * step is the number by which the input value is increased.
 */
static struct traffic_light_IMode *CreateIModeImpl(rtl_uint32_t step)
{
    /* Table of implementations of IMode interface methods. */
    static const struct traffic_light_IMode_ops ops = {
        .FMode = FMode_impl
    };

    /* Interface implementing object. */
    static struct IModeImpl impl = {
        .base = {&ops}
    };

    impl.step = step;

    return &impl.base;
}

void lightCodeToString(int code, char* string) {
    switch (code & 0x7) {
    case 0x1:
        strcat(string, "red");
        break;
    case 0x2:
        strcat(string, "yellow");
        break;
    case 0x4:
        strcat(string, "green");
        break;
    default:
        strcat(string, "UNKNOWN");
        break;
    }
    if (code & 0x8)
        strcat(string, " blinking");
}

void SendMessage(uint32_t id, char* fst, char* snd, struct DiagnosticsMessage_proxy *proxy,
    DiagnosticsMessage_SendDiagnosticsMessage_req *req, DiagnosticsMessage_SendDiagnosticsMessage_res *res,
    struct nk_arena *reqArena, struct nk_arena *resArena) {

    nk_arena_reset(reqArena);

    nk_ptr_t *message = nk_arena_alloc(nk_ptr_t, reqArena, &(req->message), 1);
    if (message == RTL_NULL)
        return;

    char data[traffic_light_DiagnosticsMessage_SendDiagnosticsMessage_req_message_size] = {0};
    int length = snprintf(data, traffic_light_DiagnosticsMessage_SendDiagnosticsMessage_req_message_size,
        "Message #%u: first light -- %s, second light -- %s", id, fst, snd);
    if (length < 0)
        return;

    nk_char_t *str = nk_arena_alloc(nk_char_t, reqArena, &message[0], (nk_size_t)(length + 1));
    if (str == RTL_NULL)
        return;

    strncpy(str, data, (rtl_size_t)(length + 1));

    DiagnosticsMessage_SendDiagnosticsMessage(&proxy->base, req, reqArena, res, resArena);
}

/* Lights GPIO entry point. */
int main(void)
{
    NkKosTransport transport;
    ServiceId iid;

    /* Get lights gpio IPC handle of "lights_gpio_connection". */
    Handle handle = ServiceLocatorRegister("lights_gpio_connection", NULL, 0, &iid);
    assert(handle != INVALID_HANDLE);

    /* Initialize transport to control system. */
    NkKosTransport_Init(&transport, handle, NK_NULL, 0);

    /**
     * Prepare the structures of the request to the lights gpio entity: constant
     * part and arena. Because none of the methods of the lights gpio entity has
     * sequence type arguments, only constant parts of the
     * request and response are used. Arenas are effectively unused. However,
     * valid arenas of the request and response must be passed to
     * lights gpio transport methods (nk_transport_recv, nk_transport_reply) and
     * to the lights gpio method.
     */
    traffic_light_LightsGPIO_entity_req req;
    char req_buffer[traffic_light_LightsGPIO_entity_req_arena_size];
    struct nk_arena req_arena = NK_ARENA_INITIALIZER(req_buffer,
                                        req_buffer + sizeof(req_buffer));

    /* Prepare response structures: constant part and arena. */
    traffic_light_LightsGPIO_entity_res res;
    char res_buffer[traffic_light_LightsGPIO_entity_res_arena_size];
    struct nk_arena res_arena = NK_ARENA_INITIALIZER(res_buffer,
                                        res_buffer + sizeof(res_buffer));

    /**
     * Initialize mode component dispatcher. 3 is the value of the step,
     * which is the number by which the input value is increased.
     */
    traffic_light_CMode_component component;
    traffic_light_CMode_component_init(&component, CreateIModeImpl(0x1000000));

    /* Initialize lights gpio entity dispatcher. */
    traffic_light_LightsGPIO_entity entity;
    traffic_light_LightsGPIO_entity_init(&entity, &component);

    /* Communication with 'Diagnostics' */
    Handle handleDiagnostics = ServiceLocatorConnect("diagnostics_connection");
    assert(handleDiagnostics != INVALID_HANDLE);

    NkKosTransport transportDiagnostics;
    NkKosTransport_Init(&transportDiagnostics, handleDiagnostics, NK_NULL, 0);

    nk_iid_t riid = ServiceLocatorGetRiid(handleDiagnostics, "traffic_light.Diagnostics.sendMessage");
    assert(riid != INVALID_RIID);

    struct DiagnosticsMessage_proxy proxy;
    DiagnosticsMessage_proxy_init(&proxy, &transportDiagnostics.base, riid);

    DiagnosticsMessage_SendDiagnosticsMessage_req reqDiagnostics;
    DiagnosticsMessage_SendDiagnosticsMessage_res resDiagnostics;
    char reqBufferDiagnostics[DiagnosticsMessage_SendDiagnosticsMessage_req_arena_size];
    struct nk_arena reqArenaDiagnostics = NK_ARENA_INITIALIZER(reqBufferDiagnostics, reqBufferDiagnostics + sizeof(reqBufferDiagnostics));

    fprintf(stderr, "Hello I'm LightsGPIO\n");

    uint32_t idx = 0;
    /* Dispatch loop implementation. */
    do
    {
        /* Flush request/response buffers. */
        nk_req_reset(&req);
        nk_arena_reset(&req_arena);
        nk_arena_reset(&res_arena);

        /* Wait for request to lights gpio entity. */
        if (nk_transport_recv(&transport.base,
                              &req.base_,
                              &req_arena) != NK_EOK) {
            fprintf(stderr, "nk_transport_recv error\n");
        } else {
            /**
             * Handle received request by calling implementation Mode_impl
             * of the requested Mode interface method.
             */
            traffic_light_LightsGPIO_entity_dispatch(&entity, &req.base_, &req_arena,
                                        &res.base_, &res_arena);

            idx++;
            char fst[128] = {0};
            char snd[128] = {0};
            int result = res.lightsGpio_mode.FMode.res_.result;
            lightCodeToString(result, fst);
            lightCodeToString(result >> 8, snd);

            SendMessage(idx, fst, snd, &proxy, &reqDiagnostics, &resDiagnostics, &reqArenaDiagnostics, RTL_NULL);
        }

        /* Send response. */
        if (nk_transport_reply(&transport.base,
                               &res.base_,
                               &res_arena) != NK_EOK) {
            fprintf(stderr, "nk_transport_reply error\n");
        }
    }
    while (true);

    return EXIT_SUCCESS;
}
