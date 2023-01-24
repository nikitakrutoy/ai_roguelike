#pragma once

#include "stateMachine.h"
#include "ecsTypes.h"


// states
State *create_attack_enemy_state();
State *create_move_to_enemy_state();
State *create_flee_from_enemy_state();
State *create_healing_state(float healDelta);
State *create_gotopos_state(Position pos);
State *create_move_to_entity_state(flecs::entity entity);
State *create_blink_state(Color color1, Color color2, int speed);
StateWithSM*create_state_with_sm();

State *create_patrol_state(float patroldist);
State *create_nop_state();

// transitions
StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs);
StateTransition *create_arrived_to_pos_transition(Position pos);
StateTransition *create_arrived_to_entity_transition(flecs::entity entity);
StateTransition *create_time_transition(float time);