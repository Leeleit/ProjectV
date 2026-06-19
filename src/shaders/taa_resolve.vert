#version 460



void main()
{

    const vec2 positionNdc = vec2(
        (gl_VertexIndex == 1u) ? 3.0 : -1.0,
        (gl_VertexIndex == 2u) ? 3.0 : -1.0);
    gl_Position = vec4(positionNdc, 0.0, 1.0);
}
