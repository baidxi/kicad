/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <r_divider_solver.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>

#include <boost/multiprecision/cpp_int.hpp>
#include <fmt/format.h>

// All the arithmetic is done on exact fractions so that candidates which are equally good really
// do compare equal, and the choice between them is left to the tiebreakers rather than to
// floating point round-off.
using RATIONAL = boost::multiprecision::cpp_rational;
using BIG_INT = boost::multiprecision::cpp_int;


// Impedance windows of solutions 2 and 3: the larger of a fixed tolerance and a number of series
// steps either side of the target.
static const RATIONAL LOOSE_WINDOW_TOLERANCE( 3, 10 );
static const int      LOOSE_WINDOW_STEPS = 3;
static const RATIONAL TIGHT_WINDOW_TOLERANCE( 1, 10 );
static const int      TIGHT_WINDOW_STEPS = 2;

// Scoring of solutions 4 and 5, in points. Lower is better.
static const RATIONAL RATIO_ERROR_POINTS( 1 );         // per percent of ratio error
static const RATIONAL IMPEDANCE_FREE_PERCENT( 1 );     // impedance error that costs nothing
static const RATIONAL LIGHT_IMPEDANCE_POINTS( 1, 10 ); // per percent of impedance error, solution 4
static const RATIONAL HEAVY_IMPEDANCE_POINTS( 1, 2 );  // per percent of impedance error, solution 5
static const RATIONAL REUSED_RESISTOR_POINTS( 3 );     // per resistor shared with solutions 1 to 3

// Three resistor dividers: the share of the bottom leg that R2 may take, leaving the rest to R3.
// It's silly to allow values of wildly different magnitudes to be used, because the precision
// of the small resistor will be swamped by the error of the large resistor
static const RATIONAL SERIES_LEG_SHARE_MIN( 5, 100 );
static const RATIONAL SERIES_LEG_SHARE_MAX( 95, 100 );

// The same for two resistors in parallel, where it is the conductance of the leg that they share
static const RATIONAL PARALLEL_LEG_SHARE_MIN( 5, 100 );
static const RATIONAL PARALLEL_LEG_SHARE_MAX( 95, 100 );

// Impedance error allowed for the last of the three resistor solutions
static const RATIONAL EXACT_WINDOW_TOLERANCE( 1, 100 );


struct DIVIDER_CANDIDATE
{
    size_t   r1;             // index into the list of R1 values
    size_t   r2;             // index into the list of R2 values
    RATIONAL impedance;      // R1 + R2 or R1 || R2, whichever the target applies to
    RATIONAL ratioError;     // relative error of Vout / Vin, or of R1 / R2 if that is the target
    RATIONAL impedanceError; // relative error of the impedance
};


struct THREE_RESISTOR_CANDIDATE
{
    bool     splitTop;       // the leg made of two resistors is the top one rather than the bottom
    bool     parallel;       // those two resistors are in parallel rather than in series
    RATIONAL single;         // the leg made of one resistor
    RATIONAL swept;          // resistor of the split leg that every value was tried for
    RATIONAL fitted;         // resistor of the split leg chosen to make up the rest of it
    RATIONAL impedance;      // sum of the two legs
    RATIONAL ratioError;     // relative error of Vout / Vin, or of R1 / R2 if that is the target
    RATIONAL impedanceError; // relative error of the impedance
};


static RATIONAL powerOfTen( int aExponent )
{
    BIG_INT power = boost::multiprecision::pow( BIG_INT( 10 ), std::abs( aExponent ) );

    return aExponent >= 0 ? RATIONAL( power ) : RATIONAL( 1, power );
}


/**
 * Convert a positive double to the decimal fraction it was most likely entered as.
 *
 * 15 significant digits are always preserved by a double, so 1.2 becomes 6/5 rather than the
 * binary fraction closest to it.
 */
static RATIONAL toRational( double aValue )
{
    std::string text = fmt::format( "{:.14e}", aValue );
    size_t      exponentPos = text.find( 'e' );
    std::string digits = text.substr( 0, exponentPos );

    digits.erase( std::remove( digits.begin(), digits.end(), '.' ), digits.end() );

    return RATIONAL( BIG_INT( digits ) ) * powerOfTen( std::stoi( text.substr( exponentPos + 1 ) ) - 14 );
}


/**
 * The largest ratio between two neighbouring values of a series, including the step from the last
 * value of one decade to the first of the next.
 *
 * The nominal step of a series with N values is 10^(1/N), but the values are rounded, so the
 * largest step is somewhat bigger: 15/13 rather than 1.1 for E24.
 */
static RATIONAL largestStep( const std::vector<uint16_t>& aSeries )
{
    RATIONAL step( 10 * aSeries.front(), aSeries.back() );

    for( size_t i = 1; i < aSeries.size(); ++i )
        step = std::max( step, RATIONAL( aSeries[i], aSeries[i - 1] ) );

    return step;
}


/**
 * How far either side of its ideal value a resistor is searched for, as a multiplier.
 *
 * These values are multiplicative. For example, if the center of our search space is a 1k, a span of 5 means we
 * include resistors between 200 and 5k in our search.
 *
 * Sparse series need a wide search to find a good ratio at all, dense ones are narrowed to keep
 * the number of combinations sensible.
 */
static int searchSpan( size_t aSeriesSize )
{
    if( aSeriesSize <= 3 )
        return 15;
    else if( aSeriesSize <= 48 )
        return 10;
    else if( aSeriesSize <= 96 )
        return 5;
    else // E192
        return 4;
}


