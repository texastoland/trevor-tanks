#include "audio.h"
#include "raylib.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

static void buildMusic();
static void unloadMusic();
static void buildTread();

static const int SR = 22050;

// gen(i, t) returns a sample in [-1, 1]
static Sound synth(float seconds, const std::function<float(int, float)>& gen) {
    int n = (int)(seconds * SR);
    std::vector<short> data(n);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / SR;
        float v = gen(i, t);
        data[i] = (short)(clampf(v, -1.0f, 1.0f) * 32000);
    }
    Wave w{(unsigned)n, SR, 16, 1, data.data()};
    return LoadSoundFromWave(w);          // copies the buffer
}

static float noise() { return (std::rand() % 2000) / 1000.0f - 1.0f; }
static float sqr(float ph) { return std::fmod(ph, 1.0f) < 0.5f ? 1.0f : -1.0f; }

static struct {
    Sound fire, bounce, poof, tankBoom, mineBoom, lay, crate;
    bool ready = false;
} S;

bool audioInit() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return false;
    std::srand(1234);
    S.fire = synth(0.14f, [](int, float t) {        // punchy square sweep 220->70 Hz
        float f = 220 - 1000 * t;
        if (f < 70) f = 70;
        return sqr(f * t) * std::exp(-18 * t) * 0.6f;
    });
    S.bounce = synth(0.06f, [](int, float t) {      // clink
        return std::sin(2 * PI_F * 660 * t) * std::exp(-40 * t) * 0.4f;
    });
    S.poof = synth(0.12f, [](int, float t) {        // soft puff
        static float lp = 0;
        lp += 0.15f * (noise() - lp);
        return lp * std::exp(-25 * t) * 0.8f;
    });
    S.tankBoom = synth(0.6f, [](int, float t) {     // filtered noise boom
        static float lp = 0;
        lp += 0.08f * (noise() - lp);
        return (lp * 2.5f + 0.3f * std::sin(2 * PI_F * (80 - 60 * t) * t)) * std::exp(-6 * t);
    });
    S.mineBoom = synth(0.8f, [](int, float t) {     // deeper, longer
        static float lp = 0;
        lp += 0.05f * (noise() - lp);
        return (lp * 3.0f + 0.4f * std::sin(2 * PI_F * (55 - 35 * t) * t)) * std::exp(-4.5f * t);
    });
    S.lay = synth(0.08f, [](int, float t) {         // arming blip
        return std::sin(2 * PI_F * 500 * t) * std::exp(-30 * t) * 0.35f;
    });
    S.crate = synth(0.2f, [](int, float t) {        // crackle
        static float lp = 0;
        lp += 0.4f * (noise() - lp);
        return lp * std::exp(-12 * t) * 0.7f;
    });
    S.ready = true;
    buildMusic();
    buildTread();
    return true;
}

static struct {
    Sound lo, hi;
    Sound alias[2][4];
    int next[2] = {0, 0};
    bool ready = false;
} T;

static void buildTread() {
    // the measured frequency centers are RESONANCES, not tones: each stroke is
    // broadband noise pushed through wide two-pole resonators at those centers
    // (sines at the same frequencies sound like a chime, not machinery)
    auto resonator = [](float& y1, float& y2, float n, float f, float r, float g) {
        float w = 2 * PI_F * f / SR;
        float y = 2 * r * std::cos(w) * y1 - r * r * y2 + n * g;
        y2 = y1; y1 = y;
        return y;
    };
    // both strokes live HIGH (the original's click centroid sits ~4kHz):
    // bright filtered noise with mild resonant coloring, no low body at all
    T.lo = synth(0.035f, [resonator](int, float t) {  // the lower of two high ticks
        static float a1 = 0, a2 = 0, b1 = 0, b2 = 0, hp = 0;
        float n = noise();
        hp += 0.5f * (n - hp);
        float v = resonator(a1, a2, n, 1550, 0.88f, 0.15f) +
                  resonator(b1, b2, n, 3100, 0.85f, 0.18f) +
                  (n - hp) * 0.24f;
        return v * std::exp(-110 * t);
    });
    T.hi = synth(0.03f, [resonator](int, float t) {   // the higher tick
        static float a1 = 0, a2 = 0, b1 = 0, b2 = 0, hp = 0;
        float n = noise();
        hp += 0.65f * (n - hp);
        float v = resonator(a1, a2, n, 2500, 0.87f, 0.14f) +
                  resonator(b1, b2, n, 4200, 0.83f, 0.18f) +
                  (n - hp) * 0.27f;
        return v * std::exp(-130 * t);
    });
    for (int i = 0; i < 4; ++i) {
        T.alias[0][i] = LoadSoundAlias(T.lo);
        T.alias[1][i] = LoadSoundAlias(T.hi);
    }
    T.ready = true;
}

