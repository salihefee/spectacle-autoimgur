#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <limits.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include "cJSON.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

char *upload_image(const char *image_path, const char *client_id);
char *get_image_name(const char *image_path);
size_t get_response(const void *buffer, size_t size, size_t nmemb, void *userp);
char *copy_string_to_clipboard(char *string);
int file_exists(const char *filename);
char *extract_link_from_response(const char *json_string);
void watch_directory(const char *path, char *new_file_path);

struct memory_struct {
    char *memory;
    size_t size;
};

char *extract_link_from_response(const char *json_string) {
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return NULL;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
    const cJSON *link = cJSON_GetObjectItemCaseSensitive(data, "link");
    char *linkString = NULL;
    if (cJSON_IsString(link) && (link->valuestring != NULL)) {
        linkString = strdup(link->valuestring);
    }
    cJSON_Delete(json);
    return linkString;
}

char *copy_string_to_clipboard(char *string) {
    FILE *pipe = popen("wl-copy", "w");
    if (pipe == NULL) {
        perror("popen");
        return NULL;
    }
    fprintf(pipe, "%s", string);
    if (pclose(pipe) == -1) {
        perror("pclose");
        return NULL;
    }
    return string;
}

int file_exists(const char *filename) {
    return access(filename, F_OK) != -1;
}

size_t get_response(const void *buffer, const size_t size, const size_t nmemb, void *userp) {
    const size_t realsize = size * nmemb;
    struct memory_struct *mem = userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), buffer, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

char *upload_image(const char *image_path, const char *client_id) {
    if (!file_exists(image_path)) {
        fprintf(stderr, "File provided does not exist\n");
        return NULL;
    }
    const char *image_name = get_image_name(image_path);
    char *image_link = NULL;
    CURL *handle = curl_easy_init();
    if (handle) {
        struct memory_struct chunk;
        chunk.memory = malloc(1);
        chunk.size = 0;
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(handle, CURLOPT_URL, "https://api.imgur.com/3/image");
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Client-ID %s", client_id);
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_mime *mime = curl_mime_init(handle);
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, "image");
        curl_mime_filedata(part, image_path);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "type");
        curl_mime_data(part, "image", CURL_ZERO_TERMINATED);
        part = curl_mime_addpart(mime);
        // curl_mime_name(part, "title");
        // curl_mime_data(part, image_name, CURL_ZERO_TERMINATED);
        curl_easy_setopt(handle, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_response);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds
        const CURLcode res = curl_easy_perform(handle);
        printf("Response code: %d\n", res);
        curl_mime_free(mime);

        if (res != CURLE_OK) {
            fprintf(stderr, "Image upload failed: %d\n", res);
            curl_easy_cleanup(handle);
            free(chunk.memory);
            return NULL;
        }

        image_link = extract_link_from_response(chunk.memory);
        free(chunk.memory);
    }

    curl_easy_cleanup(handle);
    return image_link;
}

char *get_image_name(const char *image_path) {
    if (image_path == NULL || image_path[0] == '\0') {
        return NULL;
    }

    char *filepath_copy = strdup(image_path);
    if (filepath_copy == NULL) {
        return NULL;
    }

    const char *token = NULL;
    const char *last_token = NULL;
    char *saveptr = NULL;

    token = strtok_r(filepath_copy, "/\\", &saveptr);
    while (token != NULL) {
        last_token = token;
        token = strtok_r(NULL, "/\\", &saveptr);
    }

    char *filename = NULL;
    if (last_token != NULL) {
        filename = strdup(last_token);
    }

    free(filepath_copy);

    return filename;
}

void watch_directory(const char *path, char *new_file_path) {
    int i = 0;
    char buffer[EVENT_BUF_LEN];

    const int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    const int wd = inotify_add_watch(fd, path, IN_CREATE);
    if (wd == -1) {
        fprintf(stderr, "Cannot watch '%s'\n", path);
        exit(EXIT_FAILURE);
    }

    while (1) {
        const int length = read(fd, buffer, EVENT_BUF_LEN);
        if (length < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    if (!(event->mask & IN_ISDIR)) {
                        const char *ext = strrchr(event->name, '.');
                        if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)) {
                            if (path[strlen(path) - 1] == '/') {
                                snprintf(new_file_path, PATH_MAX, "%s%s", path, event->name);
                            } else {
                                snprintf(new_file_path, PATH_MAX, "%s/%s", path, event->name);
                            }
                            inotify_rm_watch(fd, wd);
                            close(fd);
                            return;
                        }
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
        i = 0;
    }
}

int main(int argc, char *argv[]) {
    const char *directory_to_watch = NULL;
    const char *client_id = NULL;
    char default_directory[PATH_MAX];

    if (argc == 2) {
        struct stat path_stat;
        stat(argv[1], &path_stat);
        if (S_ISDIR(path_stat.st_mode)) {
            fprintf(stderr, "Error: Only one argument provided and it is a directory. Please provide a Client ID.\n");
            return EXIT_FAILURE;
        } else {
            client_id = argv[1];
            const char *home_dir = getenv("HOME");
            if (home_dir != NULL) {
                snprintf(default_directory, sizeof(default_directory), "%s/Pictures/Screenshots", home_dir);
                directory_to_watch = default_directory;
            } else {
                fprintf(stderr, "HOME environment variable is not set.\n");
                return EXIT_FAILURE;
            }
        }
    } else if (argc > 2) {
        directory_to_watch = argv[1];
        client_id = argv[2];
    } else {
        fprintf(stderr, "Usage: %s <directory_to_watch> <client_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Screenshot directory set as: %s\n", directory_to_watch);
    printf("Using Client ID: %s\n", client_id);

    curl_global_init(CURL_GLOBAL_SSL);

    char new_file_path[PATH_MAX];

    while (1) {
        watch_directory(directory_to_watch, new_file_path);
        printf("New file created: %s\n", new_file_path);
        sleep(1);
        char *link = upload_image(new_file_path, client_id);
        if (link) {
            printf("Uploaded: %s\n", link);
            const char* clipboard_success = copy_string_to_clipboard(link);
            if (clipboard_success == NULL) {
                fprintf(stderr, "Failed to copy the link to clipboard\n");
            }
        }
        else {
            fprintf(stderr, "Failed to upload to imgur\n");
        }
        free(link);
    }

    return EXIT_SUCCESS;
}