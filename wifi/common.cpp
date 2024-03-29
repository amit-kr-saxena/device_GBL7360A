
#include <stdlib.h>
#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/handlers.h>

#include "wifi_hal.h"
#include "common.h"

interface_info *getIfaceInfo(wifi_interface_handle handle)
{
    return (interface_info *)handle;
}

wifi_handle getWifiHandle(wifi_interface_handle handle)
{
    return getIfaceInfo(handle)->handle;
}

hal_info *getHalInfo(wifi_handle handle)
{
    return (hal_info *)handle;
}

hal_info *getHalInfo(wifi_interface_handle handle)
{
    return getHalInfo(getWifiHandle(handle));
}

wifi_handle getWifiHandle(hal_info *info)
{
    return (wifi_handle)info;
}

wifi_interface_handle getIfaceHandle(interface_info *info)
{
    return (wifi_interface_handle)info;
}

wifi_error wifi_register_handler(wifi_handle handle, int cmd, nl_recvmsg_msg_cb_t func, void *arg)
{
    hal_info *info = (hal_info *)handle;

    /* TODO: check for multiple handlers? */
    pthread_mutex_lock(&info->cb_lock);

    wifi_error result = WIFI_ERROR_OUT_OF_MEMORY;

    if (info->num_event_cb < info->alloc_event_cb) {
        info->event_cb[info->num_event_cb].nl_cmd  = cmd;
        info->event_cb[info->num_event_cb].vendor_id  = 0;
        info->event_cb[info->num_event_cb].vendor_subcmd  = 0;
        info->event_cb[info->num_event_cb].cb_func = func;
        info->event_cb[info->num_event_cb].cb_arg  = arg;
        ALOGD("Successfully added event handler %p:%p for command %d at %d",
                arg, func, cmd, info->num_event_cb);
        info->num_event_cb++;
        result = WIFI_SUCCESS;
    }

    pthread_mutex_unlock(&info->cb_lock);
    return result;
}

wifi_error wifi_register_vendor_handler(wifi_handle handle,
        uint32_t id, int subcmd, nl_recvmsg_msg_cb_t func, void *arg)
{
    hal_info *info = (hal_info *)handle;

    /* TODO: check for multiple handlers? */
    pthread_mutex_lock(&info->cb_lock);

    wifi_error result = WIFI_ERROR_OUT_OF_MEMORY;

    if (info->num_event_cb < info->alloc_event_cb) {
        info->event_cb[info->num_event_cb].nl_cmd  = NL80211_CMD_VENDOR;
        info->event_cb[info->num_event_cb].vendor_id  = id;
        info->event_cb[info->num_event_cb].vendor_subcmd  = subcmd;
        info->event_cb[info->num_event_cb].cb_func = func;
        info->event_cb[info->num_event_cb].cb_arg  = arg;
        ALOGD("Added event handler %p:%p for vendor 0x%0x and subcmd 0x%0x at %d",
                arg, func, id, subcmd, info->num_event_cb);
        info->num_event_cb++;
        result = WIFI_SUCCESS;
    }

    pthread_mutex_unlock(&info->cb_lock);
    return result;
}

void wifi_unregister_handler(wifi_handle handle, int cmd)
{
    hal_info *info = (hal_info *)handle;

    if (cmd == NL80211_CMD_VENDOR) {
        ALOGE("Must use wifi_unregister_vendor_handler to remove vendor handlers");
        return;
    }

    pthread_mutex_lock(&info->cb_lock);

    for (int i = 0; i < info->num_event_cb; i++) {
        if (info->event_cb[i].nl_cmd == cmd) {
            ALOGD("Successfully removed event handler %p:%p for cmd = 0x%0x from %d",
                    info->event_cb[i].cb_arg, info->event_cb[i].cb_func, cmd, i);

            memmove(&info->event_cb[i], &info->event_cb[i+1],
                (info->num_event_cb - i - 1) * sizeof(cb_info));
            info->num_event_cb--;
            break;
        }
    }

    pthread_mutex_unlock(&info->cb_lock);
}

void wifi_unregister_vendor_handler(wifi_handle handle, uint32_t id, int subcmd)
{
    hal_info *info = (hal_info *)handle;

    pthread_mutex_lock(&info->cb_lock);

    for (int i = 0; i < info->num_event_cb; i++) {

        if (info->event_cb[i].nl_cmd == NL80211_CMD_VENDOR
                && info->event_cb[i].vendor_id == id
                && info->event_cb[i].vendor_subcmd == subcmd) {
            ALOGD("Successfully removed event handler %p:%p for vendor 0x%0x, subcmd 0x%0x from %d",
                    info->event_cb[i].cb_arg, info->event_cb[i].cb_func, id, subcmd, i);
            memmove(&info->event_cb[i], &info->event_cb[i+1],
                (info->num_event_cb - i - 1) * sizeof(cb_info));
            info->num_event_cb--;
            break;
        }
    }

    pthread_mutex_unlock(&info->cb_lock);
}


wifi_error wifi_register_cmd(wifi_handle handle, int id, WifiCommand *cmd)
{
    hal_info *info = (hal_info *)handle;

    ALOGD("registering command %d", id);

    wifi_error result = WIFI_ERROR_OUT_OF_MEMORY;

    if (info->num_cmd < info->alloc_cmd) {
        info->cmd[info->num_cmd].id   = id;
        info->cmd[info->num_cmd].cmd  = cmd;
        ALOGD("Successfully added command %d: %p at %d", id, cmd, info->num_cmd);
        info->num_cmd++;
        result = WIFI_SUCCESS;
    }

    return result;
}

WifiCommand *wifi_unregister_cmd(wifi_handle handle, int id)
{
    hal_info *info = (hal_info *)handle;

    ALOGD("un-registering command %d", id);

    WifiCommand *cmd = NULL;

    for (int i = 0; i < info->num_cmd; i++) {
        if (info->cmd[i].id == id) {
            cmd = info->cmd[i].cmd;
            memmove(&info->cmd[i], &info->cmd[i+1], (info->num_cmd - i) * sizeof(cmd_info));
            info->num_cmd--;
            ALOGD("Successfully removed command %d: %p from %d", id, cmd, i);
            break;
        }
    }

    return cmd;
}

WifiCommand *wifi_get_cmd(wifi_handle handle, int id)
{
    hal_info *info = (hal_info *)handle;

    WifiCommand *cmd = NULL;

    for (int i = 0; i < info->num_cmd; i++) {
        if (info->cmd[i].id == id) {
            cmd = info->cmd[i].cmd;
            break;
        }
    }

    return cmd;
}

void wifi_unregister_cmd(wifi_handle handle, WifiCommand *cmd)
{
    hal_info *info = (hal_info *)handle;

    for (int i = 0; i < info->num_cmd; i++) {
        if (info->cmd[i].cmd == cmd) {
            int id = info->cmd[i].id;
            memmove(&info->cmd[i], &info->cmd[i+1], (info->num_cmd - i) * sizeof(cmd_info));
            info->num_cmd--;
            ALOGD("Successfully removed command %d: %p from %d", id, cmd, i);
            break;
        }
    }
}
