#pragma once

#include "inputstate.h"

#include <gsl/gsl>
#include <GLFW/glfw3.h>


namespace engine
{
    class InputHandler final
    {
    public:
        explicit InputHandler(const gsl::not_null<GLFWwindow*>& window)
            : m_window(window)
        {
            glfwGetCursorPos(m_window, &m_lastCursorX, &m_lastCursorY);
        }


        void update()
        {
            auto left = glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS;
            auto right = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;
            auto stepLeft = glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS;
            auto stepRight = glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS;
            auto forward = glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
            auto backward = glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;

            m_inputState.moveSlow = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                || glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            m_inputState.action = glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                || glfwGetKey(m_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS
                || glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            m_inputState.jump = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
            m_inputState.roll = glfwGetKey(m_window, GLFW_KEY_X) == GLFW_PRESS;

            double x, y;
            glfwGetCursorPos(m_window, &x, &y);
            m_inputState.mouseMovement = glm::vec2(x - m_lastCursorX, y - m_lastCursorY);
            m_lastCursorX = x;
            m_lastCursorY = y;

            m_inputState.freeLook = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

            m_inputState.setXAxisMovement(left, right);
            m_inputState.setZAxisMovement(backward, forward);
            m_inputState.setStepMovement(stepLeft, stepRight);
        }


        const InputState& getInputState() const
        {
            return m_inputState;
        }


    private:
        InputState m_inputState;

        gsl::not_null<GLFWwindow*> m_window;
        double m_lastCursorX = 0;
        double m_lastCursorY = 0;
    };
}