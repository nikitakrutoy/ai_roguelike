#include "aiLibrary.h"
#include <flecs.h>
#include "ecsTypes.h"
#include <bx/rng.h>
#include <cfloat>
#include <cmath>
#include <algorithm>

static bx::RngShr3 rng;

class AttackEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &/*ecs*/, flecs::entity /*entity*/) override {}
};

template<typename T>
T sqr(T a){ return a*a; }

template<typename T, typename U>
static float dist_sq(const T &lhs, const U &rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
static float dist(const T &lhs, const U &rhs) { return sqrtf(dist_sq(lhs, rhs)); }

template<typename T, typename U>
static int move_towards(const T &from, const U &to)
{
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (abs(deltaX) > abs(deltaY))
    return deltaX > 0 ? EA_MOVE_RIGHT : EA_MOVE_LEFT;
  return deltaY > 0 ? EA_MOVE_UP : EA_MOVE_DOWN;
}

static int inverse_move(int move)
{
  return move == EA_MOVE_LEFT ? EA_MOVE_RIGHT :
         move == EA_MOVE_RIGHT ? EA_MOVE_LEFT :
         move == EA_MOVE_UP ? EA_MOVE_DOWN :
         move == EA_MOVE_DOWN ? EA_MOVE_UP : move;
}


template<typename Callable>
static void on_closest_enemy_pos(flecs::world &ecs, flecs::entity entity, Callable c)
{
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  entity.set([&](const Position &pos, const Team &t, Action &a)
  {
    flecs::entity closestEnemy;
    float closestDist = FLT_MAX;
    Position closestPos;
    enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
    {
      if (t.team == et.team)
        return;
      float curDist = dist(epos, pos);
      if (curDist < closestDist)
      {
        closestDist = curDist;
        closestPos = epos;
        closestEnemy = enemy;
      }
    });
    if (ecs.is_valid(closestEnemy))
      c(a, pos, closestPos);
  });
}

class MoveToEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = move_towards(pos, enemy_pos);
    });
  }
};


class FleeFromEnemyState : public State
{
public:
  FleeFromEnemyState() {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = inverse_move(move_towards(pos, enemy_pos));
    });
  }
};

class PatrolState : public State
{
  float patrolDist;
public:
  PatrolState(float dist) : patrolDist(dist) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override
  {
    entity.set([&](const Position &pos, const PatrolPos &ppos, Action &a)
    {
      if (dist(pos, ppos) > patrolDist)
        a.action = move_towards(pos, ppos); // do a recovery walk
      else
      {
        // do a random walk
        a.action = EA_MOVE_START + (rng.gen() % (EA_MOVE_END - EA_MOVE_START));
      }
    });
  }
};

class HealingState : public State
{
  float delta = 20.0f;
public:
  HealingState(float delta = 20.0): delta(delta) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world& ecs, flecs::entity entity) override
  {
    entity.set([&](Hitpoints& hp) {
      hp.hitpoints = std::min(100.0f, hp.hitpoints + delta);
    });
  }
};

class NopState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) override {}
};

class GoToPosState: public State
{
  Position ppos = { 0, 0 };
public:
  GoToPosState(Position pos) : ppos(pos) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world& ecs, flecs::entity entity) override
  {
    entity.set([&](const Position& pos, Action& a)
      {
        a.action = move_towards(pos, ppos);
      });
  }
};

class BlinkState : public State
{
  int counter = 0;
  Color color1 = { 0 };
  Color color2 = { 0 };
  Color currentColor;
  int speed = 2;
public:
  BlinkState(Color color1, Color color2, int speed = 2) : color1(color1), color2(color2) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world& ecs, flecs::entity entity) override
  {
    counter++;
    if (counter % speed == 0)
      currentColor = (bool)((counter / speed) % 2) ? color1 : color2;
      
    entity.set([&](Color& color, Action& a)
    {
        color = currentColor;
    });
  }
};

class MoveToEntityState: public State
{
  flecs::entity target;
public:

  MoveToEntityState(flecs::entity entity): target(entity) {}

  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world& ecs, flecs::entity entity) override
  {
    if (!target.is_alive())
      return;
    Position ppos;
    target.get([&](const Position& pos) {
      ppos = pos;
    });

    entity.set([&](const Position& pos, Action& a)
    {
      a.action = move_towards(pos, ppos);
    });
  }
};

