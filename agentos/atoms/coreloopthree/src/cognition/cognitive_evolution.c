#include "cognitive_evolution.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct cog_evolution_s {
    cog_level_t current_level;
    cog_experience_t* experiences;
    size_t experience_count;
    size_t experience_capacity;
    cog_strategy_t* strategies;
    size_t strategy_count;
    size_t strategy_capacity;
    cog_pattern_t* patterns;
    size_t pattern_count;
    size_t pattern_capacity;
    cog_knowledge_t* knowledge;
    size_t knowledge_count;
    size_t knowledge_capacity;
    cog_reward_fn reward_fn;
    void* reward_user_data;
    cog_level_change_fn level_change_fn;
    void* level_change_user_data;
    double total_fitness;
    double level_thresholds[5];
};

static char* json_strdup(const char* src) {
    if (!src) return NULL;
    return strdup(src);
}

cog_evolution_t* cog_evolution_create(cog_level_t initial_level) {
    cog_evolution_t* evo = (cog_evolution_t*)calloc(1, sizeof(cog_evolution_t));
    if (!evo) return NULL;

    evo->current_level = initial_level > COG_LEVEL_CREATION ? COG_LEVEL_CREATION : initial_level;

    evo->experience_capacity = 256;
    evo->experiences = (cog_experience_t*)calloc(evo->experience_capacity, sizeof(cog_experience_t));
    if (!evo->experiences) { free(evo); return NULL; }

    evo->strategy_capacity = 64;
    evo->strategies = (cog_strategy_t*)calloc(evo->strategy_capacity, sizeof(cog_strategy_t));
    if (!evo->strategies) { free(evo->experiences); free(evo); return NULL; }

    evo->pattern_capacity = 256;
    evo->patterns = (cog_pattern_t*)calloc(evo->pattern_capacity, sizeof(cog_pattern_t));
    if (!evo->patterns) { free(evo->strategies); free(evo->experiences); free(evo); return NULL; }

    evo->knowledge_capacity = 128;
    evo->knowledge = (cog_knowledge_t*)calloc(evo->knowledge_capacity, sizeof(cog_knowledge_t));
    if (!evo->knowledge) { free(evo->patterns); free(evo->strategies); free(evo->experiences); free(evo); return NULL; }

    evo->level_thresholds[COG_LEVEL_PERCEPTION] = 0.0;
    evo->level_thresholds[COG_LEVEL_REACTION] = 0.2;
    evo->level_thresholds[COG_LEVEL_LEARNING] = 0.4;
    evo->level_thresholds[COG_LEVEL_REASONING] = 0.7;
    evo->level_thresholds[COG_LEVEL_CREATION] = 0.9;

    evo->reward_fn = NULL;
    evo->level_change_fn = NULL;
    evo->total_fitness = 0.0;

    return evo;
}

static void free_experience(cog_experience_t* e) {
    if (!e) return;
    if (e->context_json) free(e->context_json);
    if (e->action_json) free(e->action_json);
    if (e->outcome_json) free(e->outcome_json);
    e->context_json = NULL;
    e->action_json = NULL;
    e->outcome_json = NULL;
}

static void free_strategy(cog_strategy_t* s) {
    if (!s) return;
    if (s->condition_json) free(s->condition_json);
    if (s->action_json) free(s->action_json);
    s->condition_json = NULL;
    s->action_json = NULL;
}

static void free_pattern(cog_pattern_t* p) {
    if (!p) return;
    if (p->trigger_json) free(p->trigger_json);
    if (p->response_json) free(p->response_json);
    p->trigger_json = NULL;
    p->response_json = NULL;
}

static void free_knowledge(cog_knowledge_t* k) {
    if (!k) return;
    if (k->knowledge_json) free(k->knowledge_json);
    if (k->adaptation_json) free(k->adaptation_json);
    k->knowledge_json = NULL;
    k->adaptation_json = NULL;
}

