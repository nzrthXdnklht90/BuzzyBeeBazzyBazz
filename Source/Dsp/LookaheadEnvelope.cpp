#include "LookaheadEnvelope.h"

namespace bassforge
{
namespace
{
std::size_t pow2AtLeast (std::size_t v)
{
    std::size_t c = 1;
    while (c < v) c <<= 1;
    return c;
}
} // namespace

void LookaheadEnvelope::prepare (int maxAttack, int delay)
{
    maxA = std::max (1, maxAttack);
    D = std::max (delay, maxA);
    const std::size_t dqCap = pow2AtLeast ((std::size_t) maxA + 4);
    dqIdx.assign (dqCap, 0);
    dqVal.assign (dqCap, 1.0);
    dqMask = dqCap - 1;
    const std::size_t eCap = pow2AtLeast ((std::size_t) maxA + 4);
    eRing.assign (eCap, 1.0);
    eMask = eCap - 1;
    const std::size_t gCap = pow2AtLeast ((std::size_t) D + (std::size_t) maxA + 8);
    gRing.assign (gCap, 1.0);
    gMask = gCap - 1;
    if (relTable[1024] == 0.0 && relTable[0] == 0.0)
        setReleaseShape (4.0);
    A = attackTarget = std::clamp (attackTarget, 1, maxA);
    reset();
}

void LookaheadEnvelope::setReleaseShape (double k)
{
    const double ek = std::exp (-k);
    for (int i = 0; i <= 1024; ++i)
    {
        const double u = i / 1024.0;
        relTable[(size_t) i] = (std::exp (-k * u) - ek) / (1.0 - ek);
    }
    relTable[0] = 1.0;
    relTable[1024] = 0.0;
}

void LookaheadEnvelope::reset() noexcept
{
    A = attackTarget;
    n = -1;
    written = -1;
    dqHead = dqTail = 0;
    relActive = false;
    relStart = 1.0;
    relPos = relLen = 0;
    std::fill (eRing.begin(), eRing.end(), 1.0);
    sum = (double) A;
    sumComp = 0.0;
    std::fill (gRing.begin(), gRing.end(), 1.0);
}

double LookaheadEnvelope::releaseCurve (double u) const noexcept
{
    if (u >= 1.0) return 0.0;
    const double x = u * 1024.0;
    const int i = (int) x;
    const double f = x - i;
    return relTable[(size_t) i] + f * (relTable[(size_t) i + 1] - relTable[(size_t) i]);
}

double LookaheadEnvelope::push (double r) noexcept
{
    ++n;
    if (! (r > 0.0)) r = 1.0e-12;     // also catches NaN
    if (r > 1.0) r = 1.0;

    const int Aold = A;
    if (A < attackTarget) ++A;
    else if (A > attackTarget) --A;
    const int H = A + 1;

    // --- moving minimum over [n-H+1, n]
    while (dqTail != dqHead && dqVal[(dqTail - 1) & dqMask] >= r)
        --dqTail;
    dqIdx[dqTail & dqMask] = n;
    dqVal[dqTail & dqMask] = r;
    ++dqTail;
    const int64_t windowStart = n - H + 1;
    while (dqIdx[dqHead & dqMask] < windowStart)
        ++dqHead;
    const double h = dqVal[dqHead & dqMask];

    // --- finite release segment; e <= h always
    double e;
    if (releaseSamples <= 0 && ! relActive)
    {
        e = h;
    }
    else
    {
        double rel = 1.0;
        if (relActive)
        {
            ++relPos;
            if (relPos >= relLen)
                relActive = false;
            else
                rel = 1.0 - (1.0 - relStart) * releaseCurve ((double) relPos / (double) relLen);
        }
        if (h <= rel)
        {
            e = h;
            relStart = h;
            relPos = 0;
            relLen = releaseSamples;
            relActive = (h < 1.0) && relLen > 0;
        }
        else
        {
            e = rel;
        }
    }

    // --- box average of length A with exact add/remove on length change (Kahan compensated)
    auto kahanAdd = [this] (double v) noexcept
    {
        const double y = v - sumComp;
        const double t = sum + y;
        sumComp = (t - sum) - y;
        sum = t;
    };
    eRing[(std::size_t) n & eMask] = e;
    kahanAdd (e);
    // previous window [n-Aold, n-1]; new window [n-A+1, n]; remove [n-Aold, n-A]
    for (int64_t k = n - Aold; k <= n - A; ++k)
        kahanAdd (-eRing[(std::size_t) k & eMask]);
    double g = sum / (double) A;
    if (g > 1.0) g = 1.0;

    // --- write to target ring: targets n-A and n-A+1
    for (int64_t t = n - A; t <= n - A + 1; ++t)
    {
        auto& slot = gRing[(std::size_t) t & gMask];
        if (t > written)
            slot = g;
        else if (g < slot)
            slot = g;
    }
    written = std::max (written, n - A + 1);

    const int64_t target = n - D;
    if (target < 0)
        return 1.0;
    return gRing[(std::size_t) target & gMask];
}
} // namespace bassforge