class ArrivedToPosTransition : public StateTransition
{
  Position ppos = { 0, 0 };
public:
  ArrivedToPosTransition(Position pos) : ppos(pos) {}
  bool isAvailable(flecs::world& ecs, flecs::entity entity) const override
  {
    bool arrived = false;
    entity.get([&](const Position& pos) {
      arrived = pos == ppos;
    });
    return arrived;
  }
};

class ArrivedToEntityTransition : public StateTransition
{
  flecs::entity target;
public:
  ArrivedToEntityTransition(flecs::entity entity) : target(entity) {}
  bool isAvailable(flecs::world& ecs, flecs::entity entity) const override
  {
    if (!target.is_alive())
      return true;
    bool arrived = false;
    target.get([&](const Position& ppos) {
      entity.set([&](const Position& pos, Action& a)
        {
          arrived = pos == ppos;
        });
    });
    return arrived;
  }
};

class TimeTransition : public StateTransition
{
  float time = 0;
public:
  TimeTransition(float time) : time(time) {}
  bool isAvailable(flecs::world& ecs, flecs::entity entity) const override
  {
    bool expired = false;
    entity.get([&](const StateMachine &sm) {
      expired = getDuration() > time;
    });
    return expired;
  }
};



class EnemyAvailableTransition : public StateTransition
{
  float triggerDist;
public:
  EnemyAvailableTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    bool enemiesFound = false;
    entity.get([&](const Position &pos, const Team &t)
    {
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        enemiesFound |= curDist <= triggerDist;
      });
    });
    return enemiesFound;
  }
};

class HitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  HitpointsLessThanTransition(float in_thres) : threshold(in_thres) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool hitpointsThresholdReached = false;
    entity.get([&](const Hitpoints &hp)
    {
      hitpointsThresholdReached |= hp.hitpoints < threshold;
    });
    return hitpointsThresholdReached;
  }
};

class EnemyReachableTransition : public StateTransition
{
public:
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return false;
  }
};

class NegateTransition : public StateTransition
{
  const StateTransition *transition; // we own it
public:
  NegateTransition(const StateTransition *in_trans) : transition(in_trans) {}
  ~NegateTransition() override { delete transition; }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return !transition->isAvailable(ecs, entity);
  }
};

class AndTransition : public StateTransition
{
  const StateTransition *lhs; // we own it
  const StateTransition *rhs; // we own it
public:
  AndTransition(const StateTransition *in_lhs, const StateTransition *in_rhs) : lhs(in_lhs), rhs(in_rhs) {}
  ~AndTransition() override
  {
    delete lhs;
    delete rhs;
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return lhs->isAvailable(ecs, entity) && rhs->isAvailable(ecs, entity);
  }
};


// states
State *create_attack_enemy_state()
{
  return new AttackEnemyState();
}
State *create_move_to_enemy_state()
{
  return new MoveToEnemyState();
}

State *create_flee_from_enemy_state()
{
  return new FleeFromEnemyState();
}

State *create_healing_state(float healDelta)
{
  return new HealingState(healDelta);
}

State *create_patrol_state(float patrol_dist)
{
  return new PatrolState(patrol_dist);
}

State *create_nop_state()
{
  return new NopState();
}

State *create_gotopos_state(Position pos)
{
  return new GoToPosState(pos);
}

State *create_move_to_entity_state(flecs::entity entity)
{
  return new MoveToEntityState(entity);
}

State *create_blink_state(Color color1, Color color2, int speed)
{
  return new BlinkState(color1, color2, speed);
}

StateWithSM* create_state_with_sm()
{
  return new StateWithSM();
}

// transitions
StateTransition *create_enemy_available_transition(float dist)
{
  return new EnemyAvailableTransition(dist);
}

StateTransition *create_enemy_reachable_transition()
{
  return new EnemyReachableTransition();
}

StateTransition *create_hitpoints_less_than_transition(float thres)
{
  return new HitpointsLessThanTransition(thres);
}

StateTransition *create_negate_transition(StateTransition *in)
{
  return new NegateTransition(in);
}
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs)
{
  return new AndTransition(lhs, rhs);
}

StateTransition *create_arrived_to_pos_transition(Position pos)
{
  return new ArrivedToPosTransition(pos);
}

StateTransition *create_arrived_to_entity_transition(flecs::entity entity)
{
  return new ArrivedToEntityTransition(entity);
}

StateTransition *create_time_transition(float time)
{
  return new TimeTransition(time);
}
