#include "input.hpp"
#include <memory.h>

Input::Input() {
    for (uint32_t i = 0; i < MAX_KEY_COUNT; i++) {
        prev_key_states[i] = key_states[i] = KeyState::IS_RELEASED;
    }
}

void Input::press_key(Key key) {
    key_states[(int32_t)key] = KeyState::IS_PRESSED;
}

void Input::release_key(Key key) {
    key_states[(int32_t)key] = KeyState::IS_RELEASED;
}

void Input::update() {
    memcpy(prev_key_states, key_states, sizeof(key_states));
}


bool Input::is_key_pressed(Key key) {
    return key_states[(int32_t)key] == KeyState::IS_PRESSED;
}

bool Input::was_key_pressed(Key key) {
    return
        key_states[(int32_t)key] == KeyState::IS_RELEASED
        && prev_key_states[(int32_t)key] == KeyState::IS_PRESSED;
}


void Input::set_mouse_position(float x, float y) {
    mouse_x = x;
    mouse_y = y;
}


void Input::get_mouse_position(float* x, float* y) {
    *x = mouse_x;
    *y = mouse_y;
}
