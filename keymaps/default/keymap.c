#include <stdint.h>
#include <sys/types.h>
#include QMK_KEYBOARD_H
#include "keymap_japanese.h"

#define MHEN_FN   LT(1, JP_MHEN)
#define SPC_NUM   LT(2, KC_SPC)
#define SPC_SMBL  LT(3, KC_SPC)
#define HENK_FN   LT(4, JP_HENK)
#define MINS_CTL  RCTL_T(JP_MINS)
#define SLSH_SFT  RSFT_T(JP_SLSH)
#define X_SPR     LGUI_T(JP_X)
#define DOT_SPR   RGUI_T(JP_DOT)
#define C_META    LALT_T(JP_C)
#define COMM_META RALT_T(JP_COMM)

enum custom_keycodes {
    A_CTL = SAFE_RANGE,
    Z_SFT,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        JP_Q,            JP_W,            JP_E,            JP_R,            JP_T,                             JP_Y,            JP_U,            JP_I,            JP_O,            JP_P,
        A_CTL,           JP_S,            JP_D,            JP_F,            JP_G,                             JP_H,            JP_J,            JP_K,            JP_L,            MINS_CTL,
        Z_SFT,           X_SPR,           C_META,          JP_V,            JP_B,                             JP_N,            JP_M,            COMM_META,       DOT_SPR,         SLSH_SFT,
                                                           MHEN_FN,         SPC_NUM,         KC_MUTE,         SPC_SMBL,        HENK_FN
    ),
    [1] = LAYOUT(
        KC_ESC,          KC_NO,           KC_END,          KC_NO,           KC_NO,                            KC_NO,           KC_NO,           KC_NO,           KC_BSPC,         KC_NO,
        KC_HOME,         MS_BTN2,         MS_BTN3,         MS_BTN1,         KC_NO,                            KC_LEFT,         KC_DOWN,         KC_UP,           KC_RGHT,         KC_NO,
        KC_LSFT,         MS_BTN4,         MS_BTN5,         KC_NO,           KC_NO,                            KC_DEL,          KC_NO,           KC_NO,           KC_NO,           KC_NO,
                                                           KC_NO,           KC_NO,           KC_TRNS,         KC_TAB,          KC_ENT
    ),
    [2] = LAYOUT(
        KC_NO,           KC_NO,           KC_NO,           KC_NO,           KC_NO,                            KC_NO,           KC_NO,           KC_NO,           KC_NO,           KC_NO,
        JP_1,            JP_2,            JP_3,            JP_4,            JP_5,                             JP_6,            JP_7,            JP_8,            JP_9,            JP_0,
        KC_TRNS,         KC_NO,           KC_NO,           KC_NO,           KC_NO,                            KC_NO,           KC_NO,           KC_NO,           KC_NO,           KC_NO,
                                                           KC_NO,           KC_NO,           KC_TRNS,         KC_TRNS,         KC_NO
    ),
    [3] = LAYOUT(
        JP_GRV,          JP_ASTR,         JP_AMPR,         KC_NO,           JP_TILD,                          KC_NO,           JP_UNDS,         JP_LCBR,         JP_RCBR,         KC_NO,
        JP_AT,           JP_QUOT,         JP_DQUO,         JP_COLN,         KC_NO,                            JP_HASH,         JP_SCLN,         JP_LPRN,         JP_RPRN,         JP_PLUS,
        KC_TRNS,         JP_EXLM,         JP_CIRC,         JP_PIPE,         JP_DLR,                           KC_NO,           JP_PERC,         JP_LBRC,         JP_RBRC,         JP_BSLS,
                                                           KC_TRNS,         KC_TRNS,         KC_TRNS,         KC_NO,           KC_NO
    ),
    [4] = LAYOUT(
        KC_NO,           KC_NO,           KC_F11,          KC_NO,           KC_NO,                            KC_NO,           KC_NO,           KC_F12,          KC_NO,           KC_NO,
        KC_F1,           KC_F2,           KC_F3,           KC_F4,           KC_F5,                            KC_F6,           KC_F7,           KC_F8,           KC_F9,           KC_F10,
        KC_NO,           KC_NO,           KC_NO,           KC_NO,           KC_NO,                            KC_NO,           KC_NO,           KC_NO,           KC_NO,           KC_NO,
                                                           KC_NO,           KC_NO,           KC_TRNS,         KC_NO,           KC_NO
    )
};

typedef enum {
    A_IDLE,
    A_TAP,
    A_PENDING_HOLD,
    A_HOLD,
    A_HOLD_READY,
    A_DONE,
    A_QUICK_TAP,
} a_state_t;

static a_state_t a_state           = A_IDLE;
static uint16_t  a_timer           = 0;
static uint16_t  a_retap_timer     = 0;
static uint16_t  a_pending_keycode = KC_NO;
static uint16_t  a_pending_timer   = 0;
static uint16_t  a_quick_tap_timer = 0;
static bool      a_quick_tap_armed = false;

