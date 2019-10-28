#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor; // 为了在片段着色器中引用顶点着色器定义的输出变量， 需要在片段着色器定义一个输入变量，并不需要使用相同的名称（fragColor），只要location指定的索引相同即可

layout(location = 0) out vec4 outColor;  // layout（location = 0）这里的0指定了帧缓冲区的索引。

void main() {  // main函数将会作用于每个片段,  片段着色器：计算每个片段（像素）的颜色值。同时通常会要求输入纹理，从而对每个片段进行着色贴图。
    outColor = vec4(fragColor, 1.0); // GLSL中的颜色是4个分量的矢量，其中R，G，B和α通道的值都在[0,1]范围内
}
