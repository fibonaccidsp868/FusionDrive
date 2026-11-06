// FusionDsp.h - JUCE-independent DSP core for FusionDrive.
// Two saturation engines:
//   1) "Vintage" : tone-shaped pre/de-emphasis around an asymmetric generalized soft clipper
//   2) "Analog"  : flux-pre-emphasis -> Jiles-Atherton input transformer -> leakage/winding LPF
//                  -> class-A asymmetric stage with PSU sag -> slew-limited op-amp stage
//                  -> Jiles-Atherton output transformer -> flux de-emphasis -> coloration -> DC servo
// NOTE: behavioural model; not a circuit-level clone of any commercial unit.
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace fd {

constexpr double kPi = 3.141592653589793238462643383279502884;

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static double clampF (double fs, double f) { return std::min (std::max (f, 5.0), fs * 0.45); }

    void lowShelf (double fs, double f0, double gainDb, double S = 0.8)
    {
        f0 = clampF (fs, f0);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), q = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + q), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - q),
             (A + 1) + (A - 1) * c + q, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - q);
    }
    void highShelf (double fs, double f0, double gainDb, double S = 0.8)
    {
        f0 = clampF (fs, f0);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f0 / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), q = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + q), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - q),
             (A + 1) - (A - 1) * c + q, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - q);
    }
    void lowPass (double fs, double f0, double Q)
    {
        f0 = clampF (fs, f0);
        const double w = 2 * kPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

struct DCBlocker
{
    double x1 = 0, y1 = 0, R = 0.999;
    void prepare (double fs, double fc = 6.0) { R = 1.0 - 2.0 * kPi * fc / fs; }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) { const double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

struct OnePole
{
    double z = 0, a = 0;
    void setTime (double ms, double fs) { a = std::exp (-1.0 / (0.001 * ms * fs)); }
    inline double process (double t) { z = t + a * (z - t); return z; }
    void reset (double v = 0) { z = v; }
};

//==============================================================================
// Generalized, optionally biased soft clipper: f(x)=x/(1+|x|^p)^(1/p). Bias adds even harmonics.
struct SoftClip
{
    static inline double f (double x, double p)  { return x / std::pow (1.0 + std::pow (std::abs (x), p), 1.0 / p); }
    static inline double df (double x, double p) { return std::pow (1.0 + std::pow (std::abs (x), p), -1.0 / p - 1.0); }
    static inline double process (double x, double bias, double p)
    {
        if (bias < 1.0e-6) return f (x, p);
        return (f (x + bias, p) - f (bias, p)) / df (bias, p);   // zero in -> zero out, unity small-signal gain
    }
};

//==============================================================================
// Jiles-Atherton ferromagnetic hysteresis (normalised units), RK4 integration.
struct JilesAtherton
{
    double Ms = 1.0, a = 0.35, k = 0.08, c = 0.6, alpha = 0.01;
    double M = 0, Hprev = 0;
    double norm = 1.0;   // small-signal normalisation (output = M * norm)

    static inline double langevin (double q)
    {
        if (std::abs (q) < 1.0e-3) return q / 3.0 - q * q * q / 45.0;
        q = std::max (-300.0, std::min (300.0, q));
        return 1.0 / std::tanh (q) - 1.0 / q;
    }
    static inline double dLangevin (double q)
    {
        if (std::abs (q) < 1.0e-3) return 1.0 / 3.0 - q * q / 15.0;
        q = std::max (-300.0, std::min (300.0, q));
        const double s = std::sinh (q);
        return 1.0 / (q * q) - 1.0 / (s * s);
    }
    inline double dMdH (double H, double m, double dir) const
    {
        const double He = H + alpha * m, q = He / a;
        const double Man = Ms * langevin (q), dMan = Ms / a * dLangevin (q);
        const double Mirr = (m - c * Man) / (1.0 - c);
        const double delta = dir >= 0 ? 1.0 : -1.0, diff = Man - Mirr;
        const double dMirr = (delta * diff > 0) ? diff / (k * delta) : 0.0;
        const double A = c * dMan + (1.0 - c) * dMirr;
        return A / std::max (1.0 - alpha * A, 0.05);
    }
    inline double step (double H)
    {
        const double dH = H - Hprev;
        if (dH != 0.0)
        {
            const double k1 = dMdH (Hprev, M, dH) * dH;
            const double k2 = dMdH (Hprev + 0.5 * dH, M + 0.5 * k1, dH) * dH;
            const double k3 = dMdH (Hprev + 0.5 * dH, M + 0.5 * k2, dH) * dH;
            const double k4 = dMdH (H, M + k3, dH) * dH;
            M += (k1 + 2 * k2 + 2 * k3 + k4) / 6.0;
            if (! std::isfinite (M)) M = 0;
            M = std::max (-Ms, std::min (Ms, M));
        }
        Hprev = H;
        return M;
    }
    void reset() { M = 0; Hprev = 0; }

    // Measure effective gain for a moderate-amplitude sine (rate independent) and set norm = 1/gain.
    void calibrate (double Hamp = 0.04)
    {
        reset();
        double sumH = 0, sumM = 0;
        const int per = 192, cycles = 8;
        for (int n = 0; n < per * cycles; ++n)
        {
            const double h = Hamp * std::sin (2 * kPi * n / per), m = step (h);
            if (n >= per * (cycles - 3)) { sumH += h * h; sumM += m * m; }
        }
        norm = 1.0 / std::max (std::sqrt (sumM / sumH), 1.0e-6);
        reset();
    }
};

//==============================================================================
struct Params
{
    double driveDb   = 6.0;   // 0..30
    double tone      = 0.0;   // -1..1   (which band saturates)
    double character = 0.3;   // 0..1    (odd <-> even harmonic balance)
    double knee      = 0.35;  // 0..1    (soft -> hard knee)
    bool   analog    = false;
    double iron      = 0.5;   // 0..1    transformer core drive
    double warmth    = 0.5;   // 0..1    linear transformer/passive coloration
    double sag       = 0.3;   // 0..1    power-supply sag
    double slew      = 0.3;   // 0..1    op-amp slew limiting
    double drift     = 0.3;   // 0..1    component tolerance (L/R mismatch)
};

class Channel
{
public:
    void prepare (double sampleRate, int seed)
    {
        fs = sampleRate;
        // deterministic per-channel "component tolerance"
        uint32_t s = 0x9E3779B9u * (uint32_t) (seed + 1);
        auto rnd = [&s] { s = s * 1664525u + 1013904223u; return ((s >> 8) & 0xFFFF) / 32768.0 - 1.0; };
        tolGain = rnd(); tolFreq = rnd(); tolBias = rnd();
        jaIn  = JilesAtherton { 1.0, 0.35, 0.08, 0.60, 0.010 };
        jaOut = JilesAtherton { 1.0, 0.60, 0.05, 0.75, 0.005 };
        jaIn.calibrate(); jaOut.calibrate();
        dcA.prepare (fs); dcB.prepare (fs);
        driveSm.setTime (15.0, fs);
        envSm.setTime (60.0, fs);
        reset();
        lastKey = -1e9;
    }
    void reset()
    {
        for (auto* b : { &preHi, &preLo, &postHi, &postLo, &fluxPre, &fluxPost, &leak, &warmLo, &warmHi }) b->reset();
        dcA.reset(); dcB.reset(); jaIn.reset(); jaOut.reset(); slewState = 0; env = 0; envSm.reset(); driveSm.reset (1.0);
    }
    void setParams (const Params& np)
    {
        p = np;
        driveTarget = std::pow (10.0, p.driveDb / 20.0) * (1.0 + 0.01 * p.drift * tolGain);
        const double key = p.tone * 7 + p.iron * 13 + p.warmth * 17 + p.drift * 19 + p.slew * 23 + p.driveDb * 0.001;
        if (key == lastKey) return;
        lastKey = key;

        // Vintage engine: tone-weighted pre/de-emphasis
        const double gHi = std::max (p.tone, 0.0) * 10.0, gLo = std::max (-p.tone, 0.0) * 10.0;
        preHi.highShelf (fs, 2500, gHi);   postHi.highShelf (fs, 2500, -gHi);
        preLo.lowShelf  (fs, 250,  gLo);   postLo.lowShelf  (fs, 250,  -gLo);

        // Analog engine
        const double fTol = 1.0 + 0.03 * p.drift * tolFreq;
        const double fluxDb = 3.0 + 15.0 * p.iron;                  // flux ~ integral(V): LF saturates first
        fluxPre.lowShelf  (fs, 110 * fTol,  fluxDb);
        fluxPost.lowShelf (fs, 110 * fTol, -fluxDb);
        leak.lowPass (fs, 42000.0 * fTol, 0.75);                    // leakage L / winding C
        warmLo.lowShelf  (fs, 120 * fTol,  2.5 * p.warmth);          // primary inductance bloom
        warmHi.highShelf (fs, 9000 * fTol, -2.0 * p.warmth);         // gentle HF loss
        hs1   = 0.03 + 0.45 * p.iron;
        hsOut = 0.02 + 0.30 * p.iron;
        maxStep = 2 * kPi * (120000.0 * std::pow (0.1, p.slew)) / fs;
    }

    inline double process (double in)
    {
        const double d = driveSm.process (driveTarget);
        const double x = in * d;
        double y;
        if (! p.analog)
        {
            const double pk = 1.5 + 6.0 * p.knee, bias = p.character * 0.6;
            y = preHi.process (x); y = preLo.process (y);
            y = SoftClip::process (y, bias, pk);
            y = postLo.process (y); y = postHi.process (y);
            return dcA.process (y);
        }
        // ---- Analog engine ----
        double f = fluxPre.process (x);
        double t = jaIn.step (f * hs1) * jaIn.norm / hs1;            // input transformer (unity small-signal)
        t = leak.process (t);
        env = envSm.process (std::abs (t));
        const double rail = 1.0 - p.sag * 0.35 * std::min (env, 1.5); // PSU sag lowers headroom
        const double bias = 0.03 + 0.20 * p.character + 0.02 * p.drift * tolBias;
        double s = SoftClip::process (t * rail, bias, 2.4) / rail;    // class-A stage
        const double d2 = s - slewState;                              // op-amp slew limiting
        slewState += maxStep * std::tanh (d2 / maxStep);
        double u = jaOut.step (slewState * hsOut) * jaOut.norm / hsOut; // output transformer
        u = fluxPost.process (u);
        u = warmLo.process (u); u = warmHi.process (u);
        return dcB.process (u);
    }
private:
    double fs = 48000; Params p;
    double tolGain = 0, tolFreq = 0, tolBias = 0, lastKey = -1e9;
    double driveTarget = 1, hs1 = 0.3, hsOut = 0.2, maxStep = 1, slewState = 0, env = 0;
    Biquad preHi, preLo, postHi, postLo, fluxPre, fluxPost, leak, warmLo, warmHi;
    DCBlocker dcA, dcB; OnePole driveSm, envSm; JilesAtherton jaIn, jaOut;
};

} // namespace fd