void cog_evolution_destroy(cog_evolution_t* evo) {
    if (!evo) return;
    if (evo->experiences) {
        for (size_t i = 0; i < evo->experience_count; i++) free_experience(&evo->experiences[i]);
        free(evo->experiences);
    }
    if (evo->strategies) {
        for (size_t i = 0; i < evo->strategy_count; i++) free_strategy(&evo->strategies[i]);
        free(evo->strategies);
    }
    if (evo->patterns) {
        for (size_t i = 0; i < evo->pattern_count; i++) free_pattern(&evo->patterns[i]);
        free(evo->patterns);
    }
    if (evo->knowledge) {
        for (size_t i = 0; i < evo->knowledge_count; i++) free_knowledge(&evo->knowledge[i]);
        free(evo->knowledge);
    }
    free(evo);
}

int cog_evolution_record_experience(cog_evolution_t* evo, const cog_experience_t* experience) {
    if (!evo || !experience) return -1;

    if (evo->experience_count >= COG_EVO_MAX_EXPERIENCES) {
        free_experience(&evo->experiences[0]);
        memmove(evo->experiences, evo->experiences + 1,
                (evo->experience_count - 1) * sizeof(cog_experience_t));
        evo->experience_count--;
    }

    if (evo->experience_count >= evo->experience_capacity) {
        size_t new_cap = evo->experience_capacity * 2;
        if (new_cap > COG_EVO_MAX_EXPERIENCES) new_cap = COG_EVO_MAX_EXPERIENCES;
        cog_experience_t* new_arr = (cog_experience_t*)realloc(evo->experiences, new_cap * sizeof(cog_experience_t));
        if (!new_arr) return -2;
        evo->experiences = new_arr;
        evo->experience_capacity = new_cap;
    }

    cog_experience_t* slot = &evo->experiences[evo->experience_count];
    memcpy(slot, experience, sizeof(cog_experience_t));
    slot->context_json = json_strdup(experience->context_json);
    slot->action_json = json_strdup(experience->action_json);
    slot->outcome_json = json_strdup(experience->outcome_json);

    if (evo->reward_fn) {
        slot->reward = evo->reward_fn(experience, evo->reward_user_data);
    }

    evo->total_fitness += slot->reward;
    evo->experience_count++;

    return 0;
}

static double compute_pattern_confidence(const cog_experience_t* experiences, size_t count,
                                          const char* trigger, const char* response) {
    if (!trigger || !response || count == 0) return 0.0;
    size_t match_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (experiences[i].action_json && strstr(experiences[i].action_json, trigger) &&
            experiences[i].outcome_json && strstr(experiences[i].outcome_json, response)) {
            match_count++;
        }
    }
    return count > 0 ? (double)match_count / (double)count : 0.0;
}

int cog_evolution_extract_patterns(cog_evolution_t* evo, size_t* pattern_count) {
    if (!evo) return -1;

    for (size_t i = 0; i < evo->experience_count && evo->pattern_count < COG_EVO_MAX_PATTERNS; i++) {
        const cog_experience_t* exp = &evo->experiences[i];
        if (!exp->action_json || !exp->outcome_json) continue;

        int found = 0;
        for (size_t j = 0; j < evo->pattern_count; j++) {
            if (evo->patterns[j].trigger_json && exp->action_json &&
                strstr(evo->patterns[j].trigger_json, exp->action_json)) {
                evo->patterns[j].occurrence_count++;
                evo->patterns[j].last_seen = exp->timestamp;
                evo->patterns[j].confidence = compute_pattern_confidence(
                    evo->experiences, evo->experience_count,
                    evo->patterns[j].trigger_json, evo->patterns[j].response_json);
                found = 1;
                break;
            }
        }

        if (!found) {
            if (evo->pattern_count >= evo->pattern_capacity) {
                size_t new_cap = evo->pattern_capacity * 2;
                if (new_cap > COG_EVO_MAX_PATTERNS) new_cap = COG_EVO_MAX_PATTERNS;
                cog_pattern_t* new_arr = (cog_pattern_t*)realloc(evo->patterns, new_cap * sizeof(cog_pattern_t));
                if (!new_arr) break;
                evo->patterns = new_arr;
                evo->pattern_capacity = new_cap;
            }

            cog_pattern_t* p = &evo->patterns[evo->pattern_count];
            memset(p, 0, sizeof(cog_pattern_t));
            snprintf(p->id, sizeof(p->id), "pat_%zu", evo->pattern_count);
            snprintf(p->pattern_type, sizeof(p->pattern_type), "action_outcome");
            p->trigger_json = json_strdup(exp->action_json);
            p->response_json = json_strdup(exp->outcome_json);
            p->confidence = 1.0 / (double)(evo->experience_count > 0 ? evo->experience_count : 1);
            p->occurrence_count = 1;
            p->last_seen = exp->timestamp;
            evo->pattern_count++;
        }
    }

    if (pattern_count) *pattern_count = evo->pattern_count;
    return 0;
}

