const int CSMSplits = 5;

layout(std140, binding=2) uniform CSM {
    mat4 lightMVP[CSMSplits];
    vec3 lightDir;
} csm;
