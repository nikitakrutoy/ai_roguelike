#include "roguelike.h"
#include "ecsTypes.h"
#include <debugdraw/debugdraw.h>
#include "stateMachine.h"
#include "aiLibrary.h"
#include "app.h"

//for scancodes
#include <GLFW/glfw3.h>


static void add_crafter_sublevel_sm1(StateMachine& sm)
{
  int gotopos1 = sm.addState(create_gotopos_state(Position(-5, 5)));
  int blink1 = sm.addState(create_blink_state(Color(0xff000000), Color(0xffffffff), 2));
  int gotopos2 = sm.addState(create_gotopos_state(Position(5, -5)));
  int blink2 = sm.addState(create_blink_state(Color(0xffff0000), Color(0xff0000ff), 4));

  sm.addTransition(create_arrived_to_pos_transition(Position(-5, 5)), gotopos1, blink1);
  sm.addTransition(create_time_transition(10.0f), blink1, gotopos2);
  sm.addTransition(create_arrived_to_pos_transition(Position(5, -5)), gotopos2, blink2);
  sm.addTransition(create_time_transition(5.0f), blink2, gotopos1);
}

static void add_crafter_sublevel_sm2(StateMachine& sm, flecs::entity entity)
{
  int gotoent = sm.addState(create_move_to_entity_state(entity));
  int blink1 = sm.addState(create_blink_state(Color(0xff00ff00), Color(0xff004400), 2));

  sm.addTransition(create_arrived_to_entity_transition(entity), gotoent, blink1);
}

static void add_crafter_sm(flecs::entity entity, flecs::entity other)
{
  entity.get([&](StateMachine& sm)
    {
      StateMachine sm1, sm2;

      auto sm1_state = create_state_with_sm();
      auto sm2_state = create_state_with_sm();

      add_crafter_sublevel_sm1(sm1_state->sm);
      add_crafter_sublevel_sm2(sm2_state->sm, other);

      int sm1_state_id = sm.addState(sm1_state);
      int sm2_state_id = sm.addState(sm2_state);

      sm.addTransition(create_time_transition(10.f), sm1_state_id, sm2_state_id);
      sm.addTransition(create_time_transition(10.f), sm2_state_id, sm1_state_id);

    });
}


static void add_berserk_sm(flecs::entity entity)
{
  entity.get([](StateMachine& sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    sm.addTransition(create_hitpoints_less_than_transition(60.f), patrol, moveToEnemy);
  });
}

static void add_patrol_attack_flee_healing_sm(flecs::entity entity)
{
  entity.get([](StateMachine& sm)
    {
      int patrol = sm.addState(create_patrol_state(3.f));
      int moveToEnemy = sm.addState(create_move_to_enemy_state());
      int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());
      int healing = sm.addState(create_healing_state(20.0f));

      sm.addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
      sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

      sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(5.f)),
        moveToEnemy, fleeFromEnemy);
      sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(3.f)),
        patrol, fleeFromEnemy);
      sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(99.f), create_enemy_available_transition(3.f)),
        healing, fleeFromEnemy);

      sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_negate_transition(create_enemy_available_transition(5.f))),
        patrol, healing);

      sm.addTransition(create_negate_transition(create_hitpoints_less_than_transition(99.f)),
        healing, patrol);



      sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);
    });
}

static void add_patrol_attack_flee_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int moveToEnemy = sm.addState(create_move_to_enemy_state());
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, moveToEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), moveToEnemy, patrol);

    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(5.f)),
                     moveToEnemy, fleeFromEnemy);
    sm.addTransition(create_and_transition(create_hitpoints_less_than_transition(60.f), create_enemy_available_transition(3.f)),
                     patrol, fleeFromEnemy);

    sm.addTransition(create_negate_transition(create_enemy_available_transition(7.f)), fleeFromEnemy, patrol);
  });
}

static void add_patrol_flee_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    int patrol = sm.addState(create_patrol_state(3.f));
    int fleeFromEnemy = sm.addState(create_flee_from_enemy_state());

    sm.addTransition(create_enemy_available_transition(3.f), patrol, fleeFromEnemy);
    sm.addTransition(create_negate_transition(create_enemy_available_transition(5.f)), fleeFromEnemy, patrol);
  });
}