/**
 * List the resistors of a series, over as many decades as needed, that lie strictly between
 * \a aMin and \a aMax.
 */
static std::vector<RATIONAL> resistorsBetween( const std::vector<uint16_t>& aSeries, const RATIONAL& aMin,
                                               const RATIONAL& aMax )
{
    std::vector<RATIONAL> resistors;

    // Series values are 100 to 999, so three decades below aMin every resistor is too small
    int decade = static_cast<int>( std::floor( std::log10( aMin.convert_to<double>() ) ) ) - 3;

    for( RATIONAL scale = powerOfTen( decade ); 100 * scale < aMax; scale *= 10 )
    {
        for( uint16_t value : aSeries )
        {
            RATIONAL resistor = value * scale;

            if( resistor > aMin && resistor < aMax )
                resistors.push_back( resistor );
        }
    }

    return resistors;
}


/**
 * Return the acceptable candidate with the lowest score, or nullptr if none is acceptable.
 *
 * Equal scores are settled by the impedance error, then by the order of the candidates.
 */
template <typename T, typename ACCEPT, typename SCORE>
static const T* pickBest( const std::vector<T>& aCandidates, ACCEPT aAccept, SCORE aScore )
{
    const T* best = nullptr;
    RATIONAL bestScore;

    for( const T& candidate : aCandidates )
    {
        if( !aAccept( candidate ) )
            continue;

        RATIONAL score = aScore( candidate );

        if( !best || score < bestScore || ( score == bestScore && candidate.impedanceError < best->impedanceError ) )
        {
            best = &candidate;
            bestScore = score;
        }
    }

    return best;
}


/**
 * Return the resistor of a series, in any decade, closest to \a aTarget. The smaller one wins if
 * two are equally close.
 *
 * @param aByConductance measures the distance between conductances rather than resistances, which
 *                       is what counts for a resistor that goes in parallel with another.
 */
static RATIONAL nearestResistor( const std::vector<uint16_t>& aSeries, const RATIONAL& aTarget,
                                 bool aByConductance = false )
{
    // Scale the series so that its decade holds the target, give or take the rounding of log10
    int      decade = static_cast<int>( std::floor( std::log10( aTarget.convert_to<double>() ) ) ) - 2;
    RATIONAL scale = powerOfTen( decade );
    RATIONAL scaledTarget = aTarget / scale;

    auto above = std::lower_bound( aSeries.begin(), aSeries.end(), scaledTarget,
                                   []( uint16_t aValue, const RATIONAL& aScaledTarget )
                                   {
                                       return aValue < aScaledTarget;
                                   } );

    // The neighbours of the target, borrowed from the next decade when it is beyond either end
    RATIONAL higher = above == aSeries.end() ? RATIONAL( 10 * aSeries.front() ) : RATIONAL( *above );
    RATIONAL lower = above == aSeries.begin() ? RATIONAL( aSeries.back(), 10 ) : RATIONAL( *( above - 1 ) );

    bool higherIsCloser = aByConductance ? 1 / scaledTarget - 1 / higher < 1 / lower - 1 / scaledTarget
                                         : higher - scaledTarget < scaledTarget - lower;

    return ( higherIsCloser ? higher : lower ) * scale;
}


// The search is done in two stages to keep the exact arithmetic affordable. Every candidate is
// first evaluated in floating point, which is only used to throw out the ones that cannot win.
// Whatever is within APPROX_MARGIN of winning, or of the edge of an impedance window, is then
// evaluated and compared exactly. The margin is far larger than the round-off of the floating
// point evaluation, so the result is the one the exact arithmetic alone would give.
static const double APPROX_MARGIN = 1e-9;
static const double APPROX_ERROR_FLOOR = 1e-12; // for errors that are close to zero


/// 10^aExponent, from a table as std::pow() is too slow for the inner loops
static double approxPowerOfTen( int aExponent )
{
    static const int MAX_EXPONENT = 330;

    static const std::vector<double> powers =
            []()
            {
                std::vector<double> table;

                for( int exponent = -MAX_EXPONENT; exponent <= MAX_EXPONENT; ++exponent )
                    table.push_back( std::pow( 10.0, exponent ) );

                return table;
            }();

    if( aExponent < -MAX_EXPONENT || aExponent > MAX_EXPONENT )
        return std::pow( 10.0, aExponent );

    return powers[aExponent + MAX_EXPONENT];
}


/// A standard resistor, value * 10^decade ohms
struct STANDARD_RESISTOR
{
    uint16_t value;
    int      decade;

    double   Approx() const { return value * approxPowerOfTen( decade ); }
    RATIONAL Exact() const { return value * powerOfTen( decade ); }
};


/**
 * Same as resistorsBetween(), with the exact comparisons kept for the values next to the limits.
 */
static void standardResistorsBetween( const std::vector<uint16_t>& aSeries, const RATIONAL& aMin, const RATIONAL& aMax,
                                      std::vector<STANDARD_RESISTOR>& aResistors )
{
    const double min = aMin.convert_to<double>();
    const double max = aMax.convert_to<double>();

    aResistors.clear();

    for( int decade = static_cast<int>( std::floor( std::log10( min ) ) ) - 3;; ++decade )
    {
        const double scale = approxPowerOfTen( decade );

        if( 100 * scale > max * ( 1 + APPROX_MARGIN ) )
            break;

        if( 1000 * scale < min * ( 1 - APPROX_MARGIN ) )
            continue;

        for( uint16_t value : aSeries )
        {
            const STANDARD_RESISTOR resistor{ value, decade };
            const double            approx = value * scale;

            if( approx < min * ( 1 - APPROX_MARGIN ) || approx > max * ( 1 + APPROX_MARGIN ) )
                continue;

            if( approx <= min * ( 1 + APPROX_MARGIN ) && resistor.Exact() <= aMin )
                continue;

            if( approx >= max * ( 1 - APPROX_MARGIN ) && resistor.Exact() >= aMax )
                continue;

            aResistors.push_back( resistor );
        }
    }
}


