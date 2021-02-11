#version 450

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
/*
layout (binding=0, std140) uniform Matrices {
    mat4 projModelViewMatrix;
    mat3 normalMatrix;
};
 
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
 
layout(location=0) out VertexData {
    vec2 texCoord;
    vec3 normal;
} VertexOut;
 
void main()
{
    VertexOut.texCoord = texCoord;
    VertexOut.normal = normalize(normalMatrix * normal);    
    gl_Position = projModelViewMatrix * vec4(position, 1.0);
}
*/