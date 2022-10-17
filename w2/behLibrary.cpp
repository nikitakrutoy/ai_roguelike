#include "aiLibrary.h"
#include "ecsTypes.h"
#include "aiUtils.h"
#include "math.h"
#include "raylib.h"
#include "blackboard.h"

struct CompoundNode : public BehNode
{
  std::vector<BehNode*> nodes;
  BehNode* cachedNode = nullptr;

  virtual ~CompoundNode()
  {
    for (BehNode *node : nodes)
      delete node;
    nodes.clear();
  }

  CompoundNode &pushNode(BehNode *node)
  {
    nodes.push_back(node);
    return *this;
  }

  void react(flecs::world &ecs, flecs::entity entity, Blackboard &bb, BehEvent event) override
  {
    if(cachedNode)
      cachedNode->react(ecs, entity, bb, event);
  }
};

struct Sequence : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      cachedNode = node;
      if (res == BEH_FAIL)
        cachedNode = nullptr;
      if (res != BEH_SUCCESS)
        return res;
    }
    return BEH_SUCCESS;
  }
};

struct Selector : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      if (res != BEH_FAIL)
      {
        cachedNode = node;
        return res;
      }
    }
    cachedNode = nullptr;
    return BEH_FAIL;
  }
};

struct Parallel: public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
      cachedNode = node;
      if (res == BEH_FAIL)
        cachedNode = nullptr;
      if (res != BEH_RUNNING)
        return res;
    }
    return BEH_RUNNING;
  }
};

struct Or : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    assert(nodes.size() >= 2);

    BehResult res1 = nodes[0]->update(ecs, entity, bb);
    cachedNode = nodes[0];
    if (res1 == BEH_SUCCESS)
      return BEH_SUCCESS;

    BehResult res2 = nodes[1]->update(ecs, entity, bb);
    cachedNode = nodes[1];
    if (res1 == BEH_SUCCESS)
      return BEH_SUCCESS;

    if (res1 == BEH_RUNNING || res2 == BEH_RUNNING)
      return BEH_RUNNING;
    else
      return BEH_FAIL;
  }
};

struct Inverse : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override {
    assert(nodes.size() >= 1);
    BehResult res = nodes[0]->update(ecs, entity, bb);

    if (res == BEH_SUCCESS)
    {
      cachedNode = nullptr;
      return BEH_FAIL;
    }
    else
    {
      cachedNode = nodes[0];
      return BEH_SUCCESS;
    }
  }
};

struct MoveToEntity : public BehNode
{
  size_t entityBb = size_t(-1); // wraps to 0xff...
  MoveToEntity(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      auto value = bb.get_safe<flecs::entity>(entityBb);
      if (!value.is_initialized)
      {
        res = BEH_FAIL;
        return;
      }
      flecs::entity targetEntity = value.value;

      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        if (pos != target_pos)
        {
          a.action = move_towards(pos, target_pos);
          res = BEH_RUNNING;
        }
        else
          res = BEH_SUCCESS;
      });
    });
    return res;
  }
};

struct IsLowHp : public BehNode
{
  float threshold = 0.f;
  IsLowHp(float thres) : threshold(thres) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.get([&](const Hitpoints &hp)
    {
      res = hp.hitpoints < threshold ? BEH_SUCCESS : BEH_FAIL;
      if (hp.hitpoints < threshold)
        // flecs::entity().set(Event{BehEvent::E_DANGER});
    });
    return res;
  }
};

struct IsSafe : public BehNode
{
  size_t dangerBb =  size_t(-1);

  IsSafe(flecs::entity entity)
  {
    dangerBb = reg_entity_blackboard_var<bool>(entity, "danger");
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    if(!bb.get<bool>(dangerBb))
      return BEH_SUCCESS;
    else 
      return BEH_FAIL;
  }
};

struct FindEnemy : public BehNode
{
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindEnemy(flecs::entity entity, float in_dist, const char *bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_FAIL;
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    entity.set([&](const Position &pos, const Team &t)
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
      if (ecs.is_valid(closestEnemy) && closestDist <= distance)
      {
        bb.set<flecs::entity>(entityBb, closestEnemy);
        res = BEH_SUCCESS;
      }
    });
    return res;
  }
};

