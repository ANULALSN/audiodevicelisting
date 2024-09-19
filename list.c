#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

// Structure to store audio device information
typedef struct {
    char name[256];
    char path[256];
    char pcm_name[256];
    char stream_type[256];  // Playback or Capture
    char device_type[256];  // Headphones, Speaker, or Microphone
} audio_device_t;

// Function to classify devices as "Speaker", "Headphones", or "Microphone"
void classify_device(audio_device_t *device) {
    if (strcmp(device->stream_type, "Playback") == 0) {
        if (strstr(device->pcm_name, "Headphones") != NULL) {
            snprintf(device->device_type, sizeof(device->device_type), "Headphones");
        } else {
            snprintf(device->device_type, sizeof(device->device_type), "Speaker");
        }
    } else if (strcmp(device->stream_type, "Capture") == 0) {
        snprintf(device->device_type, sizeof(device->device_type), "Microphone");
    }
}

// Function to list audio devices using ALSA
int list_audio_devices(audio_device_t *device_list) {
    int card_num = -1, device_index = 0;
    snd_ctl_t *handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t *pcm_info;

    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcm_info);

    // Iterate over all sound cards
    while (snd_card_next(&card_num) >= 0 && card_num >= 0) {
        char card_name[32];
        sprintf(card_name, "hw:%d", card_num);

        if (snd_ctl_open(&handle, card_name, 0) < 0) continue;  // Open sound card
        if (snd_ctl_card_info(handle, info) < 0) { snd_ctl_close(handle); continue; }  // Get card info

        // Iterate over both Playback and Capture devices
        for (int stream = SND_PCM_STREAM_PLAYBACK; stream <= SND_PCM_STREAM_CAPTURE; stream++) {
            const char *stream_type = (stream == SND_PCM_STREAM_PLAYBACK) ? "Playback" : "Capture";

            // Iterate over PCM devices on the card
            int device = -1;
            while (snd_ctl_pcm_next_device(handle, &device) >= 0 && device >= 0) {
                snd_pcm_info_set_device(pcm_info, device);
                snd_pcm_info_set_subdevice(pcm_info, 0);
                snd_pcm_info_set_stream(pcm_info, stream);  // Set the stream type (Playback or Capture)

                if (snd_ctl_pcm_info(handle, pcm_info) >= 0) {
                    const char *pcm_name = snd_pcm_info_get_name(pcm_info);
                    snprintf(device_list[device_index].pcm_name, sizeof(device_list[device_index].pcm_name), "%s", pcm_name);
                    snprintf(device_list[device_index].stream_type, sizeof(device_list[device_index].stream_type), "%s", stream_type);
                    
                    snprintf(device_list[device_index].name, sizeof(device_list[device_index].name),
                             "Card %d Device %d", card_num, device);
                    snprintf(device_list[device_index].path, sizeof(device_list[device_index].path), "%s", card_name);

                    // Classify the device as Speaker, Headphones, or Microphone
                    classify_device(&device_list[device_index]);

                    device_index++;  // Store device info and increment index
                }
            }
        }

        snd_ctl_close(handle);  // Close sound card control interface
    }

    return device_index;  // Return number of devices found
}

// Function to print the list of audio devices
void print_audio_devices(audio_device_t *device_list, int num_devices) {
    if (num_devices == 0) {
        printf("No audio devices found.\n");
    } else {
        for (int i = 0; i < num_devices; i++) {
            printf("Device Name: %s, Path: %s, PCM: %s, Stream: %s, Type: %s\n",
                   device_list[i].name, device_list[i].path, device_list[i].pcm_name, device_list[i].stream_type,
                   device_list[i].device_type);
        }
    }
}

// Function to monitor ACPI events and handle headphone/microphone detection
void monitor_acpi_events() {
    FILE *acpi_listen;
    char event[512];

    // Open a pipe to acpi_listen
    acpi_listen = popen("acpi_listen", "r");
    if (acpi_listen == NULL) {
        perror("Failed to run acpi_listen");
        exit(1);
    }

    // Continuously read events from acpi_listen
    while (fgets(event, sizeof(event), acpi_listen) != NULL) {
        // Print the full event for debugging purposes
        printf("ACPI Event: %s", event);

        // Check if the event relates to the headphone jack
        if (strstr(event, "jack/headphone")) {
            // Check for the exact word "plug" or "unplug"
            if (strstr(event, " HEADPHONE plug")) {
                printf("Headphones plugged in.\n");
            } else if (strstr(event, " HEADPHONE unplug")) {
                printf("Headphones unplugged.\n");
            }
        }
        // Check if the event relates to the microphone jack
        else if (strstr(event, "jack/microphone")) {
            if (strstr(event, " MICROPHONE plug")) {
                printf("Microphone plugged in.\n");
            } else if (strstr(event, " MICROPHONE unplug")) {
                printf("Microphone unplugged.\n");
            }
        } else {
            // Handle any other jack-related events (for debugging)
            printf("Other jack event detected: %s\n", event);
        }
    }

    // Close the pipe when done
    pclose(acpi_listen);
}

int main() {
    // Step 1: List all current audio devices
    audio_device_t device_list[10];  // Max 10 devices
    int num_devices = list_audio_devices(device_list);
    print_audio_devices(device_list, num_devices);

    // Step 2: Monitor for real-time connection/disconnection events
    printf("Monitoring for real-time audio device events...\n");
    monitor_acpi_events();

    return 0;
}
