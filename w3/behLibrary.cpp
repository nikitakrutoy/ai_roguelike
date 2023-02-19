#include "aiLibrary.h"
#include "ecsTypes.h"
#include "aiUtils.h"
#include "math.h"
#include "raylib.h"
#include "blackboard.h"
#include <algorithm>

struct CompoundNode : public BehNode
{
  std::vector<BehNode*> nodes;

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
};

struct Sequence : public CompoundNode
{
  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    for (BehNode *node : nodes)
    {
      BehResult res = node->update(ecs, entity, bb);
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
        return res;
    }
    return BEH_FAIL;
  }
};

struct UtilitySelector : public BehNode
{
  std::vector<std::pair<BehNode*, utility_function>> utilityNodes;
  size_t prevNode = -1;
  float cooldownCoef = 0.1;
  float cooldownCoefCurrent = 0.0;
  bool weightedRandom = true;

  BehResult update(flecs::world &ecs, flecs::entity entity, Blackboard &bb) override
  {
    std::vector<std::pair<float, size_t>> utilityScores;
    for (size_t i = 0; i < utilityNodes.size(); ++i)
    {
      float utilityScore = utilityNodes[i].second(bb);
      if (prevNode == i)
        utilityScore *= 1 + cooldownCoefCurrent;
      utilityScores.push_back(std::make_pair(std::min(std::max(utilityScore, 0.0f), 100.f), i));
    }

    if (!weightedRandom)
    {
      std::sort(utilityScores.begin(), utilityScores.end(), [](auto& lhs, auto& rhs)
        {
          return lhs.first > rhs.first;
        });
      for (const std::pair<float, size_t>& node : utilityScores)
      {
        size_t nodeIdx = node.second;
        BehResult res = utilityNodes[nodeIdx].first->update(ecs, entity, bb);
        if (res != BEH_FAIL)
        {
          cooldownCoefCurrent = nodeIdx == prevNode ? cooldownCoefCurrent * cooldownCoef : cooldownCoef;
          prevNode = nodeIdx;
          printf("Entiyt %s, node %d, score %f, cooldown %f\n", entity.name().c_str(), nodeIdx, utilityScores[nodeIdx].first, cooldownCoefCurrent);
          return res;
        }
      }
      return BEH_FAIL;
    }
    else
    {
      float randomValue = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      for (int j = 0; j < utilityScores.size(); j++)
      {
        std::vector<std::pair<float, size_t>> utilityScoresTmp = utilityScores;
        float scoresSum = 0;

        for (auto score : utilityScoresTmp)
          scoresSum += score.first;

        for (auto& score : utilityScoresTmp)
          score.first = score.first / scoresSum;

        float prevValue = 0;
        size_t nodeIdx = utilityScoresTmp.size() - 1;
        float accumulator = 0;
        for (int i = 0; i < utilityScoresTmp.size(); i++)
        {
          if (randomValue > accumulator && randomValue < accumulator + utilityScoresTmp[i].first)
          {
            nodeIdx = utilityScoresTmp[i].second;
            break;
          }
          accumulator += utilityScoresTmp[i].first;
        }
        BehResult res = utilityNodes[nodeIdx].first->update(ecs, entity, bb);
        if (res != BEH_FAIL)
        {
          cooldownCoefCurrent = nodeIdx == prevNode ? cooldownCoefCurrent * cooldownCoef : cooldownCoef;
          prevNode = nodeIdx;
          printf("Entiyt %s, node %d, score %f, cooldown %f, random_v %f\n", entity.name().c_str(), nodeIdx, utilityScores[nodeIdx].first, cooldownCoefCurrent, randomValue);

          return res;
        }
        utilityScores.erase(utilityScores.begin() + nodeIdx);
      }
      return BEH_FAIL;
    }
  }
};

struct FindBase : public BehNode
{
  size_t entityBb = size_t(-1);
  float distance = 0;
  FindBase(flecs::entity entity, float in_dist, const char* bb_name) : distance(in_dist)
  {
    entityBb = reg_entity_blackboard_var<flecs::entity>(entity, bb_name);
  }
  BehResult update(flecs::world& ecs, flecs::entity entity, Blackboard& bb) override
  {
    BehResult res = BEH_FAIL;
    static auto baseQuery = ecs.query<const Position, const IsBase>();
    entity.set([&](const Position& pos)
      {
        flecs::entity closestBase;
        float closestDist = FLT_MAX;
        Position closestPos;
        baseQuery.each([&](flecs::entity base, const Position& epos, const IsBase& et)
          {
            float curDist = dist(epos, pos);
            if (curDist < closestDist)
            {
              closestDist = curDist;
              closestPos = epos;
              closestBase = base;
            }
          });
        if (ecs.is_valid(closestBase) && closestDist <= distance)
        {
          bb.set<flecs::entity>(entityBb, closestBase);
          res = BEH_SUCCESS;
        }
      });
    return res;
  }
};


struct MoveToEntity : public BehNode
{
  size_t entityBb = size_t(-1); // wraps to 0xff...
  MoveToEntity(flecs::entity &entity, const char *bb_name)
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
    });
    return res;
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

struct RandomWalk : public BehNode
{
  BehResult update(flecs::world&, flecs::entity entity, Blackboard& bb) override
  {
    BehResult res = BEH_RUNNING;
    entity.set([&](Action& a, const Position& pos)
      {
          a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1); // do a random walk
      });
    return res;
  }
};

struct Patrol : public BehNode
{
  size_t pposBb = size_t(-1);
  float patrolDist = 1.f;
  Patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
    : patrolDist(patrol_dist)
  {
    pposBb = reg_entity_blackboard_var<Position>(entity, bb_name);
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
};

struct PatchUp : public BehNode
{
  float hpThreshold = 100.f;
  PatchUp(float threshold) : hpThreshold(threshold) {}

  BehResult update(flecs::world &, flecs::entity entity, Blackboard &) override
  {
    BehResult res = BEH_SUCCESS;
    entity.set([&](Action &a, Hitpoints &hp)
    {
      if (hp.hitpoints >= hpThreshold)
        return;
      res = BEH_RUNNING;
      a.action = EA_HEAL_SELF;
    });
    return res;
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

BehNode *utility_selector(const std::vector<std::pair<BehNode*, utility_function>> &nodes)
{
  UtilitySelector *usel = new UtilitySelector;
  usel->utilityNodes = std::move(nodes);
  return usel;
}

BehNode *move_to_entity(flecs::entity& entity, const char *bb_name)
{
  return new MoveToEntity(entity, bb_name);
}

BehNode *is_low_hp(float thres)
{
  return new IsLowHp(thres);
}

BehNode *find_enemy(flecs::entity entity, float dist, const char *bb_name)
{
  return new FindEnemy(entity, dist, bb_name);
}

BehNode* find_base(flecs::entity entity, float dist, const char* bb_name)
{
  return new FindBase(entity, dist, bb_name);
}

BehNode *flee(flecs::entity entity, const char *bb_name)
{
  return new Flee(entity, bb_name);
}

BehNode *patrol(flecs::entity entity, float patrol_dist, const char *bb_name)
{
  return new Patrol(entity, patrol_dist, bb_name);
}

BehNode *random_walk(flecs::entity entity, const char* bb_name)
{
  return new RandomWalk();
}

BehNode *patch_up(float thres)
{
  return new PatchUp(thres);
}


