
#include "Connection.hpp"
#include "PlayMode.hpp" // BOARD_WIDTH, BOARD_HEIGHT

#include "hex_dump.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <time.h>
#include <unordered_map>

#ifdef _WIN32
extern "C" {
uint32_t GetACP();
}
#endif
int main(int argc, char** argv)
{
#ifdef _WIN32
    { // when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
        // see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
        uint32_t code_page = GetACP();
        if (code_page == 65001) {
            std::cout << "Code page is properly set to UTF-8." << std::endl;
        } else {
            std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
        }
    }

    // when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
    try {
#endif

        //------------ argument parsing ------------

        if (argc != 2) {
            std::cerr << "Usage:\n\t./server <port>" << std::endl;
            return 1;
        }

        //------------ initialization ------------

        Server server(argv[1]);

        //------------ main loop ------------
        constexpr float ServerTick = 1.0f / 30.0f; // TODO: set a server tick that makes sense for your game

        // server state:

        // per-client state:
        struct PlayerInfo {
            PlayerInfo()
            {
                static uint32_t next_player_id = 1;
                name = "Player" + std::to_string(next_player_id);
                next_player_id += 1;
            }
            std::string name;

            uint32_t pos_x = -1;
            uint32_t pos_y = -1;
            bool enter_pressed = false;

            int32_t total = 0;
        };
        std::unordered_map<Connection*, PlayerInfo> players;
        srand(time(0));
        int treasure_x = rand() % (BOARD_WIDTH - 1);
        int treasure_y = rand() % (BOARD_WIDTH - 1);

        while (true) {
            static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration<double>(ServerTick);
            // process incoming data from clients until a tick has elapsed:
            while (true) {
                auto now = std::chrono::steady_clock::now();
                double remain = std::chrono::duration<double>(next_tick - now).count();
                if (remain < 0.0) {
                    next_tick += std::chrono::duration<double>(ServerTick);
                    break;
                }
                server.poll([&](Connection* c, Connection::Event evt) {
                    if (evt == Connection::OnOpen) {
                        // client connected:

                        // create some player info for them:
                        players.emplace(c, PlayerInfo());

                    } else if (evt == Connection::OnClose) {
                        // client disconnected:

                        // remove them from the players list:
                        auto f = players.find(c);
                        assert(f != players.end());
                        players.erase(f);

                    } else {
                        assert(evt == Connection::OnRecv);
                        // got data from client:
                        std::cout << "got bytes:\n"
                                  << hex_dump(c->recv_buffer);
                        std::cout.flush();

                        // look up in players list:
                        auto f = players.find(c);
                        assert(f != players.end());
                        PlayerInfo& player = f->second;

                        // handle messages from client:
                        // TODO: update for the sorts of messages your clients send
                        const size_t msg_len = 4;
                        while (c->recv_buffer.size() >= msg_len) {
                            // expecting five-byte messages 'b' (left count) (right count) (down count) (up count)
                            char type = c->recv_buffer[0];
                            if (type != 'b') {
                                std::cout << " message of non-'b' type received from client!" << std::endl;
                                // shut down client connection:
                                c->close();
                                return;
                            }
                            uint8_t pos_x = c->recv_buffer[1];
                            uint8_t pos_y = c->recv_buffer[2];
                            uint8_t enter_count = c->recv_buffer[3];

                            player.pos_x = pos_x;
                            player.pos_y = pos_y;
                            player.enter_pressed = enter_count;
                            if (player.pos_x == treasure_x && player.pos_y == treasure_y && player.enter_pressed > 0) {
                                // randomize the treasure location
                                do {
                                    treasure_x = rand() % (BOARD_WIDTH - 1);
                                    treasure_y = rand() % (BOARD_WIDTH - 1);
                                    // ensure won't randomly respawn on the same tile
                                } while (treasure_x == player.pos_x || treasure_y == player.pos_y);
                            }

                            c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + msg_len);
                        }
                    }
                },
                    remain);
            }

            // update current game state
            // TODO: replace with *your* game state update
            constexpr size_t msg_len = BOARD_WIDTH * BOARD_HEIGHT;
            char board[msg_len] = { 0 };

            for (auto& [c, player] : players) {
                int idx = player.pos_x + player.pos_y * BOARD_WIDTH;
                // std::cout << "position: " << player.pos_x << " " << player.pos_y << std::endl;
                if (idx >= 0 && idx < msg_len)
                    board[idx]++;
            }

            size_t treasure_idx = treasure_x + BOARD_WIDTH * treasure_y;
            board[treasure_idx] = -board[treasure_idx];
            std::string status_message(board, msg_len);
            // std::cout << status_message << std::endl; // DEBUG

            // send updated game state to all clients
            // TODO: update for your game state
            for (auto& [c, player] : players) {
                (void)player; // work around "unused variable" warning on whatever g++ github actions uses
                // send an update starting with 'm', a 24-bit size, and a blob of text:
                c->send('m');
                c->send(uint8_t(status_message.size() >> 16));
                c->send(uint8_t((status_message.size() >> 8) % 256));
                c->send(uint8_t(status_message.size() % 256));
                c->send_buffer.insert(c->send_buffer.end(), status_message.begin(), status_message.end());
            }
        }

        return 0;

#ifdef _WIN32
    } catch (std::exception const& e) {
        std::cerr << "Unhandled exception:\n"
                  << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unhandled exception (unknown type)." << std::endl;
        throw;
    }
#endif
}