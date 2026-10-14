/* libFuzzer harness for the GRE/VLAN/IPv4 packet parser, the daemon's most hostile input. */
#include "payload.h"
#include "softgred_config.h"

/* payload.c references these; parsing itself never calls them. */
void provision_packet(const struct payload_packet *packet)
{
    (void)packet;
}

struct softgred_config *softgred_config_ref(void)
{
    static struct softgred_config cfg;
    return &cfg;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct payload_packet out;
    if (payload_parse((const unsigned char *)data, size, &out)) {
        /* Read every output field so MemorySanitizer-style bugs surface under ASan/UBSan. */
        volatile unsigned sink = out.vlan;
        for (int i = 0; i < 6; i++)
            sink ^= out.mac[i];
        sink ^= out.remote.s_addr ^ out.local.s_addr;
        (void)sink;
    }
    return 0;
}
