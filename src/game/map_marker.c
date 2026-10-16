#include "game/map_marker.h"

#include <stdio.h>
#include <string.h>

bool map_marker_compose(char* output, size_t capacity, const char* title, const char* body)
{
    if (output == NULL || title == NULL || body == NULL) return false;
    size_t a = strlen(title), b = strlen(body);
    bool meaningful = false;
    for (size_t i = 0; i < a; i++) {
        if (title[i] != ' ' && title[i] != '\t' && title[i] != '\r' && title[i] != '\n') meaningful = true;
    }
    if (!meaningful || a >= MAP_MARKER_TITLE_SIZE || b >= MAP_MARKER_BODY_SIZE
        || strchr(title, '\n') != NULL || strchr(title, '\r') != NULL
        || a + b + (b > 0 ? 2 : 1) > capacity) return false;
    snprintf(output, capacity, b > 0 ? "%s\n%s" : "%s%s", title, body);
    return true;
}

void map_marker_split(const char* text, char title[MAP_MARKER_TITLE_SIZE], char body[MAP_MARKER_BODY_SIZE])
{
    const char* newline = strchr(text, '\n');
    size_t size = newline != NULL ? (size_t)(newline - text) : strlen(text);
    if (size >= MAP_MARKER_TITLE_SIZE) size = MAP_MARKER_TITLE_SIZE - 1;
    memcpy(title, text, size);
    title[size] = '\0';
    snprintf(body, MAP_MARKER_BODY_SIZE, "%s", newline != NULL ? newline + 1 : "");
}

bool map_marker_serialized_valid(int type, int text_size)
{
    return type >= 0 && type < 8 && text_size >= 0 && text_size < MAP_MARKER_TEXT_SIZE;
}

bool map_marker_description_valid(const char* text)
{
    if (text == NULL) return false;
    bool meaningful = false;
    size_t length = 0;
    while (text[length] != '\0') {
        if (text[length] != ' ' && text[length] != '\t' && text[length] != '\r' && text[length] != '\n') meaningful = true;
        if (++length >= MAP_MARKER_TEXT_SIZE) return false;
    }
    return meaningful;
}

bool map_marker_record_save(const MapMarkerRecord* record, TigFile* stream)
{
    if (record == NULL || stream == NULL || memchr(record->text, '\0', sizeof(record->text)) == NULL) return false;
    int size = (int)strlen(record->text);
    unsigned int flags = record->flags & ~0x4u;
    if (record->id < 0 || !map_marker_serialized_valid(record->type, size)) return false;
    return tig_file_fwrite(&record->id, sizeof(int), 1, stream) == 1
        && tig_file_fwrite(&flags, sizeof(flags), 1, stream) == 1
        && tig_file_fwrite(&record->x, sizeof(int), 1, stream) == 1
        && tig_file_fwrite(&record->y, sizeof(int), 1, stream) == 1
        && tig_file_fwrite(record->bounds, sizeof(int), 4, stream) == 4
        && tig_file_fwrite(&record->type, sizeof(int), 1, stream) == 1
        && tig_file_fwrite(&size, sizeof(int), 1, stream) == 1
        && tig_file_fwrite(record->text, 1, size, stream) == (size_t)size;
}

bool map_marker_record_load(MapMarkerRecord* record, TigFile* stream)
{
    if (record == NULL || stream == NULL) return false;
    MapMarkerRecord loaded = { 0 };
    int size;
    if (tig_file_fread(&loaded.id, sizeof(int), 1, stream) != 1
        || tig_file_fread(&loaded.flags, sizeof(loaded.flags), 1, stream) != 1
        || tig_file_fread(&loaded.x, sizeof(int), 1, stream) != 1
        || tig_file_fread(&loaded.y, sizeof(int), 1, stream) != 1
        || tig_file_fread(loaded.bounds, sizeof(int), 4, stream) != 4
        || tig_file_fread(&loaded.type, sizeof(int), 1, stream) != 1
        || tig_file_fread(&size, sizeof(int), 1, stream) != 1
        || loaded.id < 0 || !map_marker_serialized_valid(loaded.type, size)
        || tig_file_fread(loaded.text, 1, size, stream) != (size_t)size) return false;
    loaded.flags &= ~0x4u;
    *record = loaded;
    return true;
}