static STANDARD_RESISTOR toStandardResistor( const RATIONAL& aResistor )
{
    const double approx = aResistor.convert_to<double>();
    int          decade = static_cast<int>( std::floor( std::log10( approx ) ) ) - 2;
    long         value = std::lround( approx / approxPowerOfTen( decade ) );

    if( value >= 1000 )
    {
        value /= 10;
        ++decade;
    }
    else if( value < 100 )
    {
        value *= 10;
        --decade;
    }

    return { static_cast<uint16_t>( value ), decade };
}


/**
 * Same as nearestResistor() for an approximate target. Returns false when the two neighbours of
 * the target are too nearly as close as each other to tell which one wins.
 */
static bool nearestStandardResistor( const std::vector<uint16_t>& aSeries, double aTarget, bool aByConductance,
                                     STANDARD_RESISTOR& aResistor )
{
    const int    decade = static_cast<int>( std::floor( std::log10( aTarget ) ) ) - 2;
    const double scaledTarget = aTarget / approxPowerOfTen( decade );

    auto above = std::lower_bound( aSeries.begin(), aSeries.end(), scaledTarget,
                                   []( uint16_t aValue, double aScaledTarget )
                                   {
                                       return aValue < aScaledTarget;
                                   } );

    const STANDARD_RESISTOR higher = above == aSeries.end() ? STANDARD_RESISTOR{ aSeries.front(), decade + 1 }
                                                            : STANDARD_RESISTOR{ *above, decade };
    const STANDARD_RESISTOR lower = above == aSeries.begin() ? STANDARD_RESISTOR{ aSeries.back(), decade - 1 }
                                                             : STANDARD_RESISTOR{ *( above - 1 ), decade };

    const double higherValue = above == aSeries.end() ? 10.0 * aSeries.front() : *above;
    const double lowerValue = above == aSeries.begin() ? aSeries.back() / 10.0 : *( above - 1 );

    const double toHigher = aByConductance ? 1 / scaledTarget - 1 / higherValue : higherValue - scaledTarget;
    const double toLower = aByConductance ? 1 / lowerValue - 1 / scaledTarget : scaledTarget - lowerValue;

    if( std::abs( toHigher - toLower ) <= APPROX_MARGIN * ( std::abs( toHigher ) + std::abs( toLower ) ) )
        return false;

    aResistor = toHigher < toLower ? higher : lower;
    return true;
}


/**
 * Accepts candidates whose impedance is within \a aTolerance or \a aSteps series steps of the
 * target, whichever reaches further.
 */
struct IMPEDANCE_WINDOW
{
    IMPEDANCE_WINDOW( const RATIONAL& aTarget, const RATIONAL& aStep, const RATIONAL& aTolerance, int aSteps )
    {
        RATIONAL factor = 1;

        for( int i = 0; i < aSteps; ++i )
            factor *= aStep;

        lowest = std::min<RATIONAL>( aTarget * ( 1 - aTolerance ), aTarget / factor );
        highest = std::max<RATIONAL>( aTarget * ( 1 + aTolerance ), aTarget * factor );
        lowestApprox = lowest.convert_to<double>();
        highestApprox = highest.convert_to<double>();
    }

    template <typename T>
    bool operator()( const T& aCandidate ) const
    {
        return aCandidate.impedance >= lowest && aCandidate.impedance <= highest;
    }

    /// 1 if an approximate impedance is inside, 0 if it is outside, -1 if it is too close to call
    int Classify( double aImpedance ) const
    {
        if( aImpedance > lowestApprox * ( 1 + APPROX_MARGIN ) && aImpedance < highestApprox * ( 1 - APPROX_MARGIN ) )
            return 1;

        if( aImpedance < lowestApprox * ( 1 - APPROX_MARGIN ) || aImpedance > highestApprox * ( 1 + APPROX_MARGIN ) )
            return 0;

        return -1;
    }

    RATIONAL lowest;
    RATIONAL highest;
    double   lowestApprox;
    double   highestApprox;
};


/**
 * List the candidates that could have the lowest score among the acceptable ones, in their
 * original order.
 *
 * @param aClassify returns 1 for acceptable candidates, 0 for unacceptable ones and -1 if unsure.
 * @param aScore returns the approximate score of a candidate.
 */
template <typename T, typename CLASSIFY, typename SCORE>
static std::vector<size_t> shortlist( const std::vector<T>& aCandidates, CLASSIFY aClassify, SCORE aScore )
{
    std::vector<double> scores( aCandidates.size() );
    std::vector<int8_t> classes( aCandidates.size() );
    double              best = std::numeric_limits<double>::infinity();

    for( size_t i = 0; i < aCandidates.size(); ++i )
    {
        classes[i] = static_cast<int8_t>( aClassify( aCandidates[i] ) );

        if( classes[i] == 0 )
            continue;

        scores[i] = aScore( aCandidates[i] );

        if( classes[i] == 1 )
            best = std::min( best, scores[i] );
    }

    const double        limit = best * ( 1 + APPROX_MARGIN ) + APPROX_ERROR_FLOOR;
    std::vector<size_t> kept;

    for( size_t i = 0; i < aCandidates.size(); ++i )
    {
        if( classes[i] != 0 && scores[i] <= limit )
            kept.push_back( i );
    }

    return kept;
}


