// object.c — Content-addressable object store
//
// Every piece of data (file contents, directory listings, commits) is stored
// as an "object" named by its SHA-256 hash. Objects are stored under
// .pes/objects/XX/YYYYYY... where XX is the first two hex characters of the
// hash (directory sharding).
//
// PROVIDED functions: compute_hash, object_path, object_exists, hash_to_hex, hex_to_hash
// TODO functions:     object_write, object_read

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

// Get the filesystem path where an object should be stored.
// Format: .pes/objects/XX/YYYYYYYY...
// The first 2 hex chars form the shard directory; the rest is the filename.
void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────
// Helper to map enum to string
static const char *type_to_str(ObjectType type) {
    switch (type) {
        case OBJ_BLOB:   return "blob";
        case OBJ_TREE:   return "tree";
        case OBJ_COMMIT: return "commit";
        default:         return NULL;
    }
}

// Helper to map string to enum
static int str_to_type(const char *s, ObjectType *out) {
    if (strncmp(s, "blob", 4) == 0)   { *out = OBJ_BLOB; return 0; }
    if (strncmp(s, "tree", 4) == 0)   { *out = OBJ_TREE; return 0; }
    if (strncmp(s, "commit", 6) == 0) { *out = OBJ_COMMIT; return 0; }
    return -1;
}

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    const char *type_str = type_to_str(type);
    if (!type_str || !data || !id_out) return -1;

    // 1. Build header: "<type> <size>\0"
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len) + 1;

    size_t total_len = header_len + len;
    unsigned char *buf = malloc(total_len);
    if (!buf) return -1;

    memcpy(buf, header, header_len);
    memcpy(buf + header_len, data, len);

    // 2. Compute hash of FULL object
    ObjectID id;
    compute_hash(buf, total_len, &id);

    // 3. Dedup check
    if (object_exists(&id)) {
        free(buf);
        *id_out = id;
        return 0;
    }

    // 4. Build paths
    char path[512];
    object_path(&id, path, sizeof(path));

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/%.2s", OBJECTS_DIR, path + strlen(OBJECTS_DIR) + 1);

    // mkdir -p style (simple version)
    mkdir(OBJECTS_DIR, 0755);
    mkdir(dir, 0755);

    // 5. Temp file path
    char tmp_path[520];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    // 6. Write temp file
    int fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(buf);
        return -1;
    }

    if (write(fd, buf, total_len) != (ssize_t)total_len) {
        close(fd);
        free(buf);
        return -1;
    }

    fsync(fd);
    close(fd);

    // 7. Atomic rename
    if (rename(tmp_path, path) != 0) {
        free(buf);
        return -1;
    }

    // 8. fsync directory
    int dirfd = open(dir, O_DIRECTORY);
    if (dirfd >= 0) {
        fsync(dirfd);
        close(dirfd);
    }

    free(buf);

    // 9. Output hash
    *id_out = id;
    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    if (!id || !type_out || !data_out || !len_out) return -1;

    // 1. Path
    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    // 2. Read full file
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    unsigned char *buf = malloc(file_size);
    if (!buf) {
        fclose(f);
        return -1;
    }

    if (fread(buf, 1, file_size, f) != (size_t)file_size) {
        fclose(f);
        free(buf);
        return -1;
    }
    fclose(f);

    // 3. Verify hash
    ObjectID computed;
    compute_hash(buf, file_size, &computed);

    if (memcmp(&computed, id, sizeof(ObjectID)) != 0) {
        free(buf);
        return -1; // corruption
    }

    // 4. Parse header
    char *null_pos = memchr(buf, '\0', file_size);
    if (!null_pos) {
        free(buf);
        return -1;
    }

    size_t header_len = null_pos - (char *)buf;
    char header[128];
    memcpy(header, buf, header_len);
    header[header_len] = '\0';

    char type_str[16];
    size_t data_size;

    if (sscanf(header, "%15s %zu", type_str, &data_size) != 2) {
        free(buf);
        return -1;
    }

    // 5. Parse type
    if (str_to_type(type_str, type_out) != 0) {
        free(buf);
        return -1;
    }

    // 6. Extract data
    unsigned char *data_start = (unsigned char *)null_pos + 1;

    if ((size_t)(file_size - (data_start - buf)) != data_size) {
        free(buf);
        return -1;
    }

    void *out = malloc(data_size);
    if (!out) {
        free(buf);
        return -1;
    }

    memcpy(out, data_start, data_size);

    *data_out = out;
    *len_out = data_size;

    free(buf);
    return 0;
}

//
// Returns 0 on success, -1 on error.
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    // TODO: Implement
    (void)type; (void)data; (void)len; (void)id_out;
    return -1;
}

// Read an object from the store.
//
// Steps:
//   1. Build the file path from the hash using object_path()
//   2. Open and read the entire file
//   3. Parse the header to extract the type string and size
//   4. Verify integrity: recompute the SHA-256 of the file contents
//      and compare to the expected hash (from *id). Return -1 if mismatch.
//   5. Set *type_out to the parsed ObjectType
//   6. Allocate a buffer, copy the data portion (after the \0), set *data_out and *len_out
//
// HINTS - Useful syscalls and functions for this phase:
//   - object_path        : getting the target file path
//   - fopen, fread, fseek: reading the file into memory
//   - memchr             : safely finding the '\0' separating header and data
//   - strncmp            : parsing the type string ("blob", "tree", "commit")
//   - compute_hash       : re-hashing the read data for integrity verification
//   - memcmp             : comparing the computed hash against the requested hash
//   - malloc, memcpy     : allocating and returning the extracted data
//
// The caller is responsible for calling free(*data_out).
// Returns 0 on success, -1 on error (file not found, corrupt, etc.).
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    // TODO: Implement
    (void)id; (void)type_out; (void)data_out; (void)len_out;
    return -1;
}
