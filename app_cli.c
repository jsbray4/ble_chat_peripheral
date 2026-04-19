// app_cli.c
#include "sl_cli.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gatt_db.h"


// -----------------------------------------------------------------------------
// Externs / globals from your application
// -----------------------------------------------------------------------------
extern volatile conn_state_t conn_state;
extern uint8_t connection_handle;
extern uint32_t service_handle;
extern uint16_t characteristic_handle;
#define SL_CLI_CHAT_BUFFER_SIZE 50 
#define NAME                          "Care: " //prepended to peripheral messages
//erase current line, may not produce desired effects in all terminals, used for macos native terminal
#define REMOVE_PREV_LINE            "\x1B[2K" 

// -----------------------------------------------------------------------------
// Command Handlers
// -----------------------------------------------------------------------------

//set bonding mode
static void cmd_bondable(sl_cli_command_arg_t *arguments)
{
    int value = sl_cli_get_argument_int8(arguments, 0);

    if (value != 0 && value != 1) {
        printf("Invalid argument value. Use 0 (disable) or 1 (enable)\r\n");
        return;
    }

    sl_status_t sc = sl_bt_sm_set_bondable_mode(value);

    if (sc != SL_STATUS_OK) {
        printf("Failed to set bondable mode (0x%04X)\r\n", sc);
        return;
    }
    if(value)
        printf("Bonding enabled successfully\r\n");
    else
        printf("Bonding disabled successfully\r\n");
}

//erase bonds
static void cmd_erase(sl_cli_command_arg_t *arguments)
{
    sl_status_t sc = sl_bt_sm_delete_bondings();
    if(sc != SL_STATUS_OK)
    {
        printf("Bonds failed to erase\r\n");
    }
    else
        printf("Bonds successfully erased\r\n");
}

//disconnect from central
static void cmd_dc(sl_cli_command_arg_t *arguments)
{
    if(connection_handle != CONNECTION_HANDLE_INVALID) {
      sl_status_t sc = sl_bt_connection_close(connection_handle);
      if(sc != SL_STATUS_OK)
        printf("Failed to disconnect\r\n");
    }
    else{
      printf("Cannot disconnect: not connected\r\n");
    }
}

//send a chat
static void cmd_chat(sl_cli_command_arg_t *arguments)
{   
    //clear current line
    printf(REMOVE_PREV_LINE);

    if (conn_state != running) {
        printf("Cannot send message: not connected\r\n");
        return;
    }

    char buf[SL_CLI_CHAT_BUFFER_SIZE];
    size_t pos = 0;

    //prepend NAME
    size_t name_len = strlen(NAME);
    if (name_len >= SL_CLI_CHAT_BUFFER_SIZE) return;
    memcpy(buf + pos, NAME, name_len);
    pos += name_len;

    //no argument provided
    int argc = sl_cli_get_argument_count(arguments);
    if (argc < 1) {
        printf("Incorrect number of arguments\r\n"); //removing this breaks code?
        return;
    }

    //append all CLI arguments
    for (int i = 0; i < argc; i++) {
        const char *arg = sl_cli_get_argument_string(arguments, i);
        if (!arg) continue;

        size_t arg_len = strlen(arg);
        if (pos + arg_len + 2 >= SL_CLI_CHAT_BUFFER_SIZE) {
            arg_len = SL_CLI_CHAT_BUFFER_SIZE - pos - 2; //truncate if needed
        }

        memcpy(buf + pos, arg, arg_len);
        pos += arg_len;

        //add space between arguments
        if (i < argc - 1 && pos + 1 < SL_CLI_CHAT_BUFFER_SIZE) {
            buf[pos++] = ' ';
        }
    }

    //append \r\n
    if (pos + 2 < SL_CLI_CHAT_BUFFER_SIZE) {
        buf[pos++] = '\r';
        buf[pos++] = '\n';
    }

    //notify central, this appears to also write to characteristic
    sl_status_t sc = sl_bt_gatt_server_send_notification(
        connection_handle,
        gattdb_message_stream,
        pos,          
        (uint8_t*)buf
    );

    //failed to notify; if testing, likely notifications not enabled
    if (sc != SL_STATUS_OK) {
        printf("Failed to send message (0x%02X)\r\n", sc);
        return;
    }

    //echo to peripheral terminal
    fwrite(buf, sizeof(char), pos, stdout);
}

