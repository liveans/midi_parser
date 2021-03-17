/* Add any includes here */
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include "parser.h"
#include <unistd.h>

static int offset = 0;

static uint32_t given_vlq_byte_length(uint32_t value) {
    if(value > 0x1FFFFF)
        return 4;

    if(value > 0x3FFF)
        return 3;

    if(value > 0x7F)
        return 2;

    return 1;
}

int count_linked_list(track_node_t *node) {
    int count = 0;
    while(node != NULL) {
        node = node->next_track;
    }
    return count;
}

song_data_t *parse_file(const char *file_path) {
    assert(file_path != NULL);
    FILE *fp = fopen(file_path, "rb"); //Read Binary
    assert(fp != NULL);
    song_data_t *song_data = (song_data_t *) malloc(sizeof(song_data_t));
    assert(song_data != NULL);
    song_data->path = (char *) malloc(strlen(file_path) + 1);
    assert(song_data->path != NULL);
    strcpy(song_data->path, file_path);
    parse_header(fp, song_data);
    track_node_t *current_list_ptr = NULL;
    for (size_t i = 0; i < song_data->num_tracks; ++i) {
        if(current_list_ptr == NULL) {
            song_data->track_list = (track_node_t *)malloc(sizeof(track_node_t));
            assert(song_data->track_list != NULL);
            current_list_ptr = song_data->track_list;
        }
        else {
            current_list_ptr->next_track = (track_node_t *)malloc(sizeof(track_node_t));
            assert(current_list_ptr != NULL);
            current_list_ptr = current_list_ptr->next_track;
        }
        current_list_ptr->track = (track_t *)malloc(sizeof(track_t));
        assert(current_list_ptr->track != NULL);
        current_list_ptr->next_track = NULL;
        parse_track(fp, song_data);
        fflush(fp);
    }
    assert(getc(fp) == EOF);
    fclose(fp);
    return song_data;
}

void parse_header(FILE *fp, song_data_t *song_data) {
    assert(fp != NULL);
    assert(song_data != NULL);

    char chunk_id[5];   //assert chunk_id == "MThd"
    assert(chunk_id != NULL);
    chunk_id[4] = '\0';
    fread(chunk_id, 4, 1, fp);
    assert(!strncmp(chunk_id, "MThd", 4));

    char number_header[1];   //assert header_size == 6;
    fseek(fp, 7, SEEK_SET);
    fread(number_header, 1, 1, fp);
    assert(number_header[0] == 6);

    uint8_t format[2];   //init format
    fread(format, 2, 1, fp);
    song_data->format = format[1];

    uint8_t num_tracks[2]; //init num_tracks
    fread(num_tracks, 2, 1, fp);
    uint8_t num_tracks_array[2];
    num_tracks_array[0] = (uint8_t) num_tracks[0];
    num_tracks_array[1] = (uint8_t) num_tracks[1];
    song_data->num_tracks = end_swap_16(num_tracks_array);

    uint8_t division[2];
    uint8_t division_array[2];
    fread(division, 2, 1, fp);
    division_array[0] = (uint8_t) division[0];
    division_array[1] = (uint8_t) division[1];

    if ((division[0] & (1u << 7u)) == 0) {
        song_data->division.uses_tpq = true;
        song_data->division.ticks_per_qtr = end_swap_16(division_array);
    } else {
        song_data->division.uses_tpq = false;
        song_data->division.ticks_per_frame = division_array[0];
        song_data->division.frames_per_sec = division_array[1];
    }
}

void parse_track(FILE *fp, song_data_t *song_data) {
    char chunk_id[5];
    chunk_id[4]='\0';
    fread(chunk_id, 4, 1, fp);
    assert(!strncmp(chunk_id, "MTrk", 4));
    uint32_t chunk_len;
    fread(&chunk_len, 4, 1, fp);
    chunk_len = end_swap_32((uint8_t*)&chunk_len);
    track_node_t* current_track = song_data->track_list;
    while(current_track->next_track != NULL)
        current_track = current_track->next_track;

    current_track->track->event_list = NULL;
    current_track->track->length = chunk_len;
    event_node_t* current_event = NULL;
    offset = 0;
    while (offset < current_track->track->length)
    {
        if(current_event == NULL) {
            current_track->track->event_list = (event_node_t*)malloc(sizeof(event_node_t));
            assert(current_track->track->event_list != NULL);
            current_event = current_track->track->event_list;
        }
        else {
            current_event->next_event = (event_node_t*)malloc(sizeof(event_node_t));
            assert(current_event->next_event != NULL);
            current_event = current_event->next_event;
        }
        current_event->event = parse_event(fp);
        current_event->next_event = NULL;
    }
}

