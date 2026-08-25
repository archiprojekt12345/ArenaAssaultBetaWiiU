# Arena Assault v0.5 — mission + enemy archetypes

## Mission flow

1. **Infiltrate** — cross the arena and reach the blue terminal at the north side.
2. **Activate terminal** — stay close and hold **A** until the blue progress bar fills.
3. **Defend terminal** — remain inside the blue defense radius while squads attack. The defense timer only advances while the player is inside the objective zone.
4. **Eliminate miniboss** — an elite Heavy enters with escorts. A purple boss health bar appears.
5. **Evacuate** — reach the green extraction ring at the south side.
6. **Complete** — mission finished; **A** restarts the mission.

The TV HUD shows five objective pips. The GamePad map shows the current objective and defense radius.

## Enemy archetypes

### Scout
- low HP, smallest collider and model scale
- fastest movement and animation
- aggressive close-range flank/strafe behavior
- rapid low-damage fire
- cyan visual identity and antenna

### Soldier
- balanced HP, speed and accuracy
- medium combat range
- standard lateral strafe behavior
- orange visual identity

### Heavy
- high HP and larger collider/model
- slow movement, longer preferred range
- high-damage, slower fire
- reduced strafe and heavy shoulder armor
- purple visual identity

### Miniboss
The miniboss is an elite Heavy variant rather than a fourth regular class: roughly 2.35x Heavy HP, larger scale, faster fire cadence and stronger damage. It has a dedicated magenta marker and boss HP bar.

## Spawn pacing

The defense phase mixes squads so the composition changes over time instead of only increasing a wave number. Up to 16 enemies can exist; new squads spawn only while population is below the pressure cap, leaving room for the miniboss encounter.