static void add_attack_sm(flecs::entity entity)
{
  entity.get([](StateMachine &sm)
  {
    sm.addState(create_move_to_enemy_state());
  });
}

static flecs::entity create_monster(flecs::world &ecs, int x, int y, uint32_t color)
{
  return ecs.entity()
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(PatrolPos{x, y})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{color})
    .set(StateMachine{})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{20.f});
}

static flecs::entity create_crafter(flecs::world& ecs, int x, int y, uint32_t color)
{
  return create_monster(ecs, x, y, color);
}

static void create_player(flecs::world &ecs, int x, int y)
{
  ecs.entity("player")
    .set(Position{x, y})
    .set(MovePos{x, y})
    .set(Hitpoints{100.f})
    .set(Color{0xffeeeeee})
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(MeleeDamage{50.f});
}

static void create_heal(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(HealAmount{amount})
    .set(Color{0xff4444ff});
}

static void create_powerup(flecs::world &ecs, int x, int y, float amount)
{
  ecs.entity()
    .set(Position{x, y})
    .set(PowerupAmount{amount})
    .set(Color{0xff00ffff});
}

static void register_roguelike_systems(flecs::world &ecs)
{
  ecs.system<PlayerInput, Action, const IsPlayer>()
    .each([&](PlayerInput &inp, Action &a, const IsPlayer)
    {
      bool left = app_keypressed(GLFW_KEY_LEFT);
      bool right = app_keypressed(GLFW_KEY_RIGHT);
      bool up = app_keypressed(GLFW_KEY_UP);
      bool down = app_keypressed(GLFW_KEY_DOWN);
      if (left && !inp.left)
        a.action = EA_MOVE_LEFT;
      if (right && !inp.right)
        a.action = EA_MOVE_RIGHT;
      if (up && !inp.up)
        a.action = EA_MOVE_UP;
      if (down && !inp.down)
        a.action = EA_MOVE_DOWN;
      inp.left = left;
      inp.right = right;
      inp.up = up;
      inp.down = down;
    });
  ecs.system<const Position, const Color>()
    .each([&](const Position &pos, const Color color)
    {
      DebugDrawEncoder dde;
      dde.begin(0);
      dde.push();
        dde.setColor(color.color);
        dde.drawQuad(bx::Vec3(0, 0, 1), bx::Vec3(pos.x, pos.y, 0.f), 1.f);
      dde.pop();
      dde.end();
    });
}


void init_roguelike(flecs::world &ecs)
{
  register_roguelike_systems(ecs);

  add_patrol_attack_flee_sm(create_monster(ecs, 5, 5, 0xffee00ee));
  add_patrol_attack_flee_sm(create_monster(ecs, 10, -5, 0xffee00ee));
  add_patrol_flee_sm(create_monster(ecs, -5, -5, 0xff111111));
  add_attack_sm(create_monster(ecs, -5, 5, 0xff00ff00));

  flecs::entity berserk = create_monster(ecs, 4, 0, 0xffff0000);
  add_berserk_sm(berserk);
  add_patrol_attack_flee_healing_sm(create_monster(ecs, -4, 0, 0xff00ffff).add<IsHealer>());

  add_crafter_sm(create_crafter(ecs, 0, 3, 0xff00ff00), berserk);



  create_player(ecs, 0, 0);


  create_powerup(ecs, 7, 7, 10.f);
  create_powerup(ecs, 10, -6, 10.f);
  create_powerup(ecs, 10, -4, 10.f);

  //create_heal(ecs, -5, -5, 50.f);
  //create_heal(ecs, -5, 5, 50.f);
}

static bool is_player_acted(flecs::world &ecs)
{
  static auto processPlayer = ecs.query<const IsPlayer, const Action>();
  bool playerActed = false;
  processPlayer.each([&](const IsPlayer, const Action &a)
  {
    playerActed = a.action != EA_NOP;
  });
  return playerActed;
}

static bool upd_player_actions_count(flecs::world &ecs)
{
  static auto updPlayerActions = ecs.query<const IsPlayer, NumActions>();
  bool actionsReached = false;
  updPlayerActions.each([&](const IsPlayer, NumActions &na)
  {
    na.curActions = (na.curActions + 1) % na.numActions;
    actionsReached |= na.curActions == 0;
  });
  return actionsReached;
}

