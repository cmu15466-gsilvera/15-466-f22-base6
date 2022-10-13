#include "Mode.hpp"

#include "Connection.hpp"
#include "GameBoard.hpp"

#include <glm/glm.hpp>

#include <deque>
#include <time.h>
#include <vector>

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 10

struct PlayMode : Mode {
    PlayMode(Client& client);
    virtual ~PlayMode();

    // functions called by main loop:
    virtual bool handle_event(SDL_Event const&, glm::uvec2 const& window_size) override;
    virtual void update(float elapsed) override;
    virtual void draw(glm::uvec2 const& drawable_size) override;

    //----- game state -----

    constexpr static glm::ivec2 board_size = glm::ivec2(BOARD_WIDTH, BOARD_HEIGHT);

    // input tracking:
    struct Button {
        uint8_t downs = 0;
        uint8_t pressed = 0;
    } left, right, down, up, enter;

    // last message from server:
    std::string server_message;

    struct GameBoard* board;
    struct Tile* last_tile = nullptr;
    int score = 0;

    // position on the game board
    glm::ivec2 pos;
    static glm::ivec2 random_pos()
    {
        return glm::ivec2(rand() % (BOARD_WIDTH - 1), rand() % (BOARD_HEIGHT - 1));
    }

    // connection to server:
    Client& client;
};