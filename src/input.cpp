#include "input.h"

void Input::GlfwCallbacks::KeyCallback(Input* input, int key, int scancode,
                                       int action, int mods)
{
    input->KeyCallback(key, scancode, action, mods);
}

void Input::GlfwCallbacks::MouseCallback(Input* input, double xpos, double ypos)
{
    input->MouseCallback(xpos, ypos);
}


void Input::GlfwCallbacks::MouseEnterCallback(Input* input, bool entered)
{
    input->MouseEnterCallback(entered);
}

Input::Input(GLFWwindow* window)
    : frozen_state_map_(&action_state_map_1_),
      recording_state_map_(&action_state_map_2_),
      frozen_mouse_movement_(&mouse_movement_1_),
      recording_mouse_movement_(&mouse_movement_2_),
      mouse_sensitivity_(25.0),
      mouse_in_window_(glfwGetWindowAttrib(window, GLFW_HOVERED))
{
    // TODO: load/edit input configuration
    // Load default keybinds
    AddKeyBindFromGlfwKey(InputAction::MoveForward, GLFW_KEY_W);
    AddKeyBindFromGlfwKey(InputAction::MoveBackward, GLFW_KEY_S);
    AddKeyBindFromGlfwKey(InputAction::MoveLeft, GLFW_KEY_A);
    AddKeyBindFromGlfwKey(InputAction::MoveRight, GLFW_KEY_D);
    AddKeyBindFromGlfwKey(InputAction::MoveUp, GLFW_KEY_R);
    AddKeyBindFromGlfwKey(InputAction::MoveDown, GLFW_KEY_F);
    AddKeyBindFromGlfwKey(InputAction::RollRight, GLFW_KEY_E);
    AddKeyBindFromGlfwKey(InputAction::RollLeft, GLFW_KEY_Q);
    AddKeyBindFromGlfwKey(InputAction::ToggleImgui, GLFW_KEY_GRAVE_ACCENT);
    AddKeyBindFromGlfwKey(InputAction::Slow, GLFW_KEY_LEFT_SHIFT);
    AddKeyBindFromGlfwKey(InputAction::Quit, GLFW_KEY_ESCAPE);
}

void Input::Poll()
{
    glfwPollEvents();
    // Clear the mouse input, but keep the current key states
    // This is so that mouse input doesn't carry over, but the
    // key states do
    *frozen_state_map_ = *recording_state_map_;
    *frozen_mouse_movement_ = glm::vec2();
    std::swap(frozen_state_map_, recording_state_map_);
    std::swap(frozen_mouse_movement_, recording_mouse_movement_);
}

bool Input::GetActionInputState(InputAction key)
{
    auto iter = frozen_state_map_->find(key);
    if (iter != frozen_state_map_->end()) {
        return iter->second;
    }
    return false;
}

glm::vec2 Input::GetMouseMovement() { return *frozen_mouse_movement_; }

float Input::GetMouseSensitivity() { return mouse_sensitivity_; }
void Input::SetMouseSensitivity(float sensitivity)
{
    mouse_sensitivity_ = sensitivity;
}

void Input::AddKeyBindFromGlfwKey(InputAction action, int key)
{
    AddKeyBind(action, glfwGetKeyScancode(key));
}

void Input::AddKeyBind(InputAction action, int scancode)
{
    action_to_scancode_map_.emplace(action, scancode);
    scancode_to_action_map_.emplace(scancode, action);
}

void Input::KeyCallback(int key, int scancode, int action, int mods)
{
    // Ignore repeats
    if (action == GLFW_REPEAT) return;

    // lookup scancode
    auto iter = scancode_to_action_map_.find(scancode);
    if (iter == scancode_to_action_map_.end()) {
        return;
    }

    auto input_action = iter->second;
    // set pressed or not pressed
    (*recording_state_map_)[input_action] = action == GLFW_PRESS;
}

void Input::MouseCallback(double xpos, double ypos)
{
    if (first_mouse_) {
        first_mouse_ = false;
        previous_mouse_position_ = {xpos, ypos};
    }
    // Measured from top-left, so negate y
    recording_mouse_movement_->x +=
        mouse_sensitivity_ * ((float)xpos - previous_mouse_position_.x);
    recording_mouse_movement_->y +=
        -mouse_sensitivity_ * ((float)ypos - previous_mouse_position_.y);
    previous_mouse_position_ = {xpos, ypos};
}

void Input::MouseEnterCallback(bool entered)
{
    mouse_in_window_ = entered;
    if (mouse_in_window_) {
        first_mouse_ = true;
    }
}