static bool isUsableInput( double aVin, double aVout, double aImpedance, const std::vector<uint16_t>& aSeries )
{
    // Comparisons with NaN are false, so NaN is rejected too
    return aVout > 0 && aVout < aVin && aImpedance > 0 && std::isfinite( aVin ) && std::isfinite( aImpedance )
           && !aSeries.empty();
}


struct APPROX_CANDIDATE
{
    uint32_t r1;
    uint32_t r2;
    double   impedance;
    double   ratioError;
    double   impedanceError;
};


std::vector<R_DIVIDER_SOLUTION> SolveResistorDivider( double aVin, double aVout, double aImpedance,
                                                      R_DIVIDER_IMPEDANCE          aImpedanceType,
                                                      const std::vector<uint16_t>& aSeries, R_DIVIDER_TARGET aTarget )
{
    if( !isUsableInput( aVin, aVout, aImpedance, aSeries ) )
        return {};

    // --------- Step 1: Approximate evaluation of every divider --------

    const RATIONAL targetRatio = toRational( aVout ) / toRational( aVin ); // Vout / Vin

    // toRational() rounds, so Vout just below Vin can still give a ratio of exactly 1
    if( targetRatio <= 0 || targetRatio >= 1 )
        return {};

    // The ratio error is measured on what the user asked for: Vout / Vin, or R1 / R2
    const bool     byResistorRatio = aTarget == R_DIVIDER_TARGET::RESISTOR_RATIO;
    const RATIONAL targetResistorRatio = ( 1 - targetRatio ) / targetRatio;
    const double   targetResistorRatioApprox = targetResistorRatio.convert_to<double>();

    const RATIONAL targetImpedance = toRational( aImpedance );
    const RATIONAL step = largestStep( aSeries );
    const int      span = searchSpan( aSeries.size() );

    const bool parallel = aImpedanceType == R_DIVIDER_IMPEDANCE::PARALLEL;

    // With R1 + R2 = Z the ideal resistors are R2 = r * Z and R1 = ( 1 - r ) * Z. With
    // R1 || R2 = Z they are R1 = Z / r and R2 = Z / ( 1 - r ). Look for each one within the
    // search span of its ideal value.
    //
    // This treats R1 and R2 alike, and the errors below are measured on the quantity the user
    // sees, so dividers below and above one half need no separate handling.
    const RATIONAL idealR1 = parallel ? RATIONAL( targetImpedance / targetRatio )
                                      : RATIONAL( ( 1 - targetRatio ) * targetImpedance );
    const RATIONAL idealR2 = parallel ? RATIONAL( targetImpedance / ( 1 - targetRatio ) )
                                      : RATIONAL( targetRatio * targetImpedance );

    const std::vector<RATIONAL> r1Values = resistorsBetween( aSeries, idealR1 / span, idealR1 * span );
    const std::vector<RATIONAL> r2Values = resistorsBetween( aSeries, idealR2 / span, idealR2 * span );

    std::vector<double> r1Approx;
    std::vector<double> r2Approx;

    for( const RATIONAL& r1 : r1Values )
        r1Approx.push_back( r1.convert_to<double>() );

    for( const RATIONAL& r2 : r2Values )
        r2Approx.push_back( r2.convert_to<double>() );

    const double targetRatioApprox = targetRatio.convert_to<double>();
    const double targetImpedanceApprox = targetImpedance.convert_to<double>();

    std::vector<APPROX_CANDIDATE> candidates;
    candidates.reserve( r1Values.size() * r2Values.size() );

    for( size_t r1 = 0; r1 < r1Values.size(); ++r1 )
    {
        for( size_t r2 = 0; r2 < r2Values.size(); ++r2 )
        {
            APPROX_CANDIDATE candidate;

            candidate.r1 = static_cast<uint32_t>( r1 );
            candidate.r2 = static_cast<uint32_t>( r2 );
            const double total = r1Approx[r1] + r2Approx[r2];

            candidate.impedance = parallel ? r1Approx[r1] * r2Approx[r2] / total : total;
            candidate.ratioError =
                    byResistorRatio ? std::abs( r1Approx[r1] / r2Approx[r2] - targetResistorRatioApprox )
                                              / targetResistorRatioApprox
                                    : std::abs( r2Approx[r2] / total - targetRatioApprox ) / targetRatioApprox;
            candidate.impedanceError = std::abs( candidate.impedance - targetImpedanceApprox ) / targetImpedanceApprox;

            candidates.push_back( candidate );
        }
    }

    // ---------- Step 2: Exact evaluation of shortlisted dividers ---------

    auto exactCandidates =
            [&]( const std::vector<size_t>& aShortlist )
            {
                std::vector<DIVIDER_CANDIDATE> exact;

                for( size_t i : aShortlist )
                {
                    DIVIDER_CANDIDATE candidate;

                    candidate.r1 = candidates[i].r1;
                    candidate.r2 = candidates[i].r2;

                    const RATIONAL& r1 = r1Values[candidate.r1];
                    const RATIONAL& r2 = r2Values[candidate.r2];
                    const RATIONAL  total = r1 + r2;

                    candidate.impedance = parallel ? RATIONAL( r1 * r2 / total ) : total;
                    candidate.ratioError =
                            byResistorRatio ? RATIONAL( abs( r1 / r2 - targetResistorRatio ) / targetResistorRatio )
                                            : RATIONAL( abs( r2 / total - targetRatio ) / targetRatio );
                    candidate.impedanceError = abs( candidate.impedance - targetImpedance ) / targetImpedance;

                    exact.push_back( std::move( candidate ) );
                }

                return exact;
            };

    // Best of the shortlisted candidates, or nothing
    auto pick =
            [&]( std::vector<DIVIDER_CANDIDATE>& aPicks, const std::vector<size_t>& aShortlist, auto aAccept,
                 auto aScore )
            {
                const std::vector<DIVIDER_CANDIDATE> exact = exactCandidates( aShortlist );
                const DIVIDER_CANDIDATE*             best = pickBest( exact, aAccept, aScore );

                DIVIDER_CANDIDATE none;

                none.r1 = SIZE_MAX;
                none.r2 = SIZE_MAX;

                aPicks.push_back( best ? *best : none );
            };

    // ---------- Step 3: Search for the best ratio ---------

    std::vector<DIVIDER_CANDIDATE> picks; // r1 is SIZE_MAX where nothing was found

    auto anyImpedance =
            []( const DIVIDER_CANDIDATE& )
            {
                return true;
            };

    auto anyImpedanceApprox =
            []( const APPROX_CANDIDATE& )
            {
                return 1;
            };

    auto ratioError =
            []( const DIVIDER_CANDIDATE& aCandidate )
            {
                return aCandidate.ratioError;
            };

    auto ratioErrorApprox =
            []( const APPROX_CANDIDATE& aCandidate )
            {
                return aCandidate.ratioError;
            };

    // Solutions 1 to 3: the best ratio, with the impedance progressively reined in
    const IMPEDANCE_WINDOW looseImpedance( targetImpedance, step, LOOSE_WINDOW_TOLERANCE, LOOSE_WINDOW_STEPS );
    const IMPEDANCE_WINDOW tightImpedance( targetImpedance, step, TIGHT_WINDOW_TOLERANCE, TIGHT_WINDOW_STEPS );

    auto windowApprox =
            []( const IMPEDANCE_WINDOW& aWindow )
            {
                return [&aWindow]( const APPROX_CANDIDATE& aCandidate )
                       {
                           return aWindow.Classify( aCandidate.impedance );
                       };
            };

    pick( picks, shortlist( candidates, anyImpedanceApprox, ratioErrorApprox ), anyImpedance, ratioError );
    pick( picks, shortlist( candidates, windowApprox( looseImpedance ), ratioErrorApprox ), looseImpedance,
          ratioError );
    pick( picks, shortlist( candidates, windowApprox( tightImpedance ), ratioErrorApprox ), tightImpedance,
          ratioError );

    // ---------- Step 4: Search for the best compromise ---------

    // Solutions 4 and 5: trade ratio error against impedance error, preferring resistor values
    // that the solutions above don't already offer
    std::vector<RATIONAL> usedResistors;

    for( const DIVIDER_CANDIDATE& used : picks )
    {
        if( used.r1 != SIZE_MAX )
        {
            usedResistors.push_back( r1Values[used.r1] );
            usedResistors.push_back( r2Values[used.r2] );
        }
    }

    auto isUsed =
            [&]( const RATIONAL& aResistor )
            {
                return std::find( usedResistors.begin(), usedResistors.end(), aResistor ) != usedResistors.end();
            };

    std::vector<char> r1Used;
    std::vector<char> r2Used;

    for( const RATIONAL& r1 : r1Values )
        r1Used.push_back( isUsed( r1 ) );

    for( const RATIONAL& r2 : r2Values )
        r2Used.push_back( isUsed( r2 ) );

    auto weightedScore =
            [&]( const RATIONAL& aImpedancePoints )
            {
                return [&, aImpedancePoints]( const DIVIDER_CANDIDATE& aCandidate )
                       {
                           int      reused = r1Used[aCandidate.r1] + r2Used[aCandidate.r2];
                           RATIONAL impedancePercent = 100 * aCandidate.impedanceError - IMPEDANCE_FREE_PERCENT;

                           RATIONAL score = RATIO_ERROR_POINTS * 100 * aCandidate.ratioError;

                           if( impedancePercent > 0 )
                               score += aImpedancePoints * impedancePercent;

                           score += REUSED_RESISTOR_POINTS * reused;

                           return score;
                       };
            };

    auto weightedScoreApprox =
            [&]( const RATIONAL& aImpedancePoints )
            {
                const double impedancePoints = aImpedancePoints.convert_to<double>();
                const double ratioPoints = RATIO_ERROR_POINTS.convert_to<double>();
                const double freePercent = IMPEDANCE_FREE_PERCENT.convert_to<double>();
                const double reusedPoints = REUSED_RESISTOR_POINTS.convert_to<double>();

                return [=, &r1Used, &r2Used]( const APPROX_CANDIDATE& aCandidate )
                       {
                           const int    reused = r1Used[aCandidate.r1] + r2Used[aCandidate.r2];
                           const double impedancePercent = 100 * aCandidate.impedanceError - freePercent;

                           return ratioPoints * 100 * aCandidate.ratioError
                                  + impedancePoints * std::max( impedancePercent, 0.0 ) + reusedPoints * reused;
                       };
            };

    pick( picks, shortlist( candidates, anyImpedanceApprox, weightedScoreApprox( LIGHT_IMPEDANCE_POINTS ) ),
          anyImpedance, weightedScore( LIGHT_IMPEDANCE_POINTS ) );
    pick( picks, shortlist( candidates, anyImpedanceApprox, weightedScoreApprox( HEAVY_IMPEDANCE_POINTS ) ),
          anyImpedance, weightedScore( HEAVY_IMPEDANCE_POINTS ) );

    // ---------- Step 5: Results pruning ---------

    // The same pair often wins more than one of the above; report it once
    std::vector<std::pair<size_t, size_t>> reported;
    std::vector<R_DIVIDER_SOLUTION>        solutions;

    for( const DIVIDER_CANDIDATE& picked : picks )
    {
        const std::pair<size_t, size_t> pair( picked.r1, picked.r2 );

        if( picked.r1 == SIZE_MAX || std::find( reported.begin(), reported.end(), pair ) != reported.end() )
            continue;

        reported.push_back( pair );
        solutions.push_back( { r1Approx[picked.r1], r2Approx[picked.r2] } );
    }

    return solutions;
}


