#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform CameraProperties {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
} camera;

layout(set = 1, binding = 0) uniform ObjectProperties {
    mat4 transform;
} object;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = camera.viewproj * object.transform * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = texCoord;
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