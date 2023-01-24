#pragma once
#include <vector>
#include <flecs.h>

class State
{
public:
  virtual void enter() const = 0;
  virtual void exit() const = 0;
  virtual void act(float dt, flecs::world &ecs, flecs::entity entity) = 0;
};


class StateTransition
{
  float duration = 0;
public:
  virtual ~StateTransition() {}
  void reset() { duration = 0; }
  float getDuration() const { return duration; }

  virtual bool isAvailable(flecs::world &ecs, flecs::entity entity) const = 0;

  bool isAvailable(flecs::world& ecs, flecs::entity entity, float dt) {
    duration += dt;
    return isAvailable(ecs, entity);
  }
};

class StateMachine
{
  int curStateIdx = 0;
  std::vector<State*> states;
  std::vector<std::vector<std::pair<StateTransition*, int>>> transitions;
public:
  StateMachine() = default;
  StateMachine(const StateMachine &sm) = default;
  StateMachine(StateMachine &&sm) = default;

  ~StateMachine();

  StateMachine &operator=(const StateMachine &sm) = default;
  StateMachine &operator=(StateMachine &&sm) = default;


  void act(float dt, flecs::world &ecs, flecs::entity entity);

  int addState(State *st);
  void addTransition(StateTransition *trans, int from, int to);
};


class StateWithSM : public State
{
public:
  StateMachine sm;

  StateWithSM() {}


  void enter() const override {}
  void exit() const override {}
  void act(float dt, flecs::world& ecs, flecs::entity entity) override
  {
    sm.act(dt, ecs, entity);
  }
};

