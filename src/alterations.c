
#include "alterations.h"
#include <string.h>
#include <malloc_debug.h>
#include <assert.h>
#include "parser.h"


/*Define change_event_octave here */

static uint32_t given_vlq_byte_length(uint32_t value) {
	if (value > 0x1FFFFF)
		return 4;

	if (value > 0x3FFF)
		return 3;

	if (value > 0x7F)
		return 2;

	return 1;
}

int change_event_octave(event_t* event, int* octave) {
	if (event->type <= 0xAF && event->type >= 0x80) {
		int expected_value = *(event->midi_event.data) + ((*octave) * 12);
		if (expected_value < 0 || expected_value > 127) return 0;
		*(event->midi_event.data) += ((*octave) * 12);
		return 1;
	}
	return 0;
}

/*Define change_event_time here */

int change_event_time(event_t* event, float* time) {
	if (event == NULL) return 0;
	uint32_t vlq_before = given_vlq_byte_length(event->delta_time);
	event->delta_time *= *time;
	uint32_t vlq_after = given_vlq_byte_length(event->delta_time);
	return (int)vlq_after - (int)vlq_before;
}

/*Define change_event_instrument here */

int change_event_instrument(event_t* event, remapping_t table) {
	if (event->type <= 0xCF && event->type >= 0xC0) {
		*event->midi_event.data = *(table + *event->midi_event.data);
		return 1;
	}
	return 0;
}

/*Define change_event_note here */

int change_event_note(event_t* event, remapping_t table) {
	if (event->type <= 0xAF && event->type >= 0x80) {
		*event->midi_event.data = *(table + *event->midi_event.data);
		return 1;
	}
	return 0;
}

/*Define apply_to_events here */

int apply_to_events(song_data_t* song, event_func_t event_func, void* data) {
	if (song == NULL) return 0;
	int sum = 0;
	track_node_t* current_track_node = song->track_list;
	while (current_track_node != NULL) {
		event_node_t* current_event_node = current_track_node->track->event_list;
		while (current_event_node != NULL) {
			sum += event_func(current_event_node->event, data);
			current_event_node = current_event_node->next_event;
		}
		current_track_node = current_track_node->next_track;
	}
	return sum;
}

/*Define change_octave here */

int change_octave(song_data_t* song, int octave) {
	if (song == NULL) return 0;
	int sum = 0;
	track_node_t* current_track_ptr = song->track_list;
	while (current_track_ptr != NULL) {
		event_node_t* current_event_ptr = current_track_ptr->track->event_list;
		while (current_event_ptr != NULL) {
			sum += change_event_octave(current_event_ptr->event, &octave);
			current_event_ptr = current_event_ptr->next_event;
		}
		current_track_ptr = current_track_ptr->next_track;
	}
	return sum;
}
/*Define warp_time here */

int warp_time(song_data_t* song, float time) {
	if (song == NULL) return 0;
	int sum = 0;
	track_node_t* current_track_ptr = song->track_list;
	while (current_track_ptr != NULL) {
		event_node_t* current_event_ptr = current_track_ptr->track->event_list;
		while (current_event_ptr != NULL) {
			int val;
			val = change_event_time(current_event_ptr->event, &time);
			sum += val;
			current_track_ptr->track->length += val;
			current_event_ptr = current_event_ptr->next_event;
		}
		current_track_ptr = current_track_ptr->next_track;
	}
	return sum;
}

/*Define remap_instruments here */

int remap_instruments(song_data_t* song, remapping_t table) {
	if (song == NULL) return 0;
	int sum = 0;
	track_node_t* current_track_ptr = song->track_list;
	while (current_track_ptr != NULL) {
		event_node_t* current_event_ptr = current_track_ptr->track->event_list;
		while (current_event_ptr != NULL) {
			sum += change_event_instrument(current_event_ptr->event, table);
			current_event_ptr = current_event_ptr->next_event;
		}
		current_track_ptr = current_track_ptr->next_track;
	}
	return sum;
}

/*Define remap_notes here */

int remap_notes(song_data_t* song, remapping_t table) {
	if (song == NULL) return 0;
	int sum = 0;
	track_node_t* current_track_ptr = song->track_list;
	while (current_track_ptr != NULL) {
		event_node_t* current_event_ptr = current_track_ptr->track->event_list;
		while (current_event_ptr != NULL) {
			sum += change_event_note(current_event_ptr->event, table);
			current_event_ptr = current_event_ptr->next_event;
		}
		current_track_ptr = current_track_ptr->next_track;
	}
	return sum;
}

