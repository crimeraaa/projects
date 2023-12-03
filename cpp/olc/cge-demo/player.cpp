#include "olcConsoleGameEngine.hpp"

class Demo : public olcConsoleGameEngine {
private:
    struct Position {
        float x;
        float y;
    };
    Position m_coord;
public:
    /**
     * @note MUST be overriden!
     */
    bool OnUserCreate() override {
        m_coord.x = 10.0f;
        m_coord.y = 10.0f;
        return true;
    }

    /**
     * Handles input as well.
     * @param dt Delta time, or elapsed time. 
     * @note MUST be overriden!
     */
    bool OnUserUpdate(float dt) override {
        // Try other factors!
        const float offset = 30.0f * dt;

        // booleans evalute to integers, i.e. true = 1 and false = 0
        // so we can do math operations with 'em!
        m_coord.x -= m_keys[VK_LEFT].bHeld  * offset;
        m_coord.x += m_keys[VK_RIGHT].bHeld * offset;
        m_coord.y -= m_keys[VK_UP].bHeld    * offset;
        m_coord.y += m_keys[VK_DOWN].bHeld  * offset;

        // TODO: Causes bad exit code, seems handles and such not being closed
        // if (m_keys[VK_LCONTROL].bHeld && m_keys['C'].bHeld) {
        //     return false;
        // }
         
        // Fill screen with whitespace first
        Fill(0, 0, m_nScreenWidth, m_nScreenHeight, L' ', 0);

        // Draw player as a white rectangle
        Fill(static_cast<int>(m_coord.x), 
             static_cast<int>(m_coord.y), 
             static_cast<int>(m_coord.x + 4), 
             static_cast<int>(m_coord.y + 4),
             PIXEL_SOLID,
             FG_GREEN);
        
        return true;
    }
};

int main() {
    Demo demo;
    demo.ConstructConsole(60, 90, 8, 8);
    demo.Start();
    return 0;
}