void audioTreadTick(bool isPlayer, bool highTone) {
    if (!S.ready || !T.ready) return;
    int v = highTone ? 1 : 0;
    Sound& s = T.alias[v][T.next[v]];
    T.next[v] = (T.next[v] + 1) % 4;
    SetSoundVolume(s, isPlayer ? 0.40f : 0.24f);
    SetSoundPitch(s, 0.94f + (std::rand() % 100) * 0.0012f);   // slight mechanical variation
    PlaySound(s);
}

void audioHandle(const std::vector<GameEvent>& events) {
    if (!S.ready) return;
    for (auto& e : events) {
        switch (e.kind) {
            case GameEvent::Fire:        PlaySound(S.fire); break;
            case GameEvent::Bounce:      PlaySound(S.bounce); break;
            case GameEvent::ShellPoof:   PlaySound(S.poof); break;
            case GameEvent::TankExplode: PlaySound(S.tankBoom); break;
            case GameEvent::MineExplode: PlaySound(S.mineBoom); break;
            case GameEvent::MineLay:     PlaySound(S.lay); break;
            case GameEvent::CrateBreak:  PlaySound(S.crate); break;
        }
    }
}

void audioShutdown() {
    if (!S.ready) return;
    UnloadSound(S.fire); UnloadSound(S.bounce); UnloadSound(S.poof);
    UnloadSound(S.tankBoom); UnloadSound(S.mineBoom); UnloadSound(S.lay);
    UnloadSound(S.crate);
    if (T.ready) {
        for (int v = 0; v < 2; ++v)
            for (int i = 0; i < 4; ++i) UnloadSoundAlias(T.alias[v][i]);
        UnloadSound(T.lo);
        UnloadSound(T.hi);
        T.ready = false;
    }
    unloadMusic();
    CloseAudioDevice();
}

// ---------------- layered percussion soundtrack ----------------
// Every tank type contributes one instrument; a layer plays only while a tank
// of that type is alive. Mixed sample-accurately into one audio stream: bar
// boundaries land on exact samples, so the march never jitters or drifts.
static const float BAR_LEN = 2.0f;               // one 4/4 bar at 120 BPM
static const int BAR_SAMPLES = (int)(BAR_LEN * SR);
static const int CHUNK = 2048;

static struct {
    std::vector<float> base;
    std::vector<float> pat[(int)TankType::COUNT];
    std::vector<float> grind;        // continuous tread bed while armor is moving
    float bedLevel = 0, bedTarget = 0;
    int grindHead = 0;
    AudioStream stream{};
    int playhead = 0;
    bool active = false;
    bool alive[(int)TankType::COUNT] = {false};
    bool useAlive[(int)TankType::COUNT] = {false};
    bool ready = false;
} M;

static void hitAt(std::vector<float>& buf, float at, float dur,
                  const std::function<float(float)>& gen) {
    int start = (int)(at * SR), len = (int)(dur * SR);
    for (int i = 0; i < len && start + i < (int)buf.size(); ++i)
        buf[start + i] += gen((float)i / SR);
}

static std::vector<float> bakeBuf(const std::function<void(std::vector<float>&)>& fill, float vol) {
    std::vector<float> buf(BAR_SAMPLES, 0.0f);
    fill(buf);
    for (auto& v : buf) v *= vol;
    return buf;
}