static Position move_pos(Position pos, int action)
{
  if (action == EA_MOVE_LEFT)
    pos.x--;
  else if (action == EA_MOVE_RIGHT)
    pos.x++;
  else if (action == EA_MOVE_UP)
    pos.y++;
  else if (action == EA_MOVE_DOWN)
    pos.y--;
  return pos;
}

static void process_actions(flecs::world &ecs)
{
  static auto processActions = ecs.query<Action, Position, MovePos, const MeleeDamage, const Team>();
  static auto checkAttacks = ecs.query<const MovePos, Hitpoints, const Team>();
  // Process all actions
  ecs.defer([&]
  {
    processActions.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
    {
      Position nextPos = move_pos(pos, a.action);
      bool blocked = false;
      checkAttacks.each([&](flecs::entity enemy, const MovePos &epos, Hitpoints &hp, const Team &enemy_team)
      {
        if (entity != enemy && epos == nextPos)
        {
          blocked = true;
          if (team.team != enemy_team.team)
            hp.hitpoints -= dmg.damage;
        }
      });
      if (blocked)
        a.action = EA_NOP;
      else
        mpos = nextPos;
    });
    // now move
    processActions.each([&](flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &, const Team&)
    {
      pos = mpos;
      a.action = EA_NOP;
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&]
  {
    deleteAllDead.each([&](flecs::entity entity, const Hitpoints &hp)
    {
      if (hp.hitpoints <= 0.f)
        entity.destruct();
    });
  });

  static auto playerPickup = ecs.query<const IsPlayer, const Position, Hitpoints, MeleeDamage>();
  static auto healPickup = ecs.query<const Position, const HealAmount>();
  static auto powerupPickup = ecs.query<const Position, const PowerupAmount>();
  ecs.defer([&]
  {
    playerPickup.each([&](const IsPlayer&, const Position &pos, Hitpoints &hp, MeleeDamage &dmg)
    {
      healPickup.each([&](flecs::entity entity, const Position &ppos, const HealAmount &amt)
      {
        if (pos == ppos)
        {
          hp.hitpoints += amt.amount;
          entity.destruct();
        }
      });
      powerupPickup.each([&](flecs::entity entity, const Position &ppos, const PowerupAmount &amt)
      {
        if (pos == ppos)
        {
          dmg.damage += amt.amount;
          entity.destruct();
        }
      });
    });
  });
}

void process_turn(flecs::world &ecs)
{
  static auto stateMachineAct = ecs.query<StateMachine>();
  if (is_player_acted(ecs))
  {
    if (upd_player_actions_count(ecs))
    {
      // Plan action for NPCs
      ecs.defer([&]
      {
        stateMachineAct.each([&](flecs::entity e, StateMachine &sm)
        {
          sm.act(1.f, ecs, e);
        });
      });
    }
    process_actions(ecs);
  }
}

void print_stats(flecs::world &ecs)
{
  bgfx::dbgTextClear();
  static auto playerStatsQuery = ecs.query<const IsPlayer, const Hitpoints, const MeleeDamage>();
  playerStatsQuery.each([&](const IsPlayer &, const Hitpoints &hp, const MeleeDamage &dmg)
  {
    bgfx::dbgTextPrintf(0, 1, 0x0f, "Player Info");
    bgfx::dbgTextPrintf(0, 2, 0x0f, "hp: %d", (int)hp.hitpoints);
    bgfx::dbgTextPrintf(0, 3, 0x0f, "power: %d", (int)dmg.damage);
  });
  static auto healerStatsQuery = ecs.query<const IsHealer, const Hitpoints, const MeleeDamage>();
  healerStatsQuery.each([&](const IsHealer&, const Hitpoints& hp, const MeleeDamage& dmg)
    {
      bgfx::dbgTextPrintf(0, 4, 0x0f, "Healer Info");
      bgfx::dbgTextPrintf(0, 5, 0x0f, "hp: %d", (int)hp.hitpoints);
      bgfx::dbgTextPrintf(0, 6, 0x0f, "power: %d", (int)dmg.damage);
    });
}