bool process_record_a_ctl(uint16_t keycode, keyrecord_t *record) {
    if (keycode != A_CTL) {
        if (record->event.pressed && a_quick_tap_armed) {
            a_quick_tap_armed = false;
        }
        if (record->event.pressed && a_state == A_TAP) {
            a_pending_keycode = keycode;
            a_pending_timer   = timer_read();
            a_state = A_PENDING_HOLD;
            return false;
        }
        if (record->event.pressed && a_state == A_PENDING_HOLD) {
            tap_code(JP_A);
            register_code16(a_pending_keycode);
            unregister_code16(a_pending_keycode);
            register_code16(keycode);
            unregister_code16(keycode);
            a_pending_keycode = KC_NO;
            a_state = A_IDLE;
            return false;
        }
        if (!record->event.pressed && a_state == A_PENDING_HOLD && keycode == a_pending_keycode) {
            register_code(KC_LCTL);
            register_code16(keycode);
            unregister_code16(keycode);
            a_pending_keycode = KC_NO;
            a_state = A_HOLD;
            a_timer = timer_read();
            return false;
        }
        return true;
    }
    if (record->event.pressed) {
        if (a_quick_tap_armed && a_state == A_IDLE && timer_elapsed(a_quick_tap_timer) < 200) {
            a_quick_tap_armed = false;
            a_state = A_QUICK_TAP;
            register_code(JP_A);
        }
        else if (a_state == A_IDLE) {
            a_state = A_TAP;
            a_timer = timer_read();
        }
        else if (a_state == A_HOLD_READY && a_retap_timer && timer_elapsed(a_retap_timer) < 100) {
            tap_code16(LCTL(JP_A));
            a_state = A_DONE;
            a_retap_timer = 0;
        }
        else {
            a_state = A_IDLE;
            a_retap_timer = 0;
        }
    }
    else {
        if (a_state == A_TAP) {
            tap_code(JP_A);
            a_quick_tap_armed = true;
            a_quick_tap_timer = timer_read();
            a_state = A_IDLE;
        }
        else if (a_state == A_PENDING_HOLD) {
            if (timer_elapsed(a_pending_timer) < 75) {
                tap_code(JP_A);
                if (a_pending_keycode != KC_NO) {
                    register_code16(a_pending_keycode);
                    unregister_code16(a_pending_keycode);
                    a_pending_keycode = KC_NO;
                }
                a_state = A_IDLE;
            }
            else {
                register_code(KC_LCTL);
                if (a_pending_keycode != KC_NO) {
                    register_code16(a_pending_keycode);
                    unregister_code16(a_pending_keycode);
                    a_pending_keycode = KC_NO;
                }
                unregister_code(KC_LCTL);
                a_state = A_IDLE;
            }
        }
        else if (a_state == A_HOLD) {
            unregister_code(KC_LCTL);
            a_state = A_IDLE;
        }
        else if (a_state == A_HOLD_READY) {
            unregister_code(KC_LCTL);
            a_retap_timer = timer_read();
        }
        else if (a_state == A_DONE) {
            a_state = A_IDLE;
        }
        else if (a_state == A_QUICK_TAP) {
            unregister_code(JP_A);
            a_state = A_IDLE;
        }
    }
    return false;
}

typedef enum {
    Z_IDLE,
    Z_TAP,
    Z_PENDING_HOLD,
    Z_HOLD,
    Z_HOLD_READY,
    Z_DONE,
    Z_QUICK_TAP,
} z_state_t;

static z_state_t z_state           = Z_IDLE;
static uint16_t  z_timer           = 0;
static uint16_t  z_retap_timer     = 0;
static uint16_t  z_pending_keycode = KC_NO;
static uint16_t  z_pending_timer   = 0;
static uint16_t  z_quick_tap_timer = 0;
static bool      z_quick_tap_armed = false;

