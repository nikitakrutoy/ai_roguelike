## Crafter FSM

![Crafter FSM](resources/crafter_fsm.png?raw=true "Crafter FSM")


Learning materials for the course "AI for videogames" based on simple roguelike mechanics.
* w1 - FSM
* w2 - Behaviour Trees + blackboard
* w3 - Utility functions
* w4 - Emergent behaviour
* w5 - Goal Oriented Action Planning

## Dependencies
This project uses:
* bgfx for week1 project
* raylib for week2 and later project
* flecs for ECS

## Building

To build you first need to update submodules:
```
git submodule sync
git submodule update --init --recursive
```

Then you need to build using cmake:
```
cmake -B build
cmake --build build
```