struct APPROX_THREE_RESISTOR_CANDIDATE
{
    uint8_t           arrangement; // index into the list of arrangements
    uint32_t          single;      // index into the single resistors of that arrangement
    STANDARD_RESISTOR swept;
    STANDARD_RESISTOR fitted;
    double            impedance;
    double            ratioError;
    double            impedanceError;
};


std::vector<R_DIVIDER_SOLUTION> SolveThreeResistorDivider( double aVin, double aVout, double aImpedance,
                                                           R_DIVIDER_IMPEDANCE          aImpedanceType,
                                                           const std::vector<uint16_t>& aSeries,
                                                           R_DIVIDER_TARGET             aTarget )
{
    if( !isUsableInput( aVin, aVout, aImpedance, aSeries ) )
        return {};

    if( aImpedanceType != R_DIVIDER_IMPEDANCE::SERIES )
        return {};

    // --------- Step 1: Approximate evaluation of every divider --------

    const RATIONAL targetRatio = toRational( aVout ) / toRational( aVin ); // Vout / Vin

    // toRational() rounds, so Vout just below Vin can still give a ratio of exactly 1
    if( targetRatio <= 0 || targetRatio >= 1 )
        return {};

    // The ratio error is measured on what the user asked for: Vout / Vin, or R1 / R2
    const bool     byResistorRatio = aTarget == R_DIVIDER_TARGET::RESISTOR_RATIO;
    const RATIONAL targetResistorRatio = ( 1 - targetRatio ) / targetRatio;
    const double   targetResistorRatioApprox = targetResistorRatio.convert_to<double>();

    const RATIONAL targetImpedance = toRational( aImpedance );
    const RATIONAL step = largestStep( aSeries );
    const int      span = searchSpan( aSeries.size() );

    const double targetRatioApprox = targetRatio.convert_to<double>();
    const double targetImpedanceApprox = targetImpedance.convert_to<double>();

    // Candidates are kept in this order, which settles the ties that the errors leave
    struct ARRANGEMENT
    {
        bool                  splitTop;
        bool                  parallel;
        RATIONAL              legPerSingle;
        std::vector<RATIONAL> singles;
    };

    ARRANGEMENT arrangements[] = { { false, false, {}, {} },
                                   { true, false, {}, {} },
                                   { false, true, {}, {} },
                                   { true, true, {}, {} } };

    std::vector<APPROX_THREE_RESISTOR_CANDIDATE> candidates;
    std::vector<STANDARD_RESISTOR>               sweptValues;

    // The single resistor leg is searched for around its ideal value, as in a two resistor
    // divider. Each value then fixes the other leg that makes the ratio exact, and that leg is
    // split in every way the series allows, both in series and in parallel.
    //
    // Splitting the top leg is the same problem as splitting the bottom one with the divider
    // turned upside down, that is with the ratio 1 - r.
    for( size_t arrangementIdx = 0; arrangementIdx < std::size( arrangements ); ++arrangementIdx )
    {
        ARRANGEMENT& arrangement = arrangements[arrangementIdx];
        const bool   isParallel = arrangement.parallel;

        const RATIONAL singleShare = arrangement.splitTop ? targetRatio : RATIONAL( 1 - targetRatio );
        const RATIONAL idealSingle = singleShare * targetImpedance;

        arrangement.legPerSingle = ( 1 - singleShare ) / singleShare;
        arrangement.singles = resistorsBetween( aSeries, idealSingle / span, idealSingle * span );

        for( size_t singleIdx = 0; singleIdx < arrangement.singles.size(); ++singleIdx )
        {
            const RATIONAL& single = arrangement.singles[singleIdx];
            const RATIONAL  leg = single * arrangement.legPerSingle;

            // A resistor carrying a share of the conductance of a leg is larger than the
            // leg by that share
            const RATIONAL sweptMin = isParallel ? RATIONAL( leg / PARALLEL_LEG_SHARE_MAX )
                                                 : RATIONAL( leg * SERIES_LEG_SHARE_MIN );
            const RATIONAL sweptMax = isParallel ? RATIONAL( leg / PARALLEL_LEG_SHARE_MIN )
                                                 : RATIONAL( leg * SERIES_LEG_SHARE_MAX );

            const double singleApprox = single.convert_to<double>();
            const double legApprox = leg.convert_to<double>();

            standardResistorsBetween( aSeries, sweptMin, sweptMax, sweptValues );

            for( const STANDARD_RESISTOR& swept : sweptValues )
            {
                APPROX_THREE_RESISTOR_CANDIDATE candidate;

                candidate.arrangement = static_cast<uint8_t>( arrangementIdx );
                candidate.single = static_cast<uint32_t>( singleIdx );
                candidate.swept = swept;

                // What the swept resistor leaves for the other one to make up
                const double sweptApprox = swept.Approx();
                const double idealFitted = isParallel ? legApprox * sweptApprox / ( sweptApprox - legApprox )
                                                      : legApprox - sweptApprox;

                if( !nearestStandardResistor( aSeries, idealFitted, isParallel, candidate.fitted ) )
                {
                    const RATIONAL sweptExact = swept.Exact();
                    const RATIONAL idealFittedExact = isParallel ? RATIONAL( leg * sweptExact / ( sweptExact - leg ) )
                                                                 : RATIONAL( leg - sweptExact );

                    candidate.fitted = toStandardResistor( nearestResistor( aSeries, idealFittedExact, isParallel ) );
                }

                const double fittedApprox = candidate.fitted.Approx();
                const double splitLeg = isParallel ? sweptApprox * fittedApprox / ( sweptApprox + fittedApprox )
                                                   : sweptApprox + fittedApprox;

                candidate.impedance = singleApprox + splitLeg;

                // The errors are those of the divider the right way up
                const double bottom = arrangement.splitTop ? singleApprox : splitLeg;

                candidate.ratioError =
                        byResistorRatio
                                ? std::abs( ( candidate.impedance - bottom ) / bottom - targetResistorRatioApprox )
                                          / targetResistorRatioApprox
                                : std::abs( bottom / candidate.impedance - targetRatioApprox ) / targetRatioApprox;
                candidate.impedanceError =
                        std::abs( candidate.impedance - targetImpedanceApprox ) / targetImpedanceApprox;

                candidates.push_back( candidate );
            }
        }
    }

    // ---------- Step 2: Exact evaluation of shortlisted dividers ---------

    // The same candidates tend to be shortlisted for several of the solutions
    std::map<size_t, THREE_RESISTOR_CANDIDATE> exactCache;

    auto exactCandidates =
            [&]( const std::vector<size_t>& aShortlist )
            {
                std::vector<THREE_RESISTOR_CANDIDATE> exact;

                for( size_t i : aShortlist )
                {
                    auto cached = exactCache.find( i );

                    if( cached != exactCache.end() )
                    {
                        exact.push_back( cached->second );
                        continue;
                    }

                    const ARRANGEMENT&       arrangement = arrangements[candidates[i].arrangement];
                    THREE_RESISTOR_CANDIDATE candidate;

                    candidate.splitTop = arrangement.splitTop;
                    candidate.parallel = arrangement.parallel;
                    candidate.single = arrangement.singles[candidates[i].single];
                    candidate.swept = candidates[i].swept.Exact();
                    candidate.fitted = candidates[i].fitted.Exact();

                    const RATIONAL splitLeg =
                            candidate.parallel ? RATIONAL( candidate.swept * candidate.fitted
                                                           / ( candidate.swept + candidate.fitted ) )
                                               : RATIONAL( candidate.swept + candidate.fitted );

                    candidate.impedance = candidate.single + splitLeg;

                    const RATIONAL& bottom = candidate.splitTop ? candidate.single : splitLeg;

                    candidate.ratioError =
                            byResistorRatio ? RATIONAL( abs( ( candidate.impedance - bottom ) / bottom
                                                             - targetResistorRatio )
                                                        / targetResistorRatio )
                                            : RATIONAL( abs( bottom / candidate.impedance - targetRatio )
                                                        / targetRatio );
                    candidate.impedanceError = abs( candidate.impedance - targetImpedance ) / targetImpedance;

                    exactCache.emplace( i, candidate );
                    exact.push_back( std::move( candidate ) );
                }

                return exact;
            };

    // ---------- Step 3: Search for the best ratio ---------

    auto anyImpedance =
            []( const THREE_RESISTOR_CANDIDATE& )
            {
                return true;
            };

    auto anyImpedanceApprox =
            []( const APPROX_THREE_RESISTOR_CANDIDATE& )
            {
                return 1;
            };

    auto exactImpedance =
            []( const THREE_RESISTOR_CANDIDATE& aCandidate )
            {
                return aCandidate.impedanceError <= EXACT_WINDOW_TOLERANCE;
            };

    const double exactToleranceApprox = EXACT_WINDOW_TOLERANCE.convert_to<double>();

    auto exactImpedanceApprox =
            [&]( const APPROX_THREE_RESISTOR_CANDIDATE& aCandidate )
            {
                if( aCandidate.impedanceError < exactToleranceApprox * ( 1 - APPROX_MARGIN ) )
                    return 1;

                return aCandidate.impedanceError > exactToleranceApprox * ( 1 + APPROX_MARGIN ) ? 0 : -1;
            };

    auto ratioError =
            []( const THREE_RESISTOR_CANDIDATE& aCandidate )
            {
                return aCandidate.ratioError;
            };

    auto ratioErrorApprox =
            []( const APPROX_THREE_RESISTOR_CANDIDATE& aCandidate )
            {
                return aCandidate.ratioError;
            };

    const IMPEDANCE_WINDOW looseImpedance( targetImpedance, step, LOOSE_WINDOW_TOLERANCE, LOOSE_WINDOW_STEPS );
    const IMPEDANCE_WINDOW tightImpedance( targetImpedance, step, TIGHT_WINDOW_TOLERANCE, TIGHT_WINDOW_STEPS );

    auto windowApprox =
            []( const IMPEDANCE_WINDOW& aWindow )
            {
                return [&aWindow]( const APPROX_THREE_RESISTOR_CANDIDATE& aCandidate )
                       {
                           return aWindow.Classify( aCandidate.impedance );
                       };
            };

    // The best ratio, with the impedance progressively reined in
    std::vector<THREE_RESISTOR_CANDIDATE> picks;

    auto pick =
            [&]( const std::vector<size_t>& aShortlist, auto aAccept )
            {
                const std::vector<THREE_RESISTOR_CANDIDATE> exact = exactCandidates( aShortlist );

                if( const THREE_RESISTOR_CANDIDATE* best = pickBest( exact, aAccept, ratioError ) )
                    picks.push_back( *best );
            };

    pick( shortlist( candidates, anyImpedanceApprox, ratioErrorApprox ), anyImpedance );
    pick( shortlist( candidates, windowApprox( looseImpedance ), ratioErrorApprox ), looseImpedance );
    pick( shortlist( candidates, windowApprox( tightImpedance ), ratioErrorApprox ), tightImpedance );
    pick( shortlist( candidates, exactImpedanceApprox, ratioErrorApprox ), exactImpedance );

    // ---------- Step 4: Results pruning ---------

    // The same resistors often win more than one of the above; report them once. The two
    // resistors of the split leg are interchangeable, so the same pair the other way round is
    // not a new solution either.
    std::vector<const THREE_RESISTOR_CANDIDATE*> reported;
    std::vector<R_DIVIDER_SOLUTION>              solutions;

    for( const THREE_RESISTOR_CANDIDATE& picked : picks )
    {
        const THREE_RESISTOR_CANDIDATE* current = &picked;

        auto isSameDivider =
                [&]( const THREE_RESISTOR_CANDIDATE* aOther )
                {
                    return aOther->splitTop == current->splitTop && aOther->parallel == current->parallel
                           && aOther->single == current->single
                           && ( ( aOther->swept == current->swept && aOther->fitted == current->fitted )
                                || ( aOther->swept == current->fitted && aOther->fitted == current->swept ) );
                };

        if( std::any_of( reported.begin(), reported.end(), isSameDivider ) )
            continue;

        reported.push_back( current );

        R_DIVIDER_SOLUTION solution;

        if( current->parallel )
            solution.topology = current->splitTop ? R_DIVIDER_TOPOLOGY::PARALLEL_TOP
                                                  : R_DIVIDER_TOPOLOGY::PARALLEL_BOTTOM;
        else
            solution.topology = current->splitTop ? R_DIVIDER_TOPOLOGY::SPLIT_TOP : R_DIVIDER_TOPOLOGY::SPLIT_BOTTOM;

        solution.R1 = ( current->splitTop ? current->swept : current->single ).convert_to<double>();
        solution.R2 = ( current->splitTop ? current->single : current->swept ).convert_to<double>();
        solution.R3 = current->fitted.convert_to<double>();

        solutions.push_back( solution );
    }

    return solutions;
}


