/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include "tracked_location.h"

kis_tracked_location_triplet::kis_tracked_location_triplet(GlobalRegistry *in_globalreg, 
        int in_id) : tracker_component(in_globalreg, in_id) {
    register_fields();
    reserve_fields(NULL);
} 

kis_tracked_location_triplet::kis_tracked_location_triplet(GlobalRegistry *in_globalreg, 
        int in_id, SharedTrackerElement e) : tracker_component(in_globalreg, in_id) {
    register_fields();
    reserve_fields(e);
}

SharedTrackerElement kis_tracked_location_triplet::clone_type() {
    return SharedTrackerElement(new kis_tracked_location_triplet(globalreg, 
                get_id()));
}

void kis_tracked_location_triplet::set(double in_lat, double in_lon, 
        double in_alt, unsigned int in_fix) {
    set_lat(in_lat);
    set_lon(in_lon);
    set_alt(in_alt);
    set_fix(in_fix);
    set_valid(1);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    set_time_sec(tv.tv_sec);
    set_time_usec(tv.tv_usec);
}

void kis_tracked_location_triplet::set(double in_lat, double in_lon) {
    set_lat(in_lat);
    set_lon(in_lon);
    set_fix(2);
    set_valid(1);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    set_time_sec(tv.tv_sec);
    set_time_usec(tv.tv_usec);
}

kis_tracked_location_triplet& 
    kis_tracked_location_triplet::operator= (const kis_tracked_location_triplet& in) {
    set_lat(in.get_lat());
    set_lon(in.get_lon());
    set_alt(in.get_alt());
    set_speed(in.get_speed());
    set_heading(in.get_heading());
    set_fix(in.get_fix());
    set_valid(in.get_valid());
    set_time_sec(in.get_time_sec());
    set_time_usec(in.get_time_usec());

    return *this;
}

void kis_tracked_location_triplet::register_fields() {
    tracker_component::register_fields();

    RegisterField("kismet.common.location.lat", TrackerDouble,
            "latitude", &lat);
    RegisterField("kismet.common.location.lon", TrackerDouble,
            "longitude", &lon);
    RegisterField("kismet.common.location.alt", TrackerDouble,
            "altitude", &alt);
    RegisterField("kismet.common.location.speed", TrackerDouble,
            "speed", &spd);
    RegisterField("kismet.common.location.heading", TrackerDouble,
            "heading", &heading);
    RegisterField("kismet.common.location.fix", TrackerUInt8,
            "gps fix", &fix);
    RegisterField("kismet.common.location.valid", TrackerUInt8,
            "valid location", &valid);
    RegisterField("kismet.common.location.time_sec", TrackerUInt64,
            "timestamp (seconds)", &time_sec);
    RegisterField("kismet.common.location.time_usec", TrackerUInt64,
            "timestamp (usec)", &time_usec);
}

kis_tracked_location::kis_tracked_location(GlobalRegistry *in_globalreg, int in_id) :
    tracker_component(in_globalreg, in_id) { 
    register_fields();
    reserve_fields(NULL);
}

kis_tracked_location::kis_tracked_location(GlobalRegistry *in_globalreg, int in_id, 
        SharedTrackerElement e) : tracker_component(in_globalreg, in_id) {

    register_fields();
    reserve_fields(e);
}

SharedTrackerElement kis_tracked_location::clone_type() {
    return SharedTrackerElement(new kis_tracked_location(globalreg, get_id()));
}


