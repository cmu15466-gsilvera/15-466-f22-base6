#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <array>
#include <random>

PlayMode::PlayMode(Client& client_)
    : client(client_)
{
    board = new GameBoard({ 10, 10 });
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const& evt, glm::uvec2 const& window_size)
{

    if (evt.type == SDL_KEYDOWN) {
        if (evt.key.repeat) {
            // ignore repeats
        } else if (evt.key.keysym.sym == SDLK_LEFT) {
            controls.left.downs += 1;
            controls.left.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RIGHT) {
            controls.right.downs += 1;
            controls.right.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_UP) {
            controls.up.downs += 1;
            controls.up.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_DOWN) {
            controls.down.downs += 1;
            controls.down.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RETURN) {
            controls.enter.downs += 1;
            controls.enter.pressed = true;
            return true;
        }
    } else if (evt.type == SDL_KEYUP) {
        if (evt.key.keysym.sym == SDLK_LEFT) {
            controls.left.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RIGHT) {
            controls.right.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_UP) {
            controls.up.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_DOWN) {
            controls.down.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RETURN) {
            controls.enter.pressed = false;
            return true;
        }
    }

    return false;
}

void PlayMode::update(float elapsed)
{

    // queue data for sending to server:
    controls.send_controls_message(&client.connection);

    // reset button press counters:
    controls.left.downs = 0;
    controls.right.downs = 0;
    controls.up.downs = 0;
    controls.down.downs = 0;
    controls.enter.downs = 0;

    // send/receive data:
    client.poll([this](Connection* c, Connection::Event event) {
        if (event == Connection::OnOpen) {
            std::cout << "[" << c->socket << "] opened" << std::endl;
        } else if (event == Connection::OnClose) {
            std::cout << "[" << c->socket << "] closed (!)" << std::endl;
            throw std::runtime_error("Lost connection to server!");
        } else {
            assert(event == Connection::OnRecv);
            // std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
            bool handled_message;
            try {
                do {
                    handled_message = false;
                    if (game.recv_state_message(c))
                        handled_message = true;
                } while (handled_message);
            } catch (std::exception const& e) {
                std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
                // quit the game:
                throw e;
            }
        }
    },
        0.0);
}

void PlayMode::draw(glm::uvec2 const& drawable_size)
{

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // figure out view transform to center the arena:
    float aspect = float(drawable_size.x) / float(drawable_size.y);
    float scale = std::min(
        2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
        2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius));
    glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

    glm::mat4 world_to_clip = glm::mat4(
        scale / aspect, 0.0f, 0.0f, offset.x,
        0.0f, scale, 0.0f, offset.y,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    // draw game board
    {
        board->draw(drawable_size);
    }

    GL_ERRORS();
}