static float kick(float t) { return std::sin(2 * PI_F * (60 + 40 * std::exp(-30 * t)) * t) * std::exp(-12 * t); }
static float snr(float t) {                       // band-limited marching snare (mid band)
    static float lp = 0, lp2 = 0;
    float n = noise();
    lp += 0.35f * (n - lp);
    lp2 += 0.06f * (n - lp2);
    return (lp - lp2) * std::exp(-25 * t) * 1.5f;
}
static float hat(float t) {                       // high-passed tick (top band only)
    static float hp = 0;
    float n = noise();
    hp += 0.55f * (n - hp);
    return (n - hp) * std::exp(-90 * t) * 0.8f;
}
static float block(float t) { return std::sin(2 * PI_F * 1100 * t) * std::exp(-45 * t) * 0.55f; }
static float bell(float t) { return (sqr(560 * t) + sqr(845 * t)) * 0.5f * std::exp(-18 * t) * 0.4f; }
static float tomHi(float t) { return std::sin(2 * PI_F * 160 * t) * std::exp(-14 * t) * 0.8f; }
static float tomLo(float t) { return std::sin(2 * PI_F * 100 * t) * std::exp(-12 * t) * 0.8f; }
static float jingle(float t) { return noise() * (0.5f + 0.5f * std::sin(2 * PI_F * 70 * t)) * std::exp(-10 * t) * 0.3f; }
static float rim(float t) { return std::sin(2 * PI_F * 1800 * t) * std::exp(-70 * t) * 0.5f; }
static float brush(float t) { return noise() * 0.14f * std::sin(PI_F * t / 0.5f); }
static float crash(float t) { return noise() * std::exp(-3.0f * t) * 0.4f; }