bool process_record_z_sft(uint16_t keycode, keyrecord_t *record) {
    if (keycode != Z_SFT) {
        if (record->event.pressed && z_quick_tap_armed) {
            z_quick_tap_armed = false;
        }
        if (record->event.pressed && z_state == Z_TAP) {
            z_pending_keycode = keycode;
            z_pending_timer   = timer_read();
            z_state = Z_PENDING_HOLD;
            return false;
        }
        if (record->event.pressed && z_state == Z_PENDING_HOLD) {
            register_code(KC_LSFT);
            register_code16(z_pending_keycode);
            unregister_code16(z_pending_keycode);
            register_code16(keycode);
            unregister_code16(keycode);
            z_pending_keycode = KC_NO;
            z_state = Z_HOLD;
            z_timer = timer_read();
            return false;
        }
        if (!record->event.pressed && z_state == Z_PENDING_HOLD && keycode == z_pending_keycode) {
            register_code(KC_LSFT);
            register_code16(keycode);
            unregister_code16(keycode);
            z_pending_keycode = KC_NO;
            z_state = Z_HOLD;
            z_timer = timer_read();
            return false;
        }
        return true;
    }
    if (record->event.pressed) {
        if (z_quick_tap_armed && z_state == Z_IDLE && timer_elapsed(z_quick_tap_timer) < 200) {
            z_quick_tap_armed = false;
            z_state = Z_QUICK_TAP;
            if (layer_state_is(0)) {
                register_code(JP_Z);
            }
        }
        else if (z_state == Z_IDLE) {
            z_state = Z_TAP;
            z_timer = timer_read();
        }
        else if (z_state == Z_HOLD_READY && z_retap_timer && timer_elapsed(z_retap_timer) < 100) {
            if (layer_state_is(0)) { tap_code16(LSFT(JP_Z)); }
            z_state = Z_DONE;
            z_retap_timer = 0;
        }
        else {
            z_state = Z_IDLE;
            z_retap_timer = 0;
        }
    }
    else {
        if (z_state == Z_TAP && layer_state_is(0)) {
            tap_code(JP_Z);
            z_quick_tap_armed = true;
            z_quick_tap_timer = timer_read();
            z_state = Z_IDLE;
        }
        else if (z_state == Z_PENDING_HOLD) {
            if (timer_elapsed(z_pending_timer) < 75) {
                if (layer_state_is(0)) {
                    tap_code(JP_Z);
                }
                if (z_pending_keycode != KC_NO) {
                    register_code16(z_pending_keycode);
                    unregister_code16(z_pending_keycode);
                    z_pending_keycode = KC_NO;
                }
                z_state = Z_IDLE;
            } else {
                register_code(KC_LSFT);
                if (z_pending_keycode != KC_NO) {
                    register_code16(z_pending_keycode);
                    unregister_code16(z_pending_keycode);
                    z_pending_keycode = KC_NO;
                }
                unregister_code(KC_LSFT);
                z_state = Z_IDLE;
            }
        }
        else if (z_state == Z_HOLD) {
            unregister_code(KC_LSFT);
            z_state = Z_IDLE;
        }
        else if (z_state == Z_HOLD_READY) {
            unregister_code(KC_LSFT);
            z_retap_timer = timer_read();
        }
        else if (z_state == Z_DONE) {
            z_state = Z_IDLE;
        }
        else if (z_state == Z_QUICK_TAP) {
            if (layer_state_is(0)) {
                unregister_code(JP_Z);
            }
            z_state = Z_IDLE;
        }
    }
    return false;
}

bool encoder_pressed = false;
bool encoder_used    = false;

bool process_record_enc(uint16_t keycode, keyrecord_t *record) {
    if (keycode == KC_MUTE) {
        if (record->event.pressed) {
            encoder_pressed = true;
            encoder_used = false;
            return false;
        }
        else {
            encoder_pressed = false;
            if (!encoder_used) {
                tap_code16(KC_MUTE);
            }
            return false;
        }
    }
    return true;
}

void matrix_scan_user(void) {
    if (a_state == A_TAP && timer_elapsed(a_timer) >= 200)                               { a_state = A_HOLD; register_code(KC_LCTL); }
    if (a_state == A_HOLD && timer_elapsed(a_timer) >= 500)                              { a_state = A_HOLD_READY; }
    if (a_state == A_HOLD_READY && a_retap_timer && timer_elapsed(a_retap_timer) >= 100) { a_state = A_IDLE; a_retap_timer = 0; }

    if (z_state == Z_TAP && timer_elapsed(z_timer) >= 200)                               { z_state = Z_HOLD; register_code(KC_LSFT); }
    if (z_state == Z_HOLD && timer_elapsed(z_timer) >= 500)                              { z_state = Z_HOLD_READY; }
    if (z_state == Z_HOLD_READY && z_retap_timer && timer_elapsed(z_retap_timer) >= 100) { z_state = Z_IDLE; z_retap_timer = 0; }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_a_ctl(keycode, record)) return false;
    if (!process_record_z_sft(keycode, record)) return false;
    if (!process_record_enc(keycode, record)) return false;
    if ((layer_state_is(2) || layer_state_is(3)) && record->event.pressed) {
        del_mods(MOD_MASK_SHIFT);
    }
    return true;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (layer_state_is(1)) {
        tap_code16(clockwise ? MS_WHLU : MS_WHLD);
    }
    else if (encoder_pressed) {
        encoder_used = true;
        tap_code16(clockwise ? KC_BRIU : KC_BRID);
    }
    else {
        tap_code16(clockwise ? KC_VOLU : KC_VOLD);
    }
    return false;
}

void suspend_wakeup_init_user(void) {
    clear_keyboard();
}