int cog_evolution_evolve_strategies(cog_evolution_t* evo, size_t* strategy_count) {
    if (!evo) return -1;

    for (size_t i = 0; i < evo->pattern_count && evo->strategy_count < COG_EVO_MAX_STRATEGIES; i++) {
        cog_pattern_t* pat = &evo->patterns[i];
        if (pat->confidence < 0.3) continue;

        int found = 0;
        for (size_t j = 0; j < evo->strategy_count; j++) {
            if (evo->strategies[j].condition_json && pat->trigger_json &&
                strcmp(evo->strategies[j].condition_json, pat->trigger_json) == 0) {
                evo->strategies[j].fitness = pat->confidence;
                evo->strategies[j].use_count = pat->occurrence_count;
                evo->strategies[j].last_used = pat->last_seen;
                found = 1;
                break;
            }
        }

        if (!found) {
            if (evo->strategy_count >= evo->strategy_capacity) {
                size_t new_cap = evo->strategy_capacity * 2;
                if (new_cap > COG_EVO_MAX_STRATEGIES) new_cap = COG_EVO_MAX_STRATEGIES;
                cog_strategy_t* new_arr = (cog_strategy_t*)realloc(evo->strategies, new_cap * sizeof(cog_strategy_t));
                if (!new_arr) break;
                evo->strategies = new_arr;
                evo->strategy_capacity = new_cap;
            }

            cog_strategy_t* s = &evo->strategies[evo->strategy_count];
            memset(s, 0, sizeof(cog_strategy_t));
            snprintf(s->id, sizeof(s->id), "strat_%zu", evo->strategy_count);
            snprintf(s->name, sizeof(s->name), "evolved_%zu", evo->strategy_count);
            snprintf(s->domain, sizeof(s->domain), "general");
            s->condition_json = json_strdup(pat->trigger_json);
            s->action_json = json_strdup(pat->response_json);
            s->fitness = pat->confidence;
            s->use_count = pat->occurrence_count;
            s->success_count = (uint64_t)(pat->occurrence_count * pat->confidence);
            s->last_used = pat->last_seen;
            s->min_level = evo->current_level;
            evo->strategy_count++;
        }
    }

    for (size_t i = 0; i < evo->strategy_count; i++) {
        cog_strategy_t* s = &evo->strategies[i];
        if (s->use_count > 0) {
            double success_rate = (double)s->success_count / (double)s->use_count;
            double decay = exp(-0.01 * (double)s->use_count);
            s->fitness = success_rate * (1.0 - decay) + s->fitness * decay;
        }
    }

    if (strategy_count) *strategy_count = evo->strategy_count;
    return 0;
}

int cog_evolution_select_strategy(cog_evolution_t* evo,
                                    const char* domain,
                                    const char* context_json,
                                    cog_strategy_t** strategy) {
    if (!evo || !strategy) return -1;

    double best_fitness = -1.0;
    cog_strategy_t* best = NULL;

    for (size_t i = 0; i < evo->strategy_count; i++) {
        cog_strategy_t* s = &evo->strategies[i];
        if (domain && s->domain[0] != '\0' && strcmp(s->domain, domain) != 0) continue;
        if (s->min_level > evo->current_level) continue;

        double match_score = s->fitness;
        if (context_json && s->condition_json) {
            if (strstr(context_json, s->condition_json) || strstr(s->condition_json, context_json)) {
                match_score *= 1.5;
            }
        }
        match_score *= (1.0 + 0.1 * log1p((double)s->success_count));

        if (match_score > best_fitness) {
            best_fitness = match_score;
            best = s;
        }
    }

    *strategy = best;
    return best ? 0 : 1;
}

