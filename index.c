#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int object_write(int type, const void *data, size_t len, ObjectID *id_out);

// ─── FIND ─────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// ─── LOAD ─────────────────────────────────────────

int index_load(Index *index) {

    // 🔥 CRITICAL FIX — initialize entire struct
    memset(index, 0, sizeof(Index));

    FILE *f = fopen(".pes/index", "r");
    if (!f) return 0;

    while (1) {
        IndexEntry *e = &index->entries[index->count];

        char hash_hex[65];

        if (fscanf(f, "%o %64s %ld %ld %s",
                   &e->mode,
                   hash_hex,
                   &e->mtime_sec,
                   &e->size,
                   e->path) != 5)
            break;

        hex_to_hash(hash_hex, &e->hash);
        index->count++;
    }

    fclose(f);
    return 0;
}

// ─── SAVE ─────────────────────────────────────────

int compare_index_entries(const void *a, const void *b) {
    return strcmp(((IndexEntry *)a)->path, ((IndexEntry *)b)->path);
}

int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    Index temp = *index;
    qsort(temp.entries, temp.count, sizeof(IndexEntry), compare_index_entries);

    for (int i = 0; i < temp.count; i++) {
        char hash_hex[65];
        hash_to_hex(&temp.entries[i].hash, hash_hex);

        fprintf(f, "%o %s %ld %ld %s\n",
                temp.entries[i].mode,
                hash_hex,
                temp.entries[i].mtime_sec,
                temp.entries[i].size,
                temp.entries[i].path);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    rename(".pes/index.tmp", ".pes/index");
    return 0;
}

// ─── ADD ─────────────────────────────────────────

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    void *data = NULL;

    if (size > 0) {
        data = malloc(size);
        fread(data, 1, size, f);
    }

    fclose(f);

    ObjectID hash;
    object_write(1, data, size, &hash);

    free(data);

    struct stat st;
    stat(path, &st);

    IndexEntry *e = index_find(index, path);

    if (!e) {
        e = &index->entries[index->count++];
    }

    e->mode = st.st_mode;
    e->hash = hash;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    strcpy(e->path, path);

    return index_save(index);
}

// ─── STATUS ───────────────────────────────────────

int index_status(const Index *index) {
    printf("Staged changes:\n");
    for (int i = 0; i < index->count; i++) {
        printf("  staged: %s\n", index->entries[i].path);
    }
    if (index->count == 0) printf("  (nothing to show)\n\n");

    return 0;
}