
#include <stdlib.h>
#include <stdio.h>
#include "ipc.h"
#include "../../thirdparty/log/log.h"

static ipc_dev_t *ipc;

int ipc_init(ipc_param_t *param)
{
    int ret = 0;
    log_debug("ipc_init");

    if (!param)
    {
        log_error("check param error\n");
        goto err;
    }

    if (!ipc || !ipc->init)
    {
        log_error("check ipc dev fail\n");
        goto err;
    }

    ret = ipc->init(ipc, param);
    if (ret != 0)
    {
        log_error("ipc init error\n");
        goto err;
    }

    return 0;
err:
    return -1;
}

void ipc_run()
{
    if (!ipc || !ipc->run)
    {
        log_error("no ipc dev found!!!\n");
        return;
    }

    ipc->run(ipc);
}

int ipc_capture_picture(char *file)
{
    if (!ipc | !ipc->capture_picture)
    {
        log_error("param check error\n");
        return -1;
    }

    ipc->capture_picture(ipc, file);
    return 0;
}

int ipc_dev_register(ipc_dev_t *dev)
{
    if (!dev)
    {
        log_error("check param error\n");
        goto err;
    }

    log_debug("Register dev");

    ipc = dev;
err:
    return -1;
}
