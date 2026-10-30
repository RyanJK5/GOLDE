#include <gtest/gtest.h>
#include <sstream>

#include "ShaderManager.hpp"

namespace Golde {

TEST(ShaderParseTest, SplitsVertexAndFragment) {
    std::istringstream stream{"#shader vertex\n"
                              "void main() {}\n"
                              "#shader fragment\n"
                              "out vec4 color;\n"};
    const auto sources = ParseShaderSource(stream);
    ASSERT_TRUE(sources.has_value());
    EXPECT_EQ(sources->Vertex, "void main() {}\n");
    EXPECT_EQ(sources->Fragment, "out vec4 color;\n");
}

// Regression: a CRLF checkout left "vertex\r" as the section type, so every
// shader failed to load with "could not be read".
TEST(ShaderParseTest, AcceptsCrlfLineEndings) {
    std::istringstream stream{"#shader vertex\r\n"
                              "void main() {}\r\n"
                              "#shader fragment\r\n"
                              "out vec4 color;\r\n"};
    const auto sources = ParseShaderSource(stream);
    ASSERT_TRUE(sources.has_value());
    EXPECT_EQ(sources->Vertex, "void main() {}\n");
    EXPECT_EQ(sources->Fragment, "out vec4 color;\n");
}

TEST(ShaderParseTest, RejectsUnknownSectionType) {
    std::istringstream stream{"#shader vertex\n"
                              "#shader geometry\n"
                              "#shader fragment\n"};
    EXPECT_FALSE(ParseShaderSource(stream).has_value());
}

TEST(ShaderParseTest, RejectsMissingFragment) {
    std::istringstream stream{"#shader vertex\nvoid main() {}\n"};
    EXPECT_FALSE(ParseShaderSource(stream).has_value());
}
} // namespace Golde