int cog_evolution_transfer_knowledge(cog_evolution_t* evo,
                                       const char* source_domain,
                                       const char* target_domain,
                                       cog_knowledge_t** knowledge) {
    if (!evo || !source_domain || !target_domain || !knowledge) return -1;

    double best_score = 0.0;
    cog_knowledge_t* best = NULL;

    for (size_t i = 0; i < evo->knowledge_count; i++) {
        cog_knowledge_t* k = &evo->knowledge[i];
        if (strcmp(k->source_domain, source_domain) == 0 &&
            strcmp(k->target_domain, target_domain) == 0) {
            if (k->transfer_score > best_score) {
                best_score = k->transfer_score;
                best = k;
            }
        }
    }

    if (!best) {
        for (size_t i = 0; i < evo->strategy_count; i++) {
            cog_strategy_t* s = &evo->strategies[i];
            if (strcmp(s->domain, source_domain) == 0 && s->fitness > 0.5) {
                if (evo->knowledge_count >= evo->knowledge_capacity) {
                    size_t new_cap = evo->knowledge_capacity * 2;
                    if (new_cap > COG_EVO_MAX_KNOWLEDGE) new_cap = COG_EVO_MAX_KNOWLEDGE;
                    cog_knowledge_t* new_arr = (cog_knowledge_t*)realloc(
                        evo->knowledge, new_cap * sizeof(cog_knowledge_t));
                    if (!new_arr) break;
                    evo->knowledge = new_arr;
                    evo->knowledge_capacity = new_cap;
                }

                cog_knowledge_t* k = &evo->knowledge[evo->knowledge_count];
                memset(k, 0, sizeof(cog_knowledge_t));
                snprintf(k->id, sizeof(k->id), "kn_%zu", evo->knowledge_count);
                strncpy(k->source_domain, source_domain, sizeof(k->source_domain) - 1);
                strncpy(k->target_domain, target_domain, sizeof(k->target_domain) - 1);
                k->knowledge_json = json_strdup(s->action_json);
                k->adaptation_json = json_strdup(s->condition_json);
                k->transfer_score = s->fitness * 0.7;
                k->validated = false;
                evo->knowledge_count++;

                if (k->transfer_score > best_score) {
                    best_score = k->transfer_score;
                    best = k;
                }
            }
        }
    }

    *knowledge = best;
    return best ? 0 : 1;
}

cog_level_t cog_evolution_get_level(cog_evolution_t* evo) {
    if (!evo) return COG_LEVEL_PERCEPTION;
    return evo->current_level;
}

int cog_evolution_evaluate_level(cog_evolution_t* evo, cog_level_t* new_level) {
    if (!evo || !new_level) return -1;

    double avg_fitness = 0.0;
    if (evo->experience_count > 0) {
        avg_fitness = evo->total_fitness / (double)evo->experience_count;
    }

    cog_level_t evaluated = evo->current_level;
    for (int i = COG_LEVEL_CREATION; i >= COG_LEVEL_PERCEPTION; i--) {
        if (avg_fitness >= evo->level_thresholds[i]) {
            evaluated = (cog_level_t)i;
            break;
        }
    }

    *new_level = evaluated;

    if (evaluated != evo->current_level) {
        cog_level_t old = evo->current_level;
        evo->current_level = evaluated;
        if (evo->level_change_fn) {
            evo->level_change_fn(old, evaluated, evo->level_change_user_data);
        }
        return 1;
    }

    return 0;
}

int cog_evolution_set_reward_fn(cog_evolution_t* evo, cog_reward_fn fn, void* user_data) {
    if (!evo) return -1;
    evo->reward_fn = fn;
    evo->reward_user_data = user_data;
    return 0;
}

int cog_evolution_set_level_change_fn(cog_evolution_t* evo, cog_level_change_fn fn, void* user_data) {
    if (!evo) return -1;
    evo->level_change_fn = fn;
    evo->level_change_user_data = user_data;
    return 0;
}

size_t cog_evolution_get_experience_count(cog_evolution_t* evo) {
    return evo ? evo->experience_count : 0;
}

size_t cog_evolution_get_strategy_count(cog_evolution_t* evo) {
    return evo ? evo->strategy_count : 0;
}

size_t cog_evolution_get_pattern_count(cog_evolution_t* evo) {
    return evo ? evo->pattern_count : 0;
}

double cog_evolution_get_fitness(cog_evolution_t* evo) {
    if (!evo || evo->experience_count == 0) return 0.0;
    return evo->total_fitness / (double)evo->experience_count;
}
