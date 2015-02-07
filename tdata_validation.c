/* Copyright 2013 Bliksem Labs.
 * See the LICENSE file at the top-level directory of this distribution and at
 * https://github.com/bliksemlabs/rrrr/
 */

#include "tdata_validation.h"
#include "tdata.h"

#include <stdio.h>

/* Validate that the first journey_pattern_point have won't alighting set and
 * the last journey_pattern_point won't allow boarding.
 */
int tdata_validation_boarding_alighting(tdata_t *tdata) {
    int32_t ret_invalid = 0;
    jpidx_t i_jp = (jpidx_t) tdata->n_journey_patterns;

    do {
        journey_pattern_t *jp;
        uint8_t *rsa;

        i_jp--;

        jp = &(tdata->journey_patterns[i_jp]);
        rsa = tdata->journey_pattern_point_attributes +
              jp->journey_pattern_point_offset;

        if ((rsa[0] & rsa_alighting) == rsa_alighting ||
            (rsa[jp->n_stops - 1] & rsa_boarding) == rsa_boarding) {
            fprintf(stderr, "journey_pattern index %d %s %s %s has:\n%s%s", i_jp,
                    tdata_operator_name_for_journey_pattern(tdata, i_jp),
                    tdata_line_code_for_journey_pattern(tdata, i_jp),
                    tdata_headsign_for_journey_pattern(tdata, i_jp),
              ((rsa[0] & rsa_alighting) == rsa_alighting ?
                "  alighting on the first stop\n" : ""),

              ((rsa[jp->n_stops - 1] & rsa_boarding) == rsa_boarding ?
                "  boarding on the last stop\n" : ""));

            ret_invalid--;
        }

        if (ret_invalid < -10) {
            fprintf(stderr, "Too many boarding/alighting problems.\n");
            break;
        }
    } while (i_jp);

    return ret_invalid;
}

/* Check that all lat/lon look like valid coordinates.
 * If validation fails the number of invalid coordinates are return
 * as negative value.
 */
int tdata_validation_coordinates(tdata_t *tdata) {

    /* farther south than Ushuaia, Argentina */
    float min_lat = -55.0f;

    /* farther north than Tromsø and Murmansk */
    float max_lat = +70.0f;
    float min_lon = -180.0f;
    float max_lon = +180.0f;

    int32_t ret_invalid = 0;

    uint32_t sp_index = tdata->n_stop_points;

    do {
        latlon_t ll;

        sp_index--;

        ll = tdata->stop_point_coords[sp_index];
        if (ll.lat < min_lat || ll.lat > max_lat ||
            ll.lon < min_lon || ll.lon > max_lon) {
            fprintf (stderr, "stop lat/lon out of range: lat=%f, lon=%f \n",
                                                ll.lat, ll.lon);
            ret_invalid--;
        }
    } while (sp_index);

    return ret_invalid;
}

/* Check that all timedemand types start at 0 and consist of
 * monotonically increasing times.
 */
int tdata_validation_increasing_times(tdata_t *tdata) {

    uint32_t jp_index, sp_index, vj_index;
    int ret_nonincreasing = 0;
    for (jp_index = 0; jp_index < tdata->n_journey_patterns; ++jp_index) {
        journey_pattern_t jp = tdata->journey_patterns[jp_index];
        vehicle_journey_t *vjs = tdata->vjs + jp.vj_offset;

        #ifdef RRRR_DEBUG
        /* statistics on errors, instead of early bail out */
        int n_nonincreasing_vjs = 0;
        #endif

        for (vj_index = 0; vj_index < jp.n_vjs; ++vj_index) {
            vehicle_journey_t vj = vjs[vj_index];
            stoptime_t *st = tdata->stop_times + vj.stop_times_offset;
            stoptime_t *prev_st = NULL;
            for (sp_index = 0; sp_index < jp.n_stops; ++sp_index) {
                if (sp_index == 0 && st->arrival != 0) {
                    fprintf (stderr,
                             "timedemand type begins at %d,%d not 0.\n",
                             st->arrival, st->departure);
                    #ifndef RRRR_DEBUG
                    return -1;
                    #endif
                }

                if (st->departure < st->arrival) {
                    fprintf (stderr, "departure before arrival at "
                                     "journey_pattern %d, vj %d, stop %d.\n",
                            jp_index, vj_index, sp_index);
                    #ifndef RRRR_DEBUG
                    return -1;
                    #endif
                }

                if (prev_st != NULL) {
                    if (st->arrival < prev_st->departure) {
                        char const *vj_id = "";
                        if (tdata->vj_ids) {
                            vj_id = tdata_vehicle_journey_id_for_index(tdata,vj_index);
                        }

                        fprintf (stderr, "negative travel time arriving at "
                                         "journey_pattern %d, vj %d (%s), stop %d.\n",
                                jp_index, vj_index,
                                vj_id, sp_index);
                        #if 0
                        fprintf (stderr, "(%d, %d) -> (%d, %d)\n",
                                         prev_st->arrival, prev_st->departure,
                                         st->arrival, st->departure);
                        #endif

                        #ifdef RRRR_DEBUG
                        n_nonincreasing_vjs += 1;
                        #else
                        return -1;
                        #endif
                    } else if (st->arrival == prev_st->departure) {
                        #if 0
                        fprintf (stderr, "last departure equals arrival at "
                                         "journey_pattern %d, vj %d, stop %d.\n",
                                jp_index, vj_index, sp_index);

                        #ifdef RRRR_DEBUG
                        n_nonincreasing_vjs += 1;
                        #else
                        return -1;
                        #endif
                        #endif
                    }
                }
                prev_st = st++;
            }
        }
        #ifdef RRRR_DEBUG
        if (n_nonincreasing_vjs > 0) {
            fprintf (stderr, "journey_pattern %d has %d vehicle_journeys with "
                             "negative travel times\n",
                    jp_index, n_nonincreasing_vjs);
            ret_nonincreasing -= n_nonincreasing_vjs;
        }
        #endif
    }

    return ret_nonincreasing;
}

