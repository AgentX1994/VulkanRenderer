#pragma once
#include "common_glm.h"
#include "common_vulkan.h"

enum class InputAction
{
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    RollRight,
    RollLeft,
    ToggleImgui,
    Slow,
    Quit
};

class Input
{
public:
    Input(GLFWwindow* window);

    void Poll();

    bool GetActionInputState(InputAction key);
    glm::vec2 GetMouseMovement();

    float GetMouseSensitivity();
    void SetMouseSensitivity(float sensitivity);

    // TODO: Find a better way to structure input so that this class
    // can take care of it all without having to have the application deal with
    // it
    struct GlfwCallbacks
    {
        static void KeyCallback(Input* input, int key, int scancode, int action,
                                int mods);
        static void MouseCallback(Input* input, double xpos, double ypos);
        static void MouseEnterCallback(Input* input, bool entered);
    };

private:
    using ActionStateMap = std::unordered_map<InputAction, bool>;
    friend GlfwCallbacks;

    void AddKeyBindFromGlfwKey(InputAction action, int key);
    void AddKeyBind(InputAction action, int scancode);

    void KeyCallback(int key, int scancode, int action, int mods);
    void MouseCallback(double xpos, double ypos);
    void MouseEnterCallback(bool entered);

    // TODO: Load key bindings
    // Holds the current action to scancode mappings
    std::unordered_map<InputAction, int> action_to_scancode_map_;
    std::unordered_map<int, InputAction> scancode_to_action_map_;

    // have two action state maps and mouse movements
    // and swap between them when poll is called
    ActionStateMap action_state_map_1_;
    ActionStateMap action_state_map_2_;

    ActionStateMap* frozen_state_map_;
    ActionStateMap* recording_state_map_;

    glm::vec2 mouse_movement_1_;
    glm::vec2 mouse_movement_2_;

    glm::vec2* frozen_mouse_movement_;
    glm::vec2* recording_mouse_movement_;

    bool first_mouse_ = true;
    glm::vec2 previous_mouse_position_;

    float mouse_sensitivity_;
    bool mouse_in_window_;
};