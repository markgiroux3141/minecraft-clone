#pragma once

namespace vc {

// Per-frame edge/repeat tracking for GameApp's input handling. Each *WasDown
// flag remembers whether a key/button was held last frame so HandleInput can
// fire once on the press edge; the *Cooldown timers throttle auto-repeat
// (held-button break/place/drop). Previously ~15 loose members bolted onto
// GameApp — grouped here so new input edges have an obvious home (see CLAUDE.md
// "Keeping modules small").
struct InputState {
    // Press-edge tracking.
    bool modeKeyWasDown = false;       // F: walk/fly toggle
    bool occlusionKeyWasDown = false;  // O: cave-cull toggle
    bool escapeWasDown = false;        // pause / back
    bool inventoryKeyWasDown = false;  // E: inventory
    bool clickWasDown = false;         // menu clicks (break/place track separately)
    bool rightClickWasDown = false;
    bool breakWasDown = false;
    bool placeWasDown = false;
    bool dropKeyWasDown = false;       // Q: throw
    bool debugMobKeyWasDown = false;   // G: debug Steve
    bool spawnPigKeyWasDown = false;   // B
    bool spawnZombieKeyWasDown = false; // C
    bool spawnCowKeyWasDown = false;      // V
    bool spawnSheepKeyWasDown = false;    // N
    bool spawnChickenKeyWasDown = false;  // M
    bool spawnCreeperKeyWasDown = false;  // K
    bool spawnSkeletonKeyWasDown = false; // J
    bool spawnBabyKeyWasDown = false;     // H: debug baby cow (M38)
    bool spawnSpiderKeyWasDown = false;   // L: debug spider (M39)

    // Auto-repeat throttles (seconds remaining).
    double breakCooldown = 0.0; // fly repeat AND the walk-dig hit delay
    double placeCooldown = 0.0;
    double dropCooldown = 0.0;
};

} // namespace vc
