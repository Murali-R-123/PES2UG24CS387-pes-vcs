// index.c — Staging area implementation

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

// Find an index entry by path (linear scan).
IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// Remove a file from the index.
int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

// Status function (unchanged)
int index_status(const Index *index) {
    printf("Staged changes:\n");
    int staged_count = 0;

    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        staged_count++;
    }
    if (staged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged_count = 0;

    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged_count++;
        } else {
            if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                st.st_size != (off_t)index->entries[i].size) {
                printf("  modified:   %s\n", index->entries[i].path);
                unstaged_count++;
            }
        }
    }

    if (unstaged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    int untracked_count = 0;

    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0 ||
                strcmp(ent->d_name, ".pes") == 0 ||
                strcmp(ent->d_name, "pes") == 0 ||
                strstr(ent->d_name, ".o") != NULL)
                continue;

            int tracked = 0;
            for (int i = 0; i < index->count; i++) {
                if (strcmp(index->entries[i].path, ent->d_name) == 0) {
                    tracked = 1;
                    break;
                }
            }

            if (!tracked) {
                struct stat st;
                stat(ent->d_name, &st);
                if (S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked_count++;
                }
            }
        }
        closedir(dir);
    }

    if (untracked_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    return 0;
}

// ─── IMPLEMENTATION ─────────────────────────────────────────────────────────

// Load index from file
int index_load(Index *index) {
    FILE *fp = fopen(".pes/index", "r");

    index->entries = NULL;
    index->count = 0;

    if (!fp) return 0; // no index yet

    char hash_hex[65];
    char path[1024];
    unsigned int mode;
    long mtime, size;

    while (fscanf(fp, "%o %64s %ld %ld %1023s\n",
                  &mode, hash_hex, &mtime, &size, path) == 5) {

        index->entries = realloc(index->entries,
                                 (index->count + 1) * sizeof(IndexEntry));

        IndexEntry *e = &index->entries[index->count];

        e->mode = mode;
        e->mtime_sec = (time_t)mtime;
        e->size = (off_t)size;
        strncpy(e->path, path, sizeof(e->path) - 1);
        e->path[sizeof(e->path) - 1] = '\0';

        hex_to_hash(hash_hex, &e->oid);

        index->count++;
    }

    fclose(fp);
    return 0;
}

// Comparator for sorting
static int cmp_entries(const void *a, const void *b) {
    return strcmp(((IndexEntry*)a)->path, ((IndexEntry*)b)->path);
}

// Save index atomically
int index_save(const Index *index) {
    FILE *fp = fopen(".pes/index.tmp", "w");
    if (!fp) return -1;

    qsort((void *)index->entries, index->count,
          sizeof(IndexEntry), cmp_entries);

    for (int i = 0; i < index->count; i++) {
        char hash_hex[65];
        hash_to_hex(&index->entries[i].oid, hash_hex);

        fprintf(fp, "%o %s %ld %ld %s\n",
                index->entries[i].mode,
                hash_hex,
                (long)index->entries[i].mtime_sec,
                (long)index->entries[i].size,
                index->entries[i].path);
    }

    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    if (rename(".pes/index.tmp", ".pes/index") != 0) {
        perror("rename");
        return -1;
    }

    return 0;
}

// Add file to index
int index_add(Index *index, const char *path) {
    struct stat st;

    if (stat(path, &st) != 0) {
        perror("stat");
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "error: '%s' is not a regular file\n", path);
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    void *buffer = malloc(st.st_size);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    fread(buffer, 1, st.st_size, fp);
    fclose(fp);

    ObjectID oid;
    if (object_write(OBJ_BLOB, buffer, st.st_size, &oid) != 0) {
        free(buffer);
        return -1;
    }

    free(buffer);

    IndexEntry *e = index_find(index, path);

    if (e) {
        e->oid = oid;
        e->mtime_sec = st.st_mtime;
        e->size = st.st_size;
        e->mode = st.st_mode;
    } else {
        index->entries = realloc(index->entries,
                                 (index->count + 1) * sizeof(IndexEntry));

        e = &index->entries[index->count];

        e->oid = oid;
        e->mtime_sec = st.st_mtime;
        e->size = st.st_size;
        e->mode = st.st_mode;

        strncpy(e->path, path, sizeof(e->path) - 1);
        e->path[sizeof(e->path) - 1] = '\0';

        index->count++;
    }

    return index_save(index);
}