static void buildMusic() {
    // footage analysis: continuous 16th marching snare with beat accents at
    // 120 BPM; ghost notes stay well under the accents so it reads as a march,
    // not a wall of snare
    M.base = bakeBuf([](std::vector<float>& b) {
        for (int i = 0; i < 16; ++i) {
            float acc = i == 0 || i == 8 ? 1.0f : (i % 4 == 0 ? 0.5f : 0.16f);
            hitAt(b, i * (BAR_LEN / 16.0f), 0.10f,
                  [acc](float t) { return snr(t) * acc * 1.6f; });
        }
        hitAt(b, 0.0f, 0.25f, [](float t) { return kick(t) * 0.35f; });
    }, 0.17f);   // the bed sits well under the type layers
    auto P = [&](TankType t, std::function<void(std::vector<float>&)> f, float v) {
        M.pat[(int)t] = bakeBuf(f, v);
    };
    P(TankType::Brown, [](std::vector<float>& b) {        // sparse woodblock
        hitAt(b, 0.75f, 0.1f, block); hitAt(b, 1.75f, 0.1f, block);
    }, 0.50f);
    P(TankType::Grey, [](std::vector<float>& b) {         // rim clicks (read over the snare)
        for (float at : {0.5f, 0.75f, 1.5f, 1.75f}) hitAt(b, at, 0.06f, rim);
    }, 0.45f);
    P(TankType::Teal, [](std::vector<float>& b) {         // steady hats
        for (int i = 0; i < 8; ++i) hitAt(b, i * 0.25f, 0.05f, hat);
    }, 0.42f);
    P(TankType::Yellow, [](std::vector<float>& b) {       // tambourine shimmer
        for (float at : {0.5f, 1.0f, 1.5f}) hitAt(b, at, 0.25f, jingle);
    }, 0.50f);
    P(TankType::Red, [](std::vector<float>& b) {          // marching toms
        for (float at : {0.0f, 0.375f, 0.75f, 1.0f, 1.375f, 1.75f}) hitAt(b, at, 0.2f, tomHi);
    }, 0.42f);
    P(TankType::Green, [](std::vector<float>& b) {        // cowbell on the backbeat
        hitAt(b, 0.5f, 0.15f, bell); hitAt(b, 1.5f, 0.15f, bell);
    }, 0.45f);
    P(TankType::Purple, [](std::vector<float>& b) {       // syncopated low toms
        for (float at : {0.25f, 1.0f, 1.25f}) hitAt(b, at, 0.25f, tomLo);
    }, 0.45f);
    P(TankType::White, [](std::vector<float>& b) {        // ghostly brush swell
        hitAt(b, 0.0f, 0.5f, brush); hitAt(b, 1.0f, 0.5f, brush);
    }, 0.42f);
    P(TankType::Black, [](std::vector<float>& b) {        // crash + closing roll
        hitAt(b, 0.0f, 1.0f, crash);
        for (int i = 0; i < 6; ++i) hitAt(b, 1.75f + i * 0.04f, 0.04f, snr);
    }, 0.42f);
    // the original's tread audio is a CONTINUOUS grind with click accents:
    // 35ms after a click its envelope still sits at ~0.8. Bake a crackly
    // rolling texture; the mixer fades it with how much armor is moving.
    M.grind.assign(BAR_SAMPLES, 0.0f);
    {
        // lesson learned: the 'measured bed slope' was actually the original's
        // MUSIC bleeding into between-click frames - any low-frequency body
        // reads as distant explosions. The real tread sustain lives only in
        // the click band: high-passed shimmer, nothing below ~1kHz.
        float g = 0, lp = 0;
        for (int i = 0; i < BAR_SAMPLES; ++i) {
            float n = noise();
            g += 0.30f * (noise() - g);              // fine roughness, not lumps
            float grain = 0.88f + 0.24f * g;
            lp += 0.30f * (n - lp);
            M.grind[i] = (n - lp) * grain;           // high-band only
        }
        // normalize to an explicit RMS so filter changes can never silently
        // change loudness again (shape and level are now independent knobs)
        float rms = 0;
        for (float v : M.grind) rms += v * v;
        rms = std::sqrt(rms / M.grind.size());
        float k = rms > 1e-6f ? 0.008f / rms : 0;   // calibrated: just above the music floor
        for (float& v : M.grind) v = clampf(v * k, -1.0f, 1.0f);
    }
    SetAudioStreamBufferSizeDefault(CHUNK);
    M.stream = LoadAudioStream(SR, 16, 1);
    PlayAudioStream(M.stream);
    M.ready = true;
}

static void unloadMusic() {
    if (!M.ready) return;
    StopAudioStream(M.stream);
    UnloadAudioStream(M.stream);
    M.ready = false;
}

void audioTreadBed(float level) {
    M.bedTarget = clampf(level, 0.0f, 1.2f);
}

void audioMusicTick(float, const bool typeAlive[], bool active) {
    if (!S.ready || !M.ready) return;
    for (int t = 0; t < (int)TankType::COUNT; ++t) M.alive[t] = typeAlive[t];
    if (!active) M.playhead = 0;                  // resume lands on a downbeat
    M.active = active;
    static short chunk[CHUNK];
    while (IsAudioStreamProcessed(M.stream)) {
        for (int i = 0; i < CHUNK; ++i) {
            float v = 0.0f;
            if (M.active) {
                if (M.playhead == 0)              // layer set changes only on the bar
                    for (int t = 0; t < (int)TankType::COUNT; ++t) M.useAlive[t] = M.alive[t];
                v = M.base[M.playhead];
                for (int t = (int)TankType::Brown; t < (int)TankType::COUNT; ++t)
                    if (M.useAlive[t] && !M.pat[t].empty()) v += M.pat[t][M.playhead];
                M.playhead = (M.playhead + 1) % BAR_SAMPLES;
                M.bedLevel += (M.bedTarget - M.bedLevel) * 0.0006f;   // zipper-free fade
                v += M.grind[M.grindHead] * M.bedLevel;
                M.grindHead = (M.grindHead + 1) % BAR_SAMPLES;
            }
            chunk[i] = (short)(clampf(v, -1.0f, 1.0f) * 32000);
        }
        UpdateAudioStream(M.stream, chunk, CHUNK);
    }
}
