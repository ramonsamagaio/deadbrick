# Reference analysis: Lay of the Land -> DEADBRICK

## What the supplied build tells us

The supplied Drive folder is a packaged Windows Unreal Engine 5 build, not source code. It contains the standard packaged `Engine`, game `Binaries`, `Content/Paks`, and runtime plugins including DLSS/FSR. The game data is stored in Unreal `.pak/.ucas/.utoc` containers. DEADBRICK therefore uses a clean-room implementation: reproduce useful systems and behaviors, not proprietary code or art.

## Systems worth preserving

1. Fine-grained voxel world that supports both construction and destruction.
2. Layered procedural generation rather than uniform noise-only generation.
3. Physical/environmental simulation as gameplay: collapse, fire, flowing materials, gas/liquid hooks.
4. Emergent combat where the player can change the environment instead of only damaging enemies.
5. Itemized world interaction, scavenging, crafting, equipment and progression.
6. Persistent player-made structures and persistent world damage.
7. Procedural points of interest connected into a coherent traversable world.

## DEADBRICK translation

Fantasy terrain becomes a living city graph:

`seed -> region -> district -> arterial roads -> blocks -> lots -> building archetypes -> floors -> rooms -> doors/windows -> props -> loot -> zombie population`

The generator should understand what a place is. A hospital is not a residential tower with a different sign. It gets medical rooms, pharmacy storage, generator rooms, ambulance access, relevant loot and a different zombie population. The same principle applies to malls, police stations, fire stations, banks, universities, offices, hotels, schools, warehouses, military sites, game stores and homes across economic tiers.

## Simulation translation

- falling sand -> rubble, gravel, loose masonry and collapse debris
- flowing water -> burst pipes, sprinklers, hydrants, flooded basements
- gas pockets -> natural-gas leaks, industrial gas, smoke and toxic interiors
- spreading fire -> furnishings, fuel, wood framing, vehicles and electrical fires
- tree collapse -> columns, floors, walls, utility poles and facade collapse
- fantasy ranged weapons -> firearms, bows/crossbows as rare alternatives, thrown explosives
- physical crafting -> salvage benches, field assembly, barricading, ammo loading and repair

## Performance rule

The city must be simulated in concentric fidelity rings. Nearby chunks use full voxel collision/physics. Mid-distance chunks use simplified meshes and coarse zombie simulation. Far districts exist as deterministic seed data plus population state until streamed in. This is mandatory if we want a dense multi-floor city rather than a tiny tech demo.
