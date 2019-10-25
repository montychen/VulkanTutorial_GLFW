#version 450
#extension GL_ARB_separate_shader_objects : enable // Vulkan着色器需要GL_ARB_separate_shader_objects扩展才能工作

// 将颜色输出变量添加到顶点着色器并在main函数中写入， 为了在片段着色器中引用这个输出变量， 需要在片段着色器定义一个输入变量，并不需要使用相同的名称（fragColor），只要location指定的索引相同即可
layout(location = 0) out vec3 fragColor;  // layout（location = 0）这里的0指定了帧缓冲区的索引。

vec2 positions[3] = vec2[](     // 这里直接使用 NDC归一化坐标范围内的坐标值来定义三角形的顶点
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](        // 为每一个顶点指定不同的颜色
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);


void main() {  //  main函数将会作用于每个顶点。 顶点着色器: 最重要的功能是执行顶点的坐标变换和逐顶点光照。顶点坐标由局部坐标转换到 *归一化设备坐标NDC* 的运算，就是在这里发生的
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); // 内置变量gl_VertexIndex 包含了当前顶点的索引，通常是顶点缓冲区的索引
    fragColor = colors[gl_VertexIndex];   //  内置变量gl_Position用作输出
}
