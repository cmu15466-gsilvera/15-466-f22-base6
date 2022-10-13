#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

PlayMode::PlayMode(Client& client_)
    : client(client_)
{
    board = new GameBoard(board_size);
    pos = PlayMode::random_pos();
    srand(time(0));
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
            left.downs += 1;
            left.pressed = true;
            pos.x = std::max(0, pos.x - 1);
            return true;
        } else if (evt.key.keysym.sym == SDLK_RIGHT) {
            right.downs += 1;
            right.pressed = true;
            pos.x = std::min(board->shape.x - 1, pos.x + 1);
            return true;
        } else if (evt.key.keysym.sym == SDLK_UP) {
            up.downs += 1;
            up.pressed = true;
            pos.y = std::max(0, pos.y - 1);
            return true;
        } else if (evt.key.keysym.sym == SDLK_DOWN) {
            down.downs += 1;
            down.pressed = true;
            pos.y = std::min(board->shape.y - 1, pos.y + 1);
            return true;
        } else if (evt.key.keysym.sym == SDLK_RETURN || evt.key.keysym.sym == SDLK_SPACE) {
            enter.downs += 1;
            enter.pressed = true;
            return true;
        }
    } else if (evt.type == SDL_KEYUP) {
        if (evt.key.keysym.sym == SDLK_LEFT) {
            left.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RIGHT) {
            right.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_UP) {
            up.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_DOWN) {
            down.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_RETURN || evt.key.keysym.sym == SDLK_SPACE) {
            enter.pressed = false;
            return true;
        }
    }

    return false;
}

void PlayMode::update(float elapsed)
{

    // queue data for sending to server:
    if (left.downs || right.downs || down.downs || up.downs || enter.downs) {
        // send a five-byte message of type 'b':
        client.connections.back().send('b');
        client.connections.back().send(static_cast<unsigned char>(pos.x));
        client.connections.back().send(static_cast<unsigned char>(pos.y));
        client.connections.back().send(enter.pressed);
    }

    // reset button press counters:
    left.downs = 0;
    right.downs = 0;
    up.downs = 0;
    down.downs = 0;
    enter.downs = 0;

    // send/receive data:
    client.poll([this](Connection* c, Connection::Event event) {
        if (event == Connection::OnOpen) {
            std::cout << "[" << c->socket << "] opened" << std::endl;
        } else if (event == Connection::OnClose) {
            std::cout << "[" << c->socket << "] closed (!)" << std::endl;
            throw std::runtime_error("Lost connection to server!");
        } else {
            assert(event == Connection::OnRecv);
            std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n"
                      << hex_dump(c->recv_buffer);
            std::cout.flush();
            // expecting message(s) like 'm' + 3-byte length + length bytes of text:
            while (c->recv_buffer.size() >= 4) {
                std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n"
                          << hex_dump(c->recv_buffer);
                std::cout.flush();
                char type = c->recv_buffer[0];
                if (type != 'm') {
                    throw std::runtime_error("Server sent unknown message type '" + std::to_string(type) + "'");
                }
                uint32_t size = ((uint32_t(c->recv_buffer[1]) << 16) | (uint32_t(c->recv_buffer[2]) << 8) | (uint32_t(c->recv_buffer[3])));
                if (c->recv_buffer.size() < 4 + size)
                    break; // if whole message isn't here, can't process
                // whole message *is* here, so set current server message:
                server_message = std::string(c->recv_buffer.begin() + 4, c->recv_buffer.begin() + 4 + size);

                // and consume this part of the buffer:
                c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4 + size);
            }
        }
    },
        0.0);

    // update board state
    {
        for (int i = 0; i < server_message.size(); i++) {
            int new_num_over = static_cast<int>(std::fabs(server_message[i]));
            board->board[i].delta = board->board[i].num_over - new_num_over;
            board->board[i].num_over = new_num_over;
            // treasure located if server message < 0
            board->board[i].treasure = (server_message[i] < 0);
            // colour treasure yellow
            board->board[i].colour_other = true; // colour with this colour
            // num_players += static_cast<size_t>(server_message[i]);
        }
        Tile::max_over = 1;
    }

    {
        if (board->GetTile(pos).treasure && enter.pressed && last_found != pos) {
            score += 1;
            last_found = pos;
        }
    }

    // colour this tile blue
    {
        if (last_tile != nullptr) {
            last_tile->colour_other = true;
        }
        auto& this_tile = board->GetTile(pos);
        this_tile.colour_other = false; // colour with this colour
        this_tile.colour = glm::vec4(0.f, 0.f, 1.f, 1.f);
        last_tile = &this_tile;
    }
}

void PlayMode::draw(glm::uvec2 const& drawable_size)
{
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    { // use DrawLines to overlay some text:
        glDisable(GL_DEPTH_TEST);
        float aspect = float(drawable_size.x) / float(drawable_size.y);
        DrawLines lines(glm::mat4(
            1.0f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f));

        auto draw_text = [&](glm::vec2 const& at, std::string const& text, float H) {
            lines.draw_text(text,
                glm::vec3(at.x, at.y, 0.0),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0x00, 0x00, 0x00, 0x00));
            float ofs = 2.0f / drawable_size.y;
            lines.draw_text(text,
                glm::vec3(at.x + ofs, at.y + ofs, 0.0),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0xff, 0xff, 0xff, 0x00));
        };

        // draw_text(glm::vec2(-aspect + 0.1f, 0.0f), server_message, 0.09f);

        draw_text(glm::vec2(-aspect + 0.1f, -0.9f), "(Your score: " + std::to_string(score) + ")", 0.09f);
    }

    // draw game board
    {
        board->draw(drawable_size);
    }

    GL_ERRORS();
}