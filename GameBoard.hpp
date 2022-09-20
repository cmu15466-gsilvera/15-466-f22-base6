#pragma once

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <vector>

// got rendering help from: https://learnopengl.com/Getting-started/Hello-Triangle
struct Tile {
    int num_over = 0;
    inline static size_t max_over = 1; // how to determine when a tile is white (maximally coloured)

    constexpr static glm::vec2 block_size = glm::vec2(0.1, 0.1);

    // for indexing
    inline static size_t static_tile_index = 0;
    const glm::ivec2 pos;
    size_t tile_id = 0;

    // for drawing
    inline static GLuint draw_program = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    glm::vec4 colour;
    bool colour_other = true;

    Tile(const glm::vec2& p)
        : pos(p)
    {
        tile_id = static_tile_index;
        static_tile_index++;

        std::cout << "constructed tile at: " << glm::to_string(p) << std::endl;

        // create shaders for rendering
        {
            const auto vertex_shader = "#version 330 core\n"
                                       "layout (location = 0) in vec3 vertex;\n"
                                       "void main()\n"
                                       "{\n"
                                       "    gl_Position = vec4(vertex.xyz, 1.0);\n"
                                       "}\n";

            const auto fragment_shader = "#version 330 core\n"
                                         "out vec4 color;\n"
                                         "uniform vec4 colour;\n"
                                         "void main()\n"
                                         "{\n"
                                         "   color = colour;\n"
                                         "}\n";
            if (Tile::draw_program == 0) {
                Tile::draw_program = gl_compile_program(vertex_shader, fragment_shader);
            }

            // initialize vertex buffer (what data is sent to the GPU)
            float vertices[] = {
                // first triangle
                -0.5f + block_size.x * pos.x, 0.5f - block_size.y * pos.y, 0.0f, // top right
                -0.5f + block_size.x * pos.x, 0.5f - block_size.y * (pos.y + 1), 0.0f, // bottom right
                -0.5f + block_size.x * (pos.x + 1), 0.5f - block_size.y * (pos.y + 1), 0.0f, // top left
                // second triangle
                -0.5f + block_size.x * pos.x, 0.5f - block_size.y * pos.y, 0.0f, // bottom right
                -0.5f + block_size.x * (pos.x + 1), 0.5f - block_size.y * pos.y, 0.0f, // bottom left
                -0.5f + block_size.x * (pos.x + 1), 0.5f - block_size.y * (pos.y + 1), 0.0f, // top left
            };

            // init arrays
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);

            // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            // set the vertex attributes pointers
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            // glBindVertexArray(0);
            GL_ERRORS();
        }
    }

    void draw()
    {
        glUseProgram(Tile::draw_program);
        if (colour_other) {
            float lum = num_over / static_cast<float>(max_over);
            colour = glm::vec4(lum, lum, lum, 1.0);
        }
        // send colour to shader
        glUniform4f(glGetUniformLocation(Tile::draw_program, "colour"), colour.x, colour.y, colour.z, colour.w);
        glBindVertexArray(VAO);

        glDrawArrays(GL_TRIANGLES, 0, 6); // 2 triangles => quad

        GL_ERRORS();
    }
};

struct GameBoard {
    GameBoard(const glm::ivec2& s)
        : shape(s)
    {
        // board.resize(shape.x * shape.y);
        for (int i = 0; i < shape.x * shape.y; i++) {
            board.push_back(Tile({ i % shape.x, i / shape.x }));
        }
    }

    std::vector<Tile> board;
    const glm::ivec2 shape;

    Tile& GetTile(const glm::ivec2& pos)
    {
        size_t index = pos.x + pos.y * shape.y;
        return board[index];
    }

    void reset()
    {
        for (auto& t : board) {
            t.num_over = 0;
        }
    }

    void draw(const glm::vec2& drawable_size)
    {
        // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        // glClear(GL_COLOR_BUFFER_BIT);

        // glDisable(GL_DEPTH_TEST);
        // float aspect = float(drawable_size.x) / float(drawable_size.y);

        for (int i = 0; i < board.size(); i++) {
            board[i].draw();
        }
        GL_ERRORS();
    }
};