//print menu
static void cmd_menu(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    print_menu();
}

//verify bonding/pairing
static void cmd_verify(sl_cli_command_arg_t *arguments)
{
    sl_status_t sc;

    if (conn_state != passkey && conn_state != numeric && conn_state != opening) {
        printf("Nothing to verify\r\n");
        return;
    }

    int value = sl_cli_get_argument_int8(arguments, 0);

    //passkey displayed on peripheral, confirmed on central
    if (conn_state == passkey) {
        printf("Enter passkey on central device, not peripheral\r\n");
        return;

    }

    if (value != 0 && value != 1) {
        printf("Invalid input. Use 1 to confirm, 0 to reject.\r\n");
        return;
    }

    if (conn_state == numeric) { //numeric comparison
        sc = sl_bt_sm_passkey_confirm(connection_handle, value);
        if (sc != SL_STATUS_OK) {
            printf("Failed to confirm passkey (0x%02X)\r\n", sc);
            return;
        }
        printf("Numeric comparison %s\r\n", value ? "confirmed" : "rejected");
    }
    else if (conn_state == opening) { //bonding confirmation
        sc = sl_bt_sm_bonding_confirm(connection_handle, value);
        if (sc != SL_STATUS_OK) {
            printf("Failed to confirm bonding (0x%02X)\r\n", sc);
            return;
        }

        if(value){
            printf("Bonding confirmed\r\n");
            conn_state = bonded;
        }
        else
            printf("Bonding rejected\r\n");
    }
}

// -----------------------------------------------------------------------------
// Command Info (metadata)
// -----------------------------------------------------------------------------
static const sl_cli_command_info_t cmd_bondable_info =
    SL_CLI_COMMAND(cmd_bondable,
                   "Allow/reject new bondings",
                   "0 = reject, 1 = allow",
                   { SL_CLI_ARG_INT8, SL_CLI_ARG_END });

static const sl_cli_command_info_t cmd_erase_info =
    SL_CLI_COMMAND(cmd_erase, "Delete bondings", "", { SL_CLI_ARG_END });

static const sl_cli_command_info_t cmd_dc_info =
    SL_CLI_COMMAND(cmd_dc, "Disconnect connection", "", { SL_CLI_ARG_END });

static const sl_cli_command_info_t cmd_chat_info =
    SL_CLI_COMMAND(cmd_chat, "Send a message", "", { SL_CLI_ARG_WILDCARD,SL_CLI_ARG_END });

static const sl_cli_command_info_t cmd_menu_info =
    SL_CLI_COMMAND(cmd_menu, "Show command menu", "", { SL_CLI_ARG_END });

static const sl_cli_command_info_t cmd_verify_info =
    SL_CLI_COMMAND(cmd_verify, "Verify numeric comparison", "numeric (0/1)", { SL_CLI_ARG_INT8, SL_CLI_ARG_END });


// -----------------------------------------------------------------------------
// Command Table
// -----------------------------------------------------------------------------
static const sl_cli_command_entry_t app_cli_table[] = {
    { "/erase", &cmd_erase_info, false },
    { "/bondable", &cmd_bondable_info, false },
    { "/dc", &cmd_dc_info, false },
    { "/chat", &cmd_chat_info, false },
    { "/menu", &cmd_menu_info, false },
    { "/verify", &cmd_verify_info, false },
    { NULL, NULL, false } // terminator
};

// -----------------------------------------------------------------------------
// Command Group
// -----------------------------------------------------------------------------
sl_cli_command_group_t app_cli_group = {
    { NULL }, // no parent
    false,    // visible
    app_cli_table
};

// -----------------------------------------------------------------------------
// CLI Initialization Helper
// -----------------------------------------------------------------------------
void app_cli_init(sl_cli_handle_t *cli_handle)
{   
    //cli initialization, call from app.c
    sl_cli_command_add_command_group(*cli_handle, &app_cli_group);
}