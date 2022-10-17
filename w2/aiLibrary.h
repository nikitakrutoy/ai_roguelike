#pragma once

#include "stateMachine.h"
#include "behaviourTree.h"

// states
State *create_attack_enemy_state();
State *create_move_to_enemy_state();
State *create_flee_from_enemy_state();
State *create_patrol_state(float patrol_dist);
State *create_nop_state();

// transitions
StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs);

BehNode *sequence(const std::vector<BehNode*> &nodes);
BehNode *selector(const std::vector<BehNode*> &nodes);
BehNode *fallback(const std::vector<BehNode*> &nodes);
BehNode *parallel(const std::vector<BehNode*> &nodes);
BehNode *logic_or(const std::vector<BehNode*> &nodes);
BehNode *logic_not(const std::vector<BehNode*> &nodes);

BehNode *move_to_entity(flecs::entity entity, const char *bb_name);
BehNode *is_low_hp(float thres);
BehNode *is_safe(flecs::entity entity);
BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name);
BehNode *get_next_waypoint(flecs::entity entity, flecs::entity path, const char *bb_name);
BehNode *set_random_color(float speed);
BehNode *flee(flecs::entity entity, const char *bb_name);
BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name);

