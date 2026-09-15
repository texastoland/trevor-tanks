#include "models.h"
#include "raymath.h"

// Minimal embedded directional-light shader (no asset files). Without a light
// shader raylib renders every face of a model the same flat color, which makes
// walls and crates read as floor paint instead of blocks.
static const char* LIT_VS = R"(
#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;
out vec2 fragTexCoord; out vec4 fragColor; out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
static const char* LIT_FS = R"(
#version 330
in vec2 fragTexCoord; in vec4 fragColor; in vec3 fragNormal;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 lightDir; uniform float ambient;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord);
    float ndl = max(dot(normalize(fragNormal), -lightDir), 0.0);
    float shade = ambient + (1.0 - ambient) * ndl;
    finalColor = vec4(texel.rgb * colDiffuse.rgb * fragColor.rgb * shade,
                      texel.a * colDiffuse.a * fragColor.a);
}
)";

static Shader buildLitShader() {
    Shader sh = LoadShaderFromMemory(LIT_VS, LIT_FS);
    Vector3 dir = Vector3Normalize({-0.4f, -1.0f, -0.35f});   // warm key light from upper-left
    float ambient = 0.45f;
    SetShaderValue(sh, GetShaderLocation(sh, "lightDir"), &dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "ambient"), &ambient, SHADER_UNIFORM_FLOAT);
    return sh;
}

Color tankColor(TankType t) {
    // sampled from gameplay footage: enemies are dull/muted next to the player's cobalt
    switch (t) {
        case TankType::Player: return {60, 105, 200, 255};    // vibrant cobalt blue
        case TankType::Brown:  return {158, 122, 78, 255};    // tan/khaki
        case TankType::Grey:   return {138, 135, 128, 255};
        case TankType::Teal:   return {78, 148, 150, 255};
        case TankType::Yellow: return {196, 172, 92, 255};    // muted olive-gold
        case TankType::Red:    return {226, 122, 138, 255};   // salmon pink ("Pink")
        case TankType::Green:  return {96, 148, 78, 255};
        case TankType::Purple: return {138, 106, 160, 255};   // muted violet
        case TankType::White:  return {235, 235, 235, 255};
        case TankType::Black:  return {52, 50, 48, 255};
        default:               return MAGENTA;
    }
}

Models buildModels() {
    Models m{};
    // proportions follow the Wii original: chunky proud treads, two-tone hull
    // with a lighter top plate, rounded dome turret, long barrel with muzzle
    m.hull = LoadModelFromMesh(GenMeshCube(0.62f, 0.20f, 0.46f));
    m.hull.transform = MatrixTranslate(0, 0.16f, 0);
    m.hullTop = LoadModelFromMesh(GenMeshCube(0.46f, 0.07f, 0.34f));
    m.hullTop.transform = MatrixTranslate(-0.02f, 0.29f, 0);
    m.tread = LoadModelFromMesh(GenMeshCube(0.78f, 0.20f, 0.18f));   // drawn twice, z = ±0.28
    m.dome = LoadModelFromMesh(GenMeshSphere(0.17f, 14, 14));
    m.dome.transform = MatrixMultiply(MatrixScale(1.0f, 0.62f, 1.0f), MatrixTranslate(0, 0.34f, 0));
    m.barrel = LoadModelFromMesh(GenMeshCylinder(0.045f, 0.58f, 10));
    // cylinder grows along +Y; lay it along +X, muzzle at x = BARREL_LEN
    m.barrel.transform = MatrixMultiply(MatrixRotateZ(-PI / 2), MatrixTranslate(0, 0.34f, 0));
    m.muzzle = LoadModelFromMesh(GenMeshCylinder(0.07f, 0.10f, 10));
    m.muzzle.transform = MatrixMultiply(MatrixRotateZ(-PI / 2), MatrixTranslate(0.48f, 0.34f, 0));
    m.wall  = LoadModelFromMesh(GenMeshCube(1.0f, 0.9f, 1.0f));
    m.crate = LoadModelFromMesh(GenMeshCube(0.96f, 0.7f, 0.96f));
    // bullet-shaped shell: casing cylinder + pointed nose cone, laid along +X
    m.shell = LoadModelFromMesh(GenMeshCylinder(0.065f, 0.16f, 10));
    m.shell.transform = MatrixMultiply(MatrixRotateZ(-PI / 2), MatrixTranslate(-0.08f, 0, 0));
    m.shellNose = LoadModelFromMesh(GenMeshCone(0.065f, 0.10f, 10));
    m.shellNose.transform = MatrixMultiply(MatrixRotateZ(-PI / 2), MatrixTranslate(0.08f, 0, 0));
    m.mine  = LoadModelFromMesh(GenMeshSphere(0.15f, 12, 12));   // the original mine is a red ball
    m.lit = buildLitShader();
    Model* all[] = {&m.hull, &m.hullTop, &m.tread, &m.dome, &m.barrel, &m.muzzle,
                    &m.wall, &m.crate, &m.shell, &m.shellNose, &m.mine};
    for (Model* mdl : all) mdl->materials[0].shader = m.lit;
    return m;
}

void buildFloor(Models& m, int aw, int ah) {
    if (m.floorTex.id) UnloadTexture(m.floorTex);
    if (m.floor.meshCount) UnloadModel(m.floor);
    // parquet: two planks per cell, tone varied by a hash; light warm wood
    // like the original, with soft brown seams instead of harsh black lines
    const int px = 16;
    Image img = GenImageColor(aw * px, ah * px, {160, 118, 76, 255});   // seam color
    for (int cy = 0; cy < ah; ++cy) {
        for (int cx = 0; cx < aw; ++cx) {
            for (int half = 0; half < 2; ++half) {
                unsigned hsh = (unsigned)(cx * 73856093 ^ cy * 19349663 ^ half * 83492791);
                float v = (hsh % 100) / 100.0f;
                Color c = {(unsigned char)(206 + v * 28), (unsigned char)(164 + v * 24),
                           (unsigned char)(110 + v * 20), 255};
                bool horiz = ((cx + cy) % 2) == 0;      // alternate plank direction
                int x = cx * px + (horiz ? 0 : half * (px / 2));
                int y = cy * px + (horiz ? half * (px / 2) : 0);
                int w = horiz ? px : px / 2;
                int h = horiz ? px / 2 : px;
                ImageDrawRectangle(&img, x, y, w - 1, h - 1, c);   // -1 leaves a seam line
            }
        }
    }
    m.floorTex = LoadTextureFromImage(img);
    UnloadImage(img);
    m.floor = LoadModelFromMesh(GenMeshPlane((float)aw, (float)ah, 1, 1));
    m.floor.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = m.floorTex;
    m.floor.materials[0].shader = m.lit;
}

void unloadModels(Models& m) {
    UnloadModel(m.hull); UnloadModel(m.hullTop); UnloadModel(m.tread);
    UnloadModel(m.dome); UnloadModel(m.barrel); UnloadModel(m.muzzle);
    UnloadModel(m.wall); UnloadModel(m.crate); UnloadModel(m.shell); UnloadModel(m.shellNose); UnloadModel(m.mine);
    if (m.floor.meshCount) UnloadModel(m.floor);
    if (m.floorTex.id) UnloadTexture(m.floorTex);
    UnloadShader(m.lit);
}
