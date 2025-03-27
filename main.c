#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    u_int32_t numInstances;
    u_int32_t numTanks;
    u_int32_t numHealers;
    u_int32_t numDPS;
    u_int32_t totalParties;
    sem_t partyReady;
    u_int8_t minTime;
    u_int8_t maxTime;
} GameState;

int read_config(const char *filename, GameState *state) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return 0;
    }

    char line[50];
    while (fgets(line, 50, file)) {
        char key[50];
        int value;

        if (sscanf(line, "%[^:]: %d", key, &value) == 2) {
            if (strcmp(key, "instances") == 0) state->numInstances = value;
            else if (strcmp(key, "tanks") == 0) state->numTanks = value;
            else if (strcmp(key, "healers") == 0) state->numHealers = value;
            else if (strcmp(key, "dps") == 0) state->numDPS = value;
            else if (strcmp(key, "min time") == 0) state->minTime = value;
            else if (strcmp(key, "max time") == 0) state->maxTime = value;
        }
    }

    fclose(file);
    return 1;
}

void *run_instance(void *arg) {
    GameState *state = (GameState *)arg;
    bool status = false;

    while (1) {
        sem_wait(&state->partyReady);

        status = true;
        int clearTime = rand() % (state->maxTime - state->minTime + 1) + state->minTime;
        printf("Instance active (clearing in %d seconds)\n", clearTime);
        sleep(clearTime);
        printf("Instance finished\n");
        status = false;
    }

    return NULL;
}

void queue_manager(GameState *state) {
    pthread_t threads[state->numInstances];

    // Create fixed threads
    for (int i = 0; i < state->numInstances; i++) {
        pthread_create(&threads[i], NULL, run_instance, state);
    }

    while (state->numTanks >= 1 && state->numHealers >= 1 && state->numDPS >= 3) {
        state->numTanks--;
        state->numHealers--;
        state->numDPS -= 3;
        state->totalParties++;

        printf("Party assigned!\n");

        sem_post(&state->partyReady);
        sleep(1);
    }

    for (int i = 0; i < state->numInstances; i++) {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&state->partyReady);
}

int main() {
    GameState state = {0};

    if (!read_config("config.txt", &state)) {
        return 1;
    }

    state.totalParties = 0;
    sem_init(&state.partyReady, 0, 0);

    srand(time(NULL));

    queue_manager(&state);

    printf("\n=== Summary ===\n");
    printf("Total parties served: %d\n", state.totalParties);

    return 0;
}
