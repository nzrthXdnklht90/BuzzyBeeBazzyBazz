#include "HalfbandResampler.h"

namespace bassforge
{
namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= q / ((double) k * (double) k);
        sum += term;
        if (term < sum * 1.0e-17)
            break;
    }
    return sum;
}

double kaiserBeta (double A)
{
    if (A > 50.0) return 0.1102 * (A - 8.7);
    if (A >= 21.0) return 0.5842 * std::pow (A - 21.0, 0.4) + 0.07886 * (A - 21.0);
    return 0.0;
}
} // namespace

std::vector<double> designHalfbandOddTaps (const HalfbandSpec& spec, int& outM)
{
    const double fp = std::min (spec.passbandEdge, 0.2499);
    const double transition = 2.0 * (0.25 - fp); // normalized to high rate (cycles/sample)
    const double A = spec.attenuationDb;
    // Kaiser length estimate: N = (A - 8) / (2.285 * 2*pi*df) + 1
    int N = (int) std::ceil ((A - 8.0) / (2.285 * 2.0 * kPi * transition)) + 1;
    // halfband length 2M+1 with M odd
    int M = (N - 1) / 2;
    if (M < 1) M = 1;
    if ((M & 1) == 0) ++M;
    const double beta = kaiserBeta (A);
    const double i0b = besselI0 (beta);

    std::vector<double> taps ((size_t) M + 1);
    double sum = 0.0;
    for (int i = 0; i <= M; ++i)
    {
        const int n = 2 * i - M; // odd offset from centre
        const double sinc = std::sin (kPi * n / 2.0) / (kPi * n);
        const double r = (double) n / (double) M;
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        taps[(size_t) i] = sinc * w;
        sum += taps[(size_t) i];
    }
    // Normalize so DC gain of the full filter is exactly 1: 0.5 + sum(odd taps) = 1
    const double scale = 0.5 / sum;
    for (auto& t : taps)
        t *= scale;
    outM = M;
    return taps;
}

void FirDecimator2::prepare (const DecimatorSpec& spec)
{
    // design at the HIGH rate (2x output): normalized edges = fraction_of_output / 2
    const double fp = spec.passEdge / 2.0, fsb = spec.stopEdge / 2.0;
    const double df = fsb - fp, fc = 0.5 * (fp + fsb);
    const double A = spec.attenuationDb;
    int N = (int) std::ceil ((A - 8.0) / (2.285 * 2.0 * kPi * df)) + 1;
    if ((N & 1) == 0) ++N;          // odd length -> integer group delay
    if ((((N - 1) / 2) & 1) == 0) N += 2; // keep latency odd like the halfband stages
    numTaps = N;
    const double beta = kaiserBeta (A), i0b = besselI0 (beta);
    taps.assign ((size_t) N, 0.0);
    double sum = 0.0;
    const int c = (N - 1) / 2;
    for (int n = 0; n < N; ++n)
    {
        const int k = n - c;
        const double sinc = k == 0 ? 2.0 * fc : std::sin (2.0 * kPi * fc * k) / (kPi * k);
        const double r = (double) k / c;
        taps[(size_t) n] = sinc * besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        sum += taps[(size_t) n];
    }
    for (auto& t : taps) t /= sum; // unity DC gain
    hist.assign ((size_t) N * 2, 0.0);
    pos = 0;
}

void HalfbandStage::prepare (const HalfbandSpec& spec)
{
    taps = designHalfbandOddTaps (spec, M);
    numTaps = M + 1;
    histLen = M + 2;
    upHist.assign ((size_t) histLen * 2, 0.0);
    evenHist.assign ((size_t) histLen * 2, 0.0);
    oddHist.assign ((size_t) histLen * 2, 0.0);
    reset();
}

void HalfbandStage::reset() noexcept
{
    std::fill (upHist.begin(), upHist.end(), 0.0);
    std::fill (evenHist.begin(), evenHist.end(), 0.0);
    std::fill (oddHist.begin(), oddHist.end(), 0.0);
    upPos = dnPos = 0;
}

void OversamplerChannel::prepare (int numStages, const std::vector<HalfbandSpec>& specs, const DecimatorSpec& finalSpec, int maxBaseBlock)
{
    stages = numStages;
    factor = 1 << numStages;
    upStages.assign ((size_t) numStages, {});
    downStages.assign ((size_t) numStages, {});
    roundTripTop = 0;
    for (int s = 0; s < numStages; ++s)
    {
        upStages[(size_t) s].prepare (specs[(size_t) s]);
        downStages[(size_t) s].prepare (specs[(size_t) s]);
        // stage s (0-based) runs at high rate 2^(s+1); top rate is 2^S
        const int scale = 1 << (numStages - s - 1);
        const int downLat = s == 0 ? -1 : downStages[(size_t) s].getM();
        roundTripTop += upStages[(size_t) s].getM() * scale + (downLat >= 0 ? downLat * scale : 0);
    }
    if (numStages > 0)
    {
        finalDown.prepare (finalSpec);
        roundTripTop += finalDown.getLatencyHigh() * (1 << (numStages - 1));
    }
    scratchA.assign ((size_t) maxBaseBlock * (size_t) factor, 0.0);
    scratchB.assign ((size_t) maxBaseBlock * (size_t) factor, 0.0);
}

void OversamplerChannel::reset() noexcept
{
    for (auto& s : upStages) s.reset();
    for (auto& s : downStages) s.reset();
    finalDown.reset();
}

void OversamplerChannel::upsample (const double* in, double* out, int n) noexcept
{
    if (stages == 0)
    {
        std::copy (in, in + n, out);
        return;
    }
    const double* src = in;
    int len = n;
    for (int s = 0; s < stages; ++s)
    {
        double* dst = (s == stages - 1) ? out : ((s & 1) ? scratchB.data() : scratchA.data());
        auto& st = upStages[(size_t) s];
        for (int i = 0; i < len; ++i)
            st.up (src[i], dst[2 * i], dst[2 * i + 1]);
        src = dst;
        len *= 2;
    }
}

void OversamplerChannel::downsample (const double* in, double* out, int n) noexcept
{
    if (stages == 0)
    {
        std::copy (in, in + n, out);
        return;
    }
    const double* src = in;
    int len = n * factor;
    for (int s = stages - 1; s >= 0; --s)
    {
        double* dst = (s == 0) ? out : ((s & 1) ? scratchA.data() : scratchB.data());
        const int outLen = len / 2;
        if (s == 0)
        {
            for (int i = 0; i < outLen; ++i)
                dst[i] = finalDown.down (src[2 * i], src[2 * i + 1]);
        }
        else
        {
            auto& st = downStages[(size_t) s];
            for (int i = 0; i < outLen; ++i)
                dst[i] = st.down (src[2 * i], src[2 * i + 1]);
        }
        src = dst;
        len = outLen;
    }
}
} // namespace bassforge
