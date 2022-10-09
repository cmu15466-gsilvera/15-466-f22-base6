#include "Game.hpp"

#include "Connection.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection* connection_) const
{
    assert(connection_);
    auto& connection = *connection_;

    uint32_t size = 5;
    connection.send(Message::C2S_Controls);
    connection.send(uint8_t(size));
    connection.send(uint8_t(size >> 8));
    connection.send(uint8_t(size >> 16));

    auto send_button = [&](Button const& b) {
        if (b.downs & 0x80) {
            std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
        }
        connection.send(uint8_t((b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f)));
    };

    send_button(left);
    send_button(right);
    send_button(up);
    send_button(down);
    send_button(enter);
}

bool Player::Controls::recv_controls_message(Connection* connection_)
{
    assert(connection_);
    auto& connection = *connection_;

    auto& recv_buffer = connection.recv_buffer;

    // expecting [type, size_low0, size_mid8, size_high8]:
    if (recv_buffer.size() < 4)
        return false;
    if (recv_buffer[0] != uint8_t(Message::C2S_Controls))
        return false;
    uint32_t size = (uint32_t(recv_buffer[3]) << 16)
        | (uint32_t(recv_buffer[2]) << 8)
        | uint32_t(recv_buffer[1]);
    if (size != 5)
        throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 5!");

    // expecting complete message:
    if (recv_buffer.size() < 4 + size)
        return false;

    auto recv_button = [](uint8_t byte, Button* button) {
        button->pressed = (byte & 0x80);
        uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
        if (d > 255) {
            std::cerr << "got a whole lot of downs" << std::endl;
            d = 255;
        }
        button->downs = uint8_t(d);
    };

    recv_button(recv_buffer[4 + 0], &left);
    recv_button(recv_buffer[4 + 1], &right);
    recv_button(recv_buffer[4 + 2], &up);
    recv_button(recv_buffer[4 + 3], &down);
    recv_button(recv_buffer[4 + 4], &enter);

    // delete message from buffer:
    recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

    return true;
}

//-----------------------------------------

Game::Game()
    : mt(0x15466666)
{
    board = new GameBoard(board_size);
}

Player* Game::spawn_player()
{
    players.emplace_back();
    Player& player = players.back();

    // random point in the middle area of the arena:
    player.position = glm::ivec2(rand() % board->shape.x - 1, rand() % board->shape.y - 1);

    do {
        player.color.r = mt() / float(mt.max());
        player.color.g = mt() / float(mt.max());
        player.color.b = mt() / float(mt.max());
    } while (player.color == glm::vec3(0.0f));
    player.color = glm::normalize(player.color);

    player.name = "Player " + std::to_string(next_player_number++);

    return &player;
}

void Game::remove_player(Player* player)
{
    bool found = false;
    for (auto pi = players.begin(); pi != players.end(); ++pi) {
        if (&*pi == player) {
            players.erase(pi);
            found = true;
            break;
        }
    }
    assert(found);
}

void Game::update(float elapsed)
{
    board->reset();
    // position/velocity update:
    for (auto& p : players) {
        glm::ivec2 dir = glm::vec2(0, 0);
        if (p.controls.left.pressed)
            dir.x -= 1;
        if (p.controls.right.pressed)
            dir.x += 1;
        if (p.controls.down.pressed)
            dir.y -= 1;
        if (p.controls.up.pressed)
            dir.y += 1;
        p.position += dir;

        // clamp within bounds
        p.position.x = std::min(std::max(0, p.position.x), board->shape.x - 1);
        p.position.y = std::min(std::max(0, p.position.y), board->shape.y - 1);

        // reset 'downs' since controls have been handled:
        p.controls.left.downs = 0;
        p.controls.right.downs = 0;
        p.controls.up.downs = 0;
        p.controls.down.downs = 0;
        p.controls.enter.downs = 0;

        Tile::max_over = players.size();
        {
            // update the num_over
            size_t index = p.position.x + BOARD_WIDTH * p.position.y;
            board->board[index].num_over++;
        }
        {
            // colour this tile red (and not other tiles)
            if (last_tile != nullptr) {
                last_tile->colour_other = true;
            }
            auto& this_tile = board->GetTile(p.position);
            this_tile.colour_other = false; // colour with this colour
            this_tile.colour = glm::vec4(1.f, 0.f, 0.f, 1.f);
            last_tile = &this_tile;
        }
    }
}

void Game::send_state_message(Connection* connection_, Player* connection_player) const
{
    assert(connection_);
    auto& connection = *connection_;

    connection.send(Message::S2C_State);
    // will patch message size in later, for now placeholder bytes:
    connection.send(uint8_t(0));
    connection.send(uint8_t(0));
    connection.send(uint8_t(0));
    size_t mark = connection.send_buffer.size(); // keep track of this position in the buffer

    // send player info helper:
    auto send_player = [&](Player const& player) {
        connection.send(player.position);
        connection.send(player.color);

        // NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
        // effectively: truncates player name to 255 chars
        uint8_t len = uint8_t(std::min<size_t>(255, player.name.size()));
        connection.send(len);
        connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
    };

    // player count:
    connection.send(uint8_t(players.size()));
    if (connection_player)
        send_player(*connection_player);
    for (auto const& player : players) {
        if (&player == connection_player)
            continue;
        send_player(player);
    }

    // compute the message size and patch into the message header:
    uint32_t size = uint32_t(connection.send_buffer.size() - mark);
    connection.send_buffer[mark - 3] = uint8_t(size);
    connection.send_buffer[mark - 2] = uint8_t(size >> 8);
    connection.send_buffer[mark - 1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection* connection_)
{
    assert(connection_);
    auto& connection = *connection_;
    auto& recv_buffer = connection.recv_buffer;

    if (recv_buffer.size() < 4)
        return false;
    if (recv_buffer[0] != uint8_t(Message::S2C_State))
        return false;
    uint32_t size = (uint32_t(recv_buffer[3]) << 16)
        | (uint32_t(recv_buffer[2]) << 8)
        | uint32_t(recv_buffer[1]);
    uint32_t at = 0;
    // expecting complete message:
    if (recv_buffer.size() < 4 + size)
        return false;

    // copy bytes from buffer and advance position:
    auto read = [&](auto* val) {
        if (at + sizeof(*val) > size) {
            throw std::runtime_error("Ran out of bytes reading state message.");
        }
        std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
        at += sizeof(*val);
    };

    players.clear();
    uint8_t player_count;
    read(&player_count);
    for (uint8_t i = 0; i < player_count; ++i) {
        players.emplace_back();
        Player& player = players.back();
        read(&player.position);
        read(&player.color);
        uint8_t name_len;
        read(&name_len);
        // n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
        player.name = "";
        for (uint8_t n = 0; n < name_len; ++n) {
            char c;
            read(&c);
            player.name += c;
        }
    }

    if (at != size)
        throw std::runtime_error("Trailing data in state message.");

    // delete message from buffer:
    recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

    return true;
}
