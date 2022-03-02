#pragma once
#include "key.hpp"

class Input {
public:
    Input();
    Input(Input const&) = delete;
    Input& operator=(Input const&) = delete;

    void press_key(Key key);
    void release_key(Key key);
    void set_mouse_position(float x, float y);
    void update();

    bool is_key_pressed(Key key);
    bool was_key_pressed(Key key);
    void get_mouse_position(float* x, float* y);

    static const uint32_t MAX_KEY_COUNT = 512;
private:

    enum struct KeyState {
        IS_RELEASED,
        IS_PRESSED,
    };

    KeyState prev_key_states[MAX_KEY_COUNT];
    KeyState key_states[MAX_KEY_COUNT];
    float mouse_x;
    float mouse_y;
};
