#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aAlbedo;
layout (location = 3) in float aMetallic;
layout (location = 4) in float aRoughness;
layout (location = 5) in float aAo;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec3 Albedo;
layout(location = 3) out float Metallic;
layout(location = 4) out float Roughness;
layout(location = 5) out float Ao;

void main()
{
    gl_Position = pc.projection * pc.modelview * vec4(aPos, 1.0);
    FragPos = vec3(pc.modelview * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(pc.modelview))) * aNormal;
    //LightPos = vec3(view * vec4(lightPos, 1.0)); // Transform world-space light position to 
    Albedo = aAlbedo;
    Metallic = aMetallic;
    Roughness = aRoughness;
    Ao = aAo;
}

