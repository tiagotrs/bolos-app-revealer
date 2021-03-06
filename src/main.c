/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"
#include "bolos_ux_common.h"
#include "revealer.h"
#include "error_codes.h"
#include "ux_nanos.h"
#include "bolos_ux.h"

#include "os_io_seproxyhal.h"

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static unsigned int current_text_pos; // parsing cursor in the text to display
static unsigned int text_y;           // current location of the displayed text

// UI currently displayed
extern enum UI_STATE { UI_IDLE, UI_TEXT, UI_APPROVAL };
extern enum UI_STATE uiState;

ux_state_t ux;

//static unsigned char display_text_part(void);

#define MAX_CHARS_PER_LINE 49


static char lineBuffer[50];

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

extern uint8_t hashDBG[32];
uint8_t row_nb;
static void sample_main(void) {

    // next timer callback in 500 ms
    //UX_CALLBACK_SET_INTERVAL(500);
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    uint8_t flags;
    revealer_struct_init();
    for (;;) {
    volatile unsigned short sw = 0;

    BEGIN_TRY {
      TRY {
        rx = tx;
        tx = 0; // ensure no race in catch_other if io_exchange throws an error
        rx = io_exchange(CHANNEL_APDU|flags, rx);
        flags = 0;

        // no apdu received, well, reset the session, and reset the bootloader configuration
        if (rx == 0) {
          THROW(0x6982);
        }

        if (G_io_apdu_buffer[0] != 0x80) {
          THROW(0x6E00);
        }

        switch (G_io_apdu_buffer[1]) {
            /*case 0xCA: // get hash
                for (int i=0; i<4; i++){
                    G_io_apdu_buffer[i] = hashDBG[i];
                }
                tx += 4;
                THROW(SW_OK);
                break;*/
            case 0xCB: // Send img row chunk
                #ifndef WORDS_IMG_DBG
                if ((G_bolos_ux_context.words_seed_valid)&&(G_bolos_ux_context.noise_seed_valid)){
                #else
                if (1){
                #endif
                    row_nb = G_io_apdu_buffer[3];
                    tx += send_row(row_nb);
                    THROW(SW_OK);
                }
                else {
                    THROW(REVEALER_UNSET);
                }
                break;
            default:
            THROW(0x6D00);
            break;
        }
      }
      CATCH_OTHER(e) {
        switch(e & 0xF000) {
          case 0x6000:
            sw = e;
            break;
          case SW_OK:
            sw = e;
            break;
          default:
            sw = 0x6800 | (e&0x7FF);
            break;
        }
        // Unexpected exception => report 
        G_io_apdu_buffer[tx] = sw>>8;
        G_io_apdu_buffer[tx+1] = sw;
        tx += 2;
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
        flags |= IO_ASYNCH_REPLY;
      }
      FINALLY {
      }
    }
    END_TRY;
  }

return_to_dashboard:
    return;
}


/*// Pick the text elements to display
static unsigned char display_text_part() {
    unsigned int i;
    WIDE char *text = (char*) G_io_apdu_buffer + 5;
    if (text[current_text_pos] == '\0') {
        return 0;
    }
    i = 0;
    while ((text[current_text_pos] != 0) && (text[current_text_pos] != '\n') &&
           (i < MAX_CHARS_PER_LINE)) {
        lineBuffer[i++] = text[current_text_pos];
        current_text_pos++;
    }
    if (text[current_text_pos] == '\n') {
        current_text_pos++;
    }
    lineBuffer[i] = '\0';

    return 1;
}*/


unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT: // for Nano S
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if ((uiState == UI_TEXT) &&
            (os_seph_features() & SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_SCREEN_BIG)) {
                UX_REDISPLAY();
            }
        else {
            if(G_bolos_ux_context.processing == 1)
            {
                UX_DISPLAYED_EVENT(check_and_write_words_Cb(););
            }
            else if (G_bolos_ux_context.processing == 2){
                UX_DISPLAYED_EVENT(initPrngAndWriteNoise_Cb(););
            }
            else
            {
                UX_DISPLAYED_EVENT();
            }
        }
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        #ifdef TARGET_NANOS
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
                // defaulty retrig very soon (will be overriden during
                // stepper_prepro)
                UX_CALLBACK_SET_INTERVAL(100);
                UX_REDISPLAY();
            });
        #endif 
        break;

    // unknown events are acknowledged
    default:
        UX_DEFAULT_EVENT();
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    current_text_pos = 0;
    text_y = 60;
    uiState = UI_IDLE;

    uint8_t font_h = 9;
    uint8_t font_w = 9;
    uint8_t i, j, x, y;
    x = 0; y = 0;

    // ensure exception will work as planned
    os_boot();

    UX_INIT();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            
#ifdef LISTEN_BLE
            if (os_seph_features() &
                SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_BLE) {
                BLE_power(0, NULL);
                // restart IOs
                BLE_power(1, NULL);
            }
#endif

            USB_power(0);
            //USB_power(1);
            revealer_struct_init();
            #ifdef WORDS_IMG_DBG
                USB_power(1);
                SPRINTF(G_bolos_ux_context.words, "slab tail mother hello host bean track mutual nest key inmate key");
                G_bolos_ux_context.words_length = strlen(G_bolos_ux_context.words);
                write_words();
            #endif
            ui_idle_init();
            sample_main();
        }
        CATCH_OTHER(e) {
            /*switch(e){
                case INVALID_NOISE_SEED:
                ui_idle_init();
                sample_main();
                break;
            }   */      
        }
        FINALLY {
        }
    }
    END_TRY;
}
