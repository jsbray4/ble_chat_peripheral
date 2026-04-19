/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"
#include "app_cli.h"
#include "sl_cli_handles.h"
#include "colors.h"


// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
volatile conn_state_t conn_state = init;
volatile uint8_t connection_handle = CONNECTION_HANDLE_INVALID;
volatile uint32_t service_handle = SERVICE_HANDLE_INVALID;
volatile uint16_t characteristic_handle = CHARACTERISTIC_HANDLE_INVALID;

// Application Init.
void app_init(void)
{
  //cli initialization
  app_cli_init(&sl_cli_inst_handle);
}

//running state logic
void app_process_action(void)
{
  if (conn_state == running) {
    if (connection_handle != CONNECTION_HANDLE_INVALID) { //ensure connection valid

      //get connection rssi
      int8_t rssi;
      char color_buf[20];
      sl_status_t sc = sl_bt_connection_get_median_rssi(connection_handle, &rssi);
      if (sc != SL_STATUS_OK) {
        return;
      }

      //print rssi to terminal
      //color based on connection strength
      if(rssi >= -65) {
        snprintf(color_buf, sizeof(color_buf), COLOR_GREEN);
      }
      else if(rssi >= -75) {
        snprintf(color_buf, sizeof(color_buf), COLOR_YELLOW);
      }
      else {
        snprintf(color_buf, sizeof(color_buf), COLOR_RED);
      }

      //appends it to the end of previous line
      printf("%s\x1B[1A\t\t\t\t\t\t\t\t[RSSI] %d dBm%s\r\n", color_buf, rssi, COLOR_EXIT);
    }
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the default weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      //configure security flags
      uint8_t flags = SL_BT_SM_CONFIGURATION_MITM_REQUIRED | SL_BT_SM_CONFIGURATION_SC_ONLY | SL_BT_SM_CONFIGURATION_BONDING_REQUEST_REQUIRED | SL_BT_SM_CONFIGURATION_PREFER_MITM;
      sc = sl_bt_sm_configure(flags, sl_bt_sm_io_capability_keyboarddisplay); //fixed on peripheral, configurable on central
      app_assert_status(sc);
      sc = sl_bt_sm_set_passkey (-1); //generate randomly
      app_assert_status(sc);

      //configure bonding
      sc = sl_bt_sm_store_bonding_configuration(4, 2); //4 max bonds, replacing oldest if full
      app_assert_status(sc);
      sc = sl_bt_sm_set_bondable_mode(1); //on by default
      app_assert_status(sc);

      //create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      //generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      //append name to advertising data
      uint8_t adv_data[] = {
      16, 0x09, 'C', 'h', 'a', 't', ' ', 'P', 'e', 'r', 'i', 'p', 'h', 'e', 'r', 'a', 'l'
      };
      sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                          sl_bt_advertiser_scan_response_packet,
                                           sizeof(adv_data),
                                           adv_data);
      app_assert_status(sc);

      //set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

      //start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      print_menu();
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      //store connection handle
      printf("Connection opened\r\n");
      connection_handle = evt->data.evt_connection_opened.connection;
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      //reset connection handle + app state
      printf("Connection terminated\r\n");
      connection_handle = CONNECTION_HANDLE_INVALID;
      conn_state = init;

      //display menu
      print_menu();

      //generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      //observed that there is no need to re-append name to advertising data

      //restart advertising
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;
    
    // This event indicated connection parameters have changed
    case sl_bt_evt_connection_parameters_id:
      //print connection parameters
      printf("Connection parameters updated:\r\n");
      printf("\tConnection Handle: %u\r\n", evt->data.evt_connection_parameters.connection);
      printf("\tInterval: %u * 1.25ms\r\n", evt->data.evt_connection_parameters.interval);
      printf("\tLatency: %u\r\n", evt->data.evt_connection_parameters.latency);
      printf("\tTimeout: %u * 10ms\r\n", evt->data.evt_connection_parameters.timeout);
      printf("\tSecurity Mode: %u\r\n", evt->data.evt_connection_parameters.security_mode);
      
      //triggers in all scenarios; used as confirm bonding state if bond doesn't exist already
      if(evt->data.evt_connection_parameters.security_mode == 0)
        conn_state = opening;
      //connection authenticated
      else if(evt->data.evt_connection_parameters.security_mode == 3)//
      {
        //triggerd if bond already existed
        if(conn_state == opening)
          printf("Previous bond used to reconnect\r\n");
        
        conn_state = running;
      }
      break;
    
    //passkey pairing, triggered if central set to keyboard only
    case sl_bt_evt_sm_passkey_display_id:
      printf("Passkey confirmation requested. Please enter the passkey %06u on the central device using '/verify {xxxxxx}'.\r\n", evt->data.evt_sm_passkey_display.passkey);
      conn_state = passkey;
      break;
    
    //numeric comparison pairing, triggered if central set to keyboard/display
    case sl_bt_evt_sm_confirm_passkey_id:
      printf("Numeric comparison confirmation requested. Please confirm the passkey %06u matches using '/verify {0,1}'.\r\n", evt->data.evt_sm_confirm_passkey.passkey);
      conn_state = numeric;
      break;
    
    //triggered when central writes to attribute
    case sl_bt_evt_gatt_server_attribute_value_id:
      //echo attribute value to terminal
      fwrite(evt->data.evt_gatt_server_attribute_value.value.data, sizeof(uint8_t), evt->data.evt_gatt_server_attribute_value.value.len, stdout);
      break;
    
    //triggered if no bond exists, even if bonding disabled on one or both boards
    case sl_bt_evt_sm_confirm_bonding_id:
      printf("Bonding confirmation requested. Please use '/verify {0,1}' to respond.\r\n");
      break;
    
    //triggered when bonding/pairing complete
    case sl_bt_evt_sm_bonded_id:
        printf("Pairing process completed\r\n");
        break;

    //triggered when bonding/pairing process fails
    case sl_bt_evt_sm_bonding_failed_id:
      //close the connection
      printf("Bonding/pairing process failed\r\n");
      if(connection_handle != CONNECTION_HANDLE_INVALID) {
          sl_bt_connection_close(connection_handle);
      }
      else{ //edge case, shouldn't occur
          printf("Cannot disconnect: not connected\r\n");
      }
      break;
    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

void print_menu(void)
{
    printf("\r\n=== BLE Chat App Commands ===\r\n");

    //commands
    printf("/erase      - Erase all bonds\r\n");
    printf("/bondable   - Enable or disable bondable mode (0 or 1)\r\n");
    printf("/dc         - Disconnect from current device\r\n");
    printf("/chat       - Send a chat message to the connected device\r\n");
    printf("/menu       - Show this menu\r\n");
    printf("\r\n");
}
