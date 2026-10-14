/* libFuzzer harness for the INI loader, bridge-map and allow-remotes parsers. */
#include "softgred_config.h"

#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[] = "/tmp/softgre-fuzz-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return 0;
    ssize_t written = write(fd, data, size);
    close(fd);
    if (written == (ssize_t)size) {
        bool accepted = softgred_config_load_conf(path);
        /* Binary corruption must never hide an authorization setting. */
        if (accepted && memchr(data, '\0', size))
            abort();
    }
    unlink(path);
    softgred_config_release();
    return 0;
}