/*Define add_round here */

void
add_round(song_data_t* song,
          int track_index,
          int octave_diff,
          unsigned int time_delay,
          uint8_t instrument) {
    assert(song != NULL);
    int current_track_index = 0;
    track_node_t* current_track_ptr = song->track_list;
    while (current_track_ptr != NULL && current_track_index < track_index) {
        current_track_ptr = current_track_ptr->next_track;
        ++current_track_index;
    }
    assert(current_track_index == track_index);
    assert(current_track_ptr != NULL);
    assert(song->format != 2);
    track_node_t* new_track = (track_node_t*)malloc(sizeof(track_node_t));
    new_track->track = (track_t*)malloc(sizeof(track_t));
    new_track->track->event_list = (event_node_t*)malloc(sizeof(event_node_t));
    new_track->track->event_list->event = (event_t*)malloc(sizeof(event_t));
    new_track->track->event_list->next_event = NULL;
    new_track->track->length = current_track_ptr->track->length;
    new_track->next_track = NULL;
    memcpy(new_track->track->event_list->event, current_track_ptr->track->event_list->event, sizeof(event_t));
    switch(event_type(new_track->track->event_list->event)) {
        case SYS_EVENT_T:
            if(current_track_ptr->track->event_list->event->sys_event.data_len > 0) {
                new_track->track->event_list->event->sys_event.data = (uint8_t *) malloc(
                        current_track_ptr->track->event_list->event->sys_event.data_len);
                assert(new_track->track->event_list->event->sys_event.data != NULL);
                memcpy(new_track->track->event_list->event->sys_event.data,
                       current_track_ptr->track->event_list->event->sys_event.data,
                       current_track_ptr->track->event_list->event->sys_event.data_len);
            }
            break;
        case MIDI_EVENT_T:
            if(current_track_ptr->track->event_list->event->midi_event.data_len > 0) {
                new_track->track->event_list->event->midi_event.data = (uint8_t *) malloc(
                        current_track_ptr->track->event_list->event->midi_event.data_len);
                assert(new_track->track->event_list->event->midi_event.data != NULL);
                memcpy(new_track->track->event_list->event->midi_event.data,
                       current_track_ptr->track->event_list->event->midi_event.data,
                       current_track_ptr->track->event_list->event->midi_event.data_len);
            }
            break;
        case META_EVENT_T:
            if(current_track_ptr->track->event_list->event->meta_event.data_len > 0) {
                new_track->track->event_list->event->meta_event.data = (uint8_t *) malloc(
                        current_track_ptr->track->event_list->event->meta_event.data_len);
                assert(new_track->track->event_list->event->meta_event.data != NULL);
                memcpy(new_track->track->event_list->event->meta_event.data,
                       current_track_ptr->track->event_list->event->meta_event.data,
                       current_track_ptr->track->event_list->event->meta_event.data_len);
            }
            break;
    }


    event_node_t* event_node = new_track->track->event_list;
    event_node_t* current_event_node = current_track_ptr->track->event_list;
    while (current_event_node->next_event != NULL) {
        event_node->next_event = (event_node_t*)malloc(sizeof(event_node_t));
        assert(event_node->next_event != NULL);
        event_node->next_event->event = (event_t*)malloc(sizeof(event_t));
        assert(event_node->next_event->event != NULL);
        memcpy(event_node->next_event->event, current_event_node->next_event->event, sizeof(event_t));
        switch(event_type(event_node->next_event->event)) {
            case SYS_EVENT_T:
                if(current_event_node->next_event->event->sys_event.data_len > 0) {
                    event_node->next_event->event->sys_event.data = (uint8_t *) malloc(
                            current_event_node->next_event->event->sys_event.data_len);
                    assert(event_node->next_event->event->sys_event.data != NULL);
                    memcpy(event_node->next_event->event->sys_event.data,
                           current_event_node->next_event->event->sys_event.data,
                           current_event_node->next_event->event->sys_event.data_len);
                }
                break;
            case MIDI_EVENT_T:
                if(current_event_node->next_event->event->midi_event.data_len > 0) {
                    event_node->next_event->event->midi_event.data = (uint8_t *) malloc(
                            current_event_node->next_event->event->midi_event.data_len);
                    assert(event_node->next_event->event->midi_event.data != NULL);
                    memcpy(event_node->next_event->event->midi_event.data,
                           current_event_node->next_event->event->midi_event.data,
                           current_event_node->next_event->event->midi_event.data_len);

                }
                break;
            case META_EVENT_T:
                if(current_event_node->next_event->event->meta_event.data_len > 0) {
                    event_node->next_event->event->meta_event.data = (uint8_t *) malloc(
                            current_event_node->next_event->event->meta_event.data_len);
                    assert(event_node->next_event->event->meta_event.data != NULL);
                    memcpy(event_node->next_event->event->meta_event.data,
                           current_event_node->next_event->event->meta_event.data,
                           current_event_node->next_event->event->meta_event.data_len);
                }
                break;
        }
        event_node->next_event->next_event = NULL;
        event_node = event_node->next_event;
        current_event_node = current_event_node->next_event;
    }
    event_node = new_track->track->event_list;
    uint32_t vlq_before = given_vlq_byte_length(event_node->event->delta_time);
    event_node->event->delta_time += time_delay;
    uint32_t vlq_after = given_vlq_byte_length(event_node->event->delta_time);
    current_track_ptr->track->length += (int)vlq_after - (int)vlq_before;
    while (event_node != NULL) {
        change_event_octave(event_node->event, &octave_diff);
        event_node = event_node->next_event;
    }
    track_node_t* list = current_track_ptr;
    while (list->next_track != NULL) {
        list = list->next_track;
    }

    int channels[16] = {0};
    track_node_t* my_track_ptr = song->track_list;
    int count = 0;
    while(my_track_ptr != NULL) {
        event_node_t* my_event_ptr = my_track_ptr->track->event_list;
        while(my_event_ptr != NULL) {
            if (my_event_ptr->event->midi_event.status <= 0xEF && my_event_ptr->event->midi_event.status >= 0x80) {
                channels[my_event_ptr->event->midi_event.status & 0x0Fu] = 1;
            }
            my_event_ptr = my_event_ptr->next_event;
        }
        ++count;
        my_track_ptr = my_track_ptr->next_track;
    }
    size_t available_channel = 0;
    for(size_t i = 0; i < 16; ++i) {
        if(channels[i] == 0) {
            available_channel = i;
            break;
        }
    }
    assert(available_channel != 0);
    track_node_t* test_node = song->track_list;
    int cnt_node = 0;
    while(test_node != NULL) {
        ++cnt_node;
        test_node = test_node->next_track;
    }
    event_node_t* my_event_ptr = new_track->track->event_list;
    int event_cnt = 0;
    while(my_event_ptr != NULL) {
        if (my_event_ptr->event->midi_event.status <= 0xEF && my_event_ptr->event->midi_event.status >= 0x80) {
            my_event_ptr->event->midi_event.status &= 0xF0u;
            my_event_ptr->event->type &= 0xF0u;
            my_event_ptr->event->midi_event.status |= available_channel;
            my_event_ptr->event->type |= available_channel;
        }
        if (my_event_ptr->event->midi_event.status <= 0xCF && my_event_ptr->event->midi_event.status >= 0xC0) {
            *my_event_ptr->event->midi_event.data = instrument;
        }
        ++event_cnt;
        my_event_ptr = my_event_ptr->next_event;
    }
    list->next_track = new_track;
    ++song->num_tracks;
}

/*
 * Function called prior to main that sets up random mapping tables
 */

void build_mapping_tables()
{
	for (int i = 0; i <= 0xFF; i++) {
		I_BRASS_BAND[i] = 61;
	}

	for (int i = 0; i <= 0xFF; i++) {
		I_HELICOPTER[i] = 125;
	}

	for (int i = 0; i <= 0xFF; i++) {
		N_LOWER[i] = i;
	}
	//  Swap C# for C
	for (int i = 1; i <= 0xFF; i += 12) {
		N_LOWER[i] = i - 1;
	}
	//  Swap F# for G
	for (int i = 6; i <= 0xFF; i += 12) {
		N_LOWER[i] = i + 1;
	}
} /* build_mapping_tables() */