std::vector<R_DIVIDER_RESULT> FindResistorDividers( double aVin, double aVout, double aImpedance,
                                                    R_DIVIDER_IMPEDANCE          aImpedanceType,
                                                    const std::vector<uint16_t>& aSeries, R_DIVIDER_TARGET aTarget )
{
    // Keeps two results that are equally good from beating each other on round-off
    static const double MARGIN = 1e-11;

    if( !isUsableInput( aVin, aVout, aImpedance, aSeries ) )
        return {};

    // The targets as the solvers understand them, so that a divider they find exact has no error
    const RATIONAL targetRatioExact = toRational( aVout ) / toRational( aVin );

    if( targetRatioExact <= 0 || targetRatioExact >= 1 )
        return {};

    const double targetRatio = targetRatioExact.convert_to<double>();
    const double targetResistorRatio = RATIONAL( ( 1 - targetRatioExact ) / targetRatioExact ).convert_to<double>();

    // Compute the various displayed criteria (%error, impedence) for a given solution
    auto evaluate =
            [&]( const R_DIVIDER_SOLUTION& aSolution )
            {
                const double top = aSolution.Top();
                const double bottom = aSolution.Bottom();

                R_DIVIDER_RESULT result;

                result.solution = aSolution;
                result.impedance = aImpedanceType == R_DIVIDER_IMPEDANCE::PARALLEL ? top * bottom / ( top + bottom )
                                                                                   : top + bottom;
                result.impedanceError = result.impedance / aImpedance - 1;

                if( aTarget == R_DIVIDER_TARGET::RESISTOR_RATIO )
                    result.targetError = top / bottom / targetResistorRatio - 1;
                else
                    result.targetError = bottom / ( top + bottom ) / targetRatio - 1;

                return result;
            };

    std::vector<R_DIVIDER_RESULT> results;

    double bestTargetError = std::numeric_limits<double>::infinity();
    double bestImpedanceError = std::numeric_limits<double>::infinity();

    // Get results from the two-resistor topologies, and add them to the results array
    for( const R_DIVIDER_SOLUTION& solution :
         SolveResistorDivider( aVin, aVout, aImpedance, aImpedanceType, aSeries, aTarget ) )
    {
        results.push_back( evaluate( solution ) );

        // Store the best performance figures from the two-resistor topologies
        bestTargetError = std::min( bestTargetError, std::abs( results.back().targetError ) );
        bestImpedanceError = std::min( bestImpedanceError, std::abs( results.back().impedanceError ) );
    }

    // Get results from the three resistor topologies, and add them to the results array.
    for( const R_DIVIDER_SOLUTION& solution :
         SolveThreeResistorDivider( aVin, aVout, aImpedance, aImpedanceType, aSeries, aTarget ) )
    {
        const R_DIVIDER_RESULT result = evaluate( solution );

        // Only solutions that are better then all of the two-resistor solutions on at least one metric
        // are shown to the user
        if( std::abs( result.targetError ) < bestTargetError - MARGIN
            || std::abs( result.impedanceError ) < bestImpedanceError - MARGIN )
        {
            results.push_back( result );
        }
    }

    return results;
}
