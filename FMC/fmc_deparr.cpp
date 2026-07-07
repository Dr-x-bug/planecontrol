#include "fmc_deparr.h"

// ===== KSEA 跑道 =====
Runway ksea_runways[] = {
    {"16L", 47.450, -122.309, 164, 11901},
    {"16C", 47.455, -122.312, 164, 9426},
    {"16R", 47.460, -122.315, 164, 8500},
    {"34L", 47.460, -122.315, 344, 8500},
    {"34C", 47.455, -122.312, 344, 9426},
    {"34R", 47.450, -122.309, 344, 11901},
};
int ksea_rwy_count = 6;

// ===== KBFI 跑道 =====
Runway kbfi_runways[] = {
    {"13L", 47.530, -122.302, 134, 10000},
    {"13R", 47.535, -122.305, 134, 3700},
    {"31L", 47.535, -122.305, 314, 3700},
    {"31R", 47.530, -122.302, 314, 10000},
};
int kbfi_rwy_count = 4;

// ===== KSEA SID 离场程序 =====
Procedure ksea_sids[] = {
    {"SEA5",    'S', "16L", {"SEA", "BLI", "FREDY"}, 3},
    {"SEA5",    'S', "16R", {"SEA", "BLI", "FREDY"}, 3},
    {"SUMMA2",  'S', "34R", {"SUMMA", "BLI", "YVR"}, 3},
    {"BANGR9",  'S', "",    {"BANGR", "EPH", "MLP"}, 3},
    {"HAROB6",  'S', "16L", {"HAROB", "PAE", "SEA"}, 3},
    {"HAROB6",  'S', "16C", {"HAROB", "PAE", "SEA"}, 3},
    {"MOUNT7",  'S', "34L", {"MOUNT", "OLM", "BTG"}, 3},
};
int ksea_sid_count = 7;

// ===== KBFI STAR 进场程序 =====
Procedure kbfi_stars[] = {
    {"GLASR1",  'T', "13L", {"GLASR", "OLM", "SEA"}, 3},
    {"GLASR1",  'T', "13R", {"GLASR", "OLM", "SEA"}, 3},
    {"OLM2",    'T', "31R", {"OLM", "BTG", "PDX"}, 3},
    {"CHINS3",  'T', "",    {"CHINS", "PAE", "SEA"}, 3},
    {"ILS13L",  'A', "13L", {"FF13L", "RW13L"}, 2},
    {"ILS13R",  'A', "13R", {"FF13R", "RW13R"}, 2},
    {"RNAV31L", 'A', "31L", {"FF31L", "RW31L"}, 2},
    {"RNAV31R", 'A', "31R", {"FF31R", "RW31R"}, 2},
};
int kbfi_star_count = 8;

DepArrState g_deparr;