struct SetRandomColor : public BehNode
{
  float speed = 1;
  SetRandomColor(float speed) : speed(speed) {}
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb)
  {
    entity.set([&](Color &color)
    {
      char r = int(ecs.time() * speed * 1000) % 255;
      char g = int(ecs.time() * speed * 2000) % 255;
      char b = int(ecs.time() * speed * 3000) % 255;
      char a = 255;
      color = Color(r, g, b, a);
    }); 
    return BEH_RUNNING;
  }
};

struct GetNextWayPoint : public BehNode
{
  size_t entityBb = size_t(-1);
  flecs::entity currentPath;
  size_t currentWaypoint = -1;
  GetNextWayPoint(flecs::entity entity, flecs::entity path, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
    currentPath = path;
  }
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    currentPath.get([&](const Path &p) {
      size_t size = p.points.size();
      flecs::entity e = p.points[++currentWaypoint % size];
      bb.set<flecs::entity>(entityBb, e);
    });
    return BEH_SUCCESS;
  }
};

struct Flee : public BehNode
{
  size_t entityBb = size_t(-1);
  Flee(flecs::entity entity, const char *bb_name)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      flecs::entity targetEntity = bb.get<flecs::entity>(entityBb);
      if (!targetEntity.is_alive())
      {
        res = BEH_FAIL;
        return;
      }
      targetEntity.get([&](const Position &target_pos)
      {
        a.action = inverse_move(move_towards(pos, target_pos));
      });
    });
    return res;
  }
};

struct Patrol : public BehNode
{
  size_t pposBb = size_t(-1);
  size_t dangerBb =  size_t(-1);

  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
    : patrolDist(patrol_dist)
  {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
    dangerBb = reg_entity_blackboard_var<bool>(entity, "danger");

    entity.set([&](Blackboard &bb, const Position &pos)
    {
      bb.set<Position>(pposBb, pos);
    });
  }

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action &a, const Position &pos)
    {
      Position patrolPos = bb.get<Position>(pposBb);
      if (dist(pos, patrolPos) > patrolDist)
        a.action = move_towards(pos, patrolPos);
      else
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
    });
    return res;
  }

  void react(flecs::world &, flecs::entity entity, Blackboard &bb, BehEvent event) override
  {
    if (event == BehEvent::E_DANGER)
      bb.set<bool>(dangerBb, true);
  }
};


BehNode *sequence(const std::vector<BehNode*> &nodes)
{
  Sequence *seq = new Sequence;
  for (BehNode *node : nodes)
    seq->pushNode(node);
  return seq;
}

BehNode *selector(const std::vector<BehNode*> &nodes)
{
  Selector *sel = new Selector;
  for (BehNode *node : nodes)
    sel->pushNode(node);
  return sel;
}

BehNode *fallback(const std::vector<BehNode*> &nodes)
{
  return selector(nodes);
}

BehNode *parallel(const std::vector<BehNode*> &nodes)
{
  Parallel *par = new Parallel;
  for (BehNode *node : nodes)
    par->pushNode(node);
  return par;
}

BehNode *logic_not(const std::vector<BehNode*> &nodes)
{
  Inverse *inv = new Inverse;
  for (BehNode *node : nodes)
    inv->pushNode(node);
  return inv;
}


BehNode *set_random_color(float speed)
{
  return new SetRandomColor(speed);
}

BehNode *move_to_entity(flecs::entity entity, const char *bb_name)
{
  return new MoveToEntity(entity, bb_name);
}

BehNode *is_low_hp(float thres)
{
  return new IsLowHp(thres);
}

BehNode *is_safe(flecs::entity entity)
{
  return new IsSafe(entity);
}

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name)
{
  return new FindEnemy(entity, dist, bb_name);
}

BehNode *get_next_waypoint(flecs::entity entity, flecs::entity path, const char *bb_name)
{
  return new GetNextWayPoint(entity, path, bb_name);
}

BehNode *flee(flecs::entity entity, const char *bb_name)
{
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
{
  return new Patrol(entity, patrol_dist, bb_name);
}

