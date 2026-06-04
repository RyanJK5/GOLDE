#shader vertex
#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;

uniform mat4 u_MVP;

out vec2 v_TexCoord;

void main()
{
    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}

#shader fragment
#version 330 core

layout(location = 0) out vec4 color;

uniform vec4 u_Color;
uniform sampler2D u_StateTex;

in vec2 v_TexCoord;

void main() 
{
    float alive = texture(u_StateTex, v_TexCoord).r;
    if (alive < 0.5) {
        discard;
    }
    color = u_Color;
}