void kis_tracked_location::add_loc(double in_lat, double in_lon, double in_alt, 
        unsigned int fix) {
    set_valid(1);

    if (fix > get_fix()) {
        set_fix(fix);
    }

    if (in_lat < min_loc->get_lat() || min_loc->get_lat() == 0) {
        min_loc->set_lat(in_lat);
    }

    if (in_lat > max_loc->get_lat() || max_loc->get_lat() == 0) {
        max_loc->set_lat(in_lat);
    }

    if (in_lon < min_loc->get_lon() || min_loc->get_lon() == 0) {
        min_loc->set_lon(in_lon);
    }

    if (in_lon > max_loc->get_lon() || max_loc->get_lon() == 0) {
        max_loc->set_lon(in_lon);
    }

    if (fix > 2) {
        if (in_alt < min_loc->get_alt() || min_loc->get_alt() == 0) {
            min_loc->set_alt(in_alt);
        }

        if (in_alt > max_loc->get_alt() || max_loc->get_alt() == 0) {
            max_loc->set_alt(in_alt);
        }
    }

    // Append to averaged location
    (*avg_lat) += (int64_t) (in_lat * precision_multiplier);
    (*avg_lon) += (int64_t) (in_lon * precision_multiplier);
    (*num_avg)++;

    if (fix > 2) {
        (*avg_alt) += (int64_t) (in_alt * precision_multiplier);
        (*num_alt_avg)++;
    }

    double calc_lat, calc_lon, calc_alt;

    calc_lat = (double) (GetTrackerValue<int64_t>(avg_lat) / 
            GetTrackerValue<int64_t>(num_avg)) / precision_multiplier;
    calc_lon = (double) (GetTrackerValue<int64_t>(avg_lon) / 
            GetTrackerValue<int64_t>(num_avg)) / precision_multiplier;
    if (GetTrackerValue<int64_t>(num_alt_avg) != 0) {
        calc_alt = (double) (GetTrackerValue<int64_t>(avg_alt) / 
                GetTrackerValue<int64_t>(num_alt_avg)) / precision_multiplier;
    } else {
        calc_alt = 0;
    }
    avg_loc->set(calc_lat, calc_lon, calc_alt, 3);

    // Are we getting too close to the maximum size of any of our counters?
    // This would take a really long time but we might as well be safe.  We're
    // throwing away some of the highest ranges but it's a cheap compare.
    uint64_t max_size_mask = 0xF000000000000000LL;
    if ((GetTrackerValue<int64_t>(avg_lat) & max_size_mask) ||
            (GetTrackerValue<int64_t>(avg_lon) & max_size_mask) ||
            (GetTrackerValue<int64_t>(avg_alt) & max_size_mask) ||
            (GetTrackerValue<int64_t>(num_avg) & max_size_mask) ||
            (GetTrackerValue<int64_t>(num_alt_avg) & max_size_mask)) {
        avg_lat->set((int64_t) (calc_lat * precision_multiplier));
        avg_lon->set((int64_t) (calc_lon * precision_multiplier));
        avg_alt->set((int64_t) (calc_alt * precision_multiplier));
        num_avg->set((int64_t) 1);
        num_alt_avg->set((int64_t) 1);
    }
}

void kis_tracked_location::register_fields() {
    tracker_component::register_fields();

    RegisterField("kismet.common.location.loc_valid", TrackerUInt8,
            "location data valid", &loc_valid);

    RegisterField("kismet.common.location.loc_fix", TrackerUInt8,
            "location fix precision (2d/3d)", &loc_fix);

    shared_ptr<kis_tracked_location_triplet> 
        loc_builder(new kis_tracked_location_triplet(globalreg, 0));

    min_loc_id = 
        RegisterComplexField("kismet.common.location.min_loc", loc_builder, 
                "minimum corner of bounding rectangle");
    max_loc_id = 
        RegisterComplexField("kismet.common.location.max_loc", loc_builder,
                "maximum corner of bounding rectangle");
    avg_loc_id = 
        RegisterComplexField("kismet.common.location.avg_loc", loc_builder,
                "average corner of bounding rectangle");

    RegisterField("kismet.common.location.avg_lat", TrackerInt64,
            "run-time average latitude", &avg_lat);
    RegisterField("kismet.common.location.avg_lon", TrackerInt64,
            "run-time average longitude", &avg_lon);
    RegisterField("kismet.common.location.avg_alt", TrackerInt64,
            "run-time average altitude", &avg_alt);
    RegisterField("kismet.common.location.avg_num", TrackerInt64,
            "number of run-time average samples", &num_avg);
    RegisterField("kismet.common.location.avg_alt_num", 
            TrackerInt64,
            "number of run-time average samples (altitude)", &num_alt_avg);

}

void kis_tracked_location::reserve_fields(SharedTrackerElement e) {
    tracker_component::reserve_fields(e);

    if (e != NULL) {
        min_loc.reset(new kis_tracked_location_triplet(globalreg, min_loc_id, 
                    e->get_map_value(min_loc_id)));
        max_loc.reset(new kis_tracked_location_triplet(globalreg, max_loc_id, 
                    e->get_map_value(max_loc_id)));
        avg_loc.reset(new kis_tracked_location_triplet(globalreg, avg_loc_id, 
                    e->get_map_value(avg_loc_id)));
    } else {
        min_loc.reset(new kis_tracked_location_triplet(globalreg, min_loc_id));
        max_loc.reset(new kis_tracked_location_triplet(globalreg, max_loc_id));
        avg_loc.reset(new kis_tracked_location_triplet(globalreg, avg_loc_id));
    }

    add_map(avg_loc);
    add_map(min_loc);
    add_map(max_loc);

}

