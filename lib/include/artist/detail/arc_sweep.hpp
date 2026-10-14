/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_ARC_SWEEP_OCTOBER_14_2026)
#define ARTIST_DETAIL_ARC_SWEEP_OCTOBER_14_2026

#include <infra/support.hpp>
#include <cmath>

namespace cycfi::artist::detail
{
   // The sweep of an arc from start_angle to end_angle, as the HTML canvas
   // has it: clockwise, from 0 up to a full turn; counterclockwise, from 0
   // down to a full turn.
   inline double arc_sweep(double start_angle, double end_angle, bool ccw)
   {
      auto sweep = end_angle - start_angle;
      if (!ccw)
      {
         if (sweep >= 2 * pi)
            return 2 * pi;
         sweep = std::fmod(sweep, 2 * pi);
         return sweep < 0? sweep + 2 * pi : sweep;
      }
      if (sweep <= -2 * pi)
         return -2 * pi;
      sweep = std::fmod(sweep, 2 * pi);
      return sweep > 0? sweep - 2 * pi : sweep;
   }
}

#endif
