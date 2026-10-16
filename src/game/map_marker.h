#ifndef ARCANUM_GAME_MAP_MARKER_H_
#define ARCANUM_GAME_MAP_MARKER_H_

#include <stdbool.h>
#include <stddef.h>
#include <tig/file.h>

#define MAP_MARKER_PERSONAL 0x08u
#define MAP_MARKER_COMPLETED 0x10u
#define MAP_MARKER_TITLE_SIZE 65
#define MAP_MARKER_BODY_SIZE 181
#define MAP_MARKER_TEXT_SIZE 250

bool map_marker_compose(char* output, size_t capacity, const char* title, const char* body);
void map_marker_split(const char* text, char title[MAP_MARKER_TITLE_SIZE], char body[MAP_MARKER_BODY_SIZE]);
bool map_marker_serialized_valid(int type, int text_size);
bool map_marker_description_valid(const char* text);

// Existing map-note wire format; no additional save section or version.
typedef struct MapMarkerRecord {
    int id;
    unsigned int flags;
    int x, y;
    int bounds[4];
    int type;
    char text[MAP_MARKER_TEXT_SIZE];
} MapMarkerRecord;
bool map_marker_record_save(const MapMarkerRecord* record, TigFile* stream);
bool map_marker_record_load(MapMarkerRecord* record, TigFile* stream);

#endif