event_t *parse_event(FILE *fp) {
    event_t *event = (event_t *)malloc(sizeof(event_t));
    assert(event != NULL);
    event->delta_time = parse_var_len(fp);
    offset += given_vlq_byte_length(event->delta_time);
    event->type = getc(fp);
    ++offset;
    switch (event_type(event)) {
        case SYS_EVENT_T:
            event->sys_event = parse_sys_event(fp);
            break;
        case MIDI_EVENT_T:
            event->midi_event = parse_midi_event(fp, event->type);
            break;
        case META_EVENT_T:
            event->meta_event = parse_meta_event(fp);
            break;
        default:
            //TODO: Fatal Error!
            break;
    }
    return event;
}

sys_event_t parse_sys_event(FILE *fp) {
    sys_event_t test_sys;
    test_sys.data_len = parse_var_len(fp);
    offset += given_vlq_byte_length(test_sys.data_len);
    test_sys.data = (uint8_t *) malloc(test_sys.data_len);
    uint8_t* ptr = test_sys.data;

    while(ptr != test_sys.data + test_sys.data_len) {
        *ptr++ = (uint8_t) getc(fp);
        ++offset;
    }

    return test_sys;
}

meta_event_t parse_meta_event(FILE *fp) {
    uint8_t type = (uint8_t) getc(fp);
    ++offset;
    assert(META_TABLE[type].name != NULL);
    meta_event_t test_meta = META_TABLE[type];
    if(test_meta.data_len == 0) {
        test_meta.data_len = parse_var_len(fp);
        offset += given_vlq_byte_length(test_meta.data_len);
    }
    else {
        uint8_t val = getc(fp);
        ++offset;
        assert(val == test_meta.data_len);
    }

    if(test_meta.data_len > 0) {
        test_meta.data = (uint8_t *) malloc(test_meta.data_len);
        assert(test_meta.data != NULL);
        uint8_t* ptr = test_meta.data;
        while(ptr != test_meta.data + test_meta.data_len) {
            *ptr++ = (uint8_t) getc(fp);
            ++offset;
        }
    }

    return test_meta;
}

midi_event_t parse_midi_event(FILE *fp, uint8_t first_byte) {
    static uint8_t status = 0x00;
    //yukardan gelen byteı burda sayıyo olabiliriz. birdaha bak
    if((first_byte & 0x80u) && status != first_byte) {
        status = first_byte;
    }

    assert(MIDI_TABLE[status].status != 0);
    midi_event_t midi = MIDI_TABLE[status];
    midi.data = (uint8_t *) malloc(midi.data_len);
    assert(midi.data != NULL);
    uint8_t *ptr = midi.data;
    if(!(first_byte & 0x80u) && midi.data_len > 0)
        *ptr++ = first_byte;
    while(ptr != midi.data + midi.data_len) {
        *ptr++ = (uint8_t) getc(fp);
        ++offset;
    }

    return midi;
}

uint32_t parse_var_len(FILE *fp) {
    uint32_t value;
    uint8_t c;
    value = getc(fp);
    if ((value) & 0x80u)
    {
        value &= 0x7fu;
        do
        {
            value = (value << 7u) + ((c = getc(fp)) & 0x7fu);
        } while (c & 0x80u);
    }
    return value;
}

uint8_t event_type(event_t *test) {
    if(SYS_EVENT_1 == test->type || SYS_EVENT_2 == test->type)
        return SYS_EVENT_T;

    if(META_EVENT == test->type)
        return META_EVENT_T;

    return MIDI_EVENT_T;
}

//  Data manipulation
void free_song(song_data_t *song_data) {
    free(song_data->path);
    free_track_node(song_data->track_list);
    free(song_data);
}

void free_track_node(track_node_t *test) {
    if(test == NULL)
        return;

    free_event_node(test->track->event_list);
    free(test->track);
    free_track_node(test->next_track);
    free(test);
}

void free_event_node(event_node_t *test) {
    if(test == NULL)
        return;

    free_event_node(test->next_event);
    switch (event_type(test->event)) {
        case SYS_EVENT_T:
            if(test->event->sys_event.data_len > 0) {
                free(test->event->sys_event.data);
            }
            break;
        case MIDI_EVENT_T:
            if(test->event->midi_event.data_len > 0) {
                free(test->event->midi_event.data);
            }
            break;
        case META_EVENT_T:
            if(test->event->meta_event.data_len > 0) {
                free(test->event->meta_event.data);
            }
            break;
        default:
            break;
    }
    free(test->event);
    free(test);
}

//  Functions for swapping endian-ness
uint16_t end_swap_16(uint8_t ui8[2]) {
    return ui8[1] | (ui8[0] << 8);
}

uint32_t end_swap_32(uint8_t ui8[4]) {
    return ui8[3] | (ui8[2] << 8) | (ui8[1] << 16) | (ui8[0] << 24);
}