/* Check that all transfers are symmetric.
 */
int tdata_validation_symmetric_transfers(tdata_t *tdata) {
    int n_transfers_checked = 0;
    spidx_t sp_index_from;
    for (sp_index_from = 0;
         sp_index_from < tdata->n_stop_points;
         ++sp_index_from) {

        /* Iterate over all transfers going out of this stop_point */
        uint32_t t  = tdata->stop_points[sp_index_from].transfers_offset;
        uint32_t tN = tdata->stop_points[sp_index_from + 1].transfers_offset;
        for ( ; t < tN ; ++t) {
            spidx_t sp_index_to = tdata->transfer_target_stops[t];
            rtime_t forward_duration = tdata->transfer_durations[t];

            /* Find the reverse transfer (sp_index_to -> sp_index_from) */
            uint32_t u  = tdata->stop_points[sp_index_to].transfers_offset;
            uint32_t uN = tdata->stop_points[sp_index_to + 1].transfers_offset;
            bool found_reverse = false;

            for ( ; u < uN ; ++u) {
                n_transfers_checked += 1;
                if (tdata->transfer_target_stops[u] == sp_index_from) {
                    /* this is the same transfer in reverse */
                    rtime_t reverse_duration = tdata->transfer_durations[u];
                    if (reverse_duration != forward_duration) {
                        fprintf (stderr, "transfer from_stop_point %d to %d is "
                                         "not symmetric. "
                                         "forward duration is %d, "
                                         "reverse duration is %d.\n",
                                sp_index_from,
                                sp_index_to,
                                         forward_duration,
                                         reverse_duration);
                    }
                    found_reverse = true;
                    break;
                }
            }
            if ( ! found_reverse) {
                fprintf (stderr, "transfer from_stop_point %d to %d does not have "
                                 "an equivalent reverse transfer.\n",
                        sp_index_from, sp_index_to);
                return -1;
            }
        }
    }
    fprintf (stderr, "checked %d transfers for symmetry.\n",
                     n_transfers_checked);

    return 0;
}

static bool tdata_validation_check_nstop_points(tdata_t *tdata) {
    if (tdata->n_stop_points < 2) {
        fprintf (stderr, "n_stop_points should be at least two, %d found.\n", tdata->n_stop_points);
        return false;
    } else
    if (tdata->n_stop_points > ONBOARD) {
        fprintf (stderr, "n_stop_points %d exceeds compiled spidx_t width.\n", tdata->n_stop_points);
        return false;
    }

    return true;
}

bool tdata_validation_check_coherent (tdata_t *tdata) {
    fprintf (stderr, "checking tdata coherency...\n");

    return  (tdata_validation_check_nstop_points(tdata) &&
             tdata->n_journey_patterns > 0 &&
             tdata->n_journey_patterns < ((jpidx_t) -1) &&
             tdata_validation_boarding_alighting(tdata) == 0 &&
             tdata_validation_coordinates(tdata) == 0 &&
             tdata_validation_increasing_times(tdata) == 0 &&
             tdata_validation_symmetric_transfers(tdata) == 0);
}

