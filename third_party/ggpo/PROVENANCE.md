# GGPO synchronization core

Source: https://github.com/pond3r/ggpo at 7ddadef8546a7d99ff0b3530c6056bc8ee4b9c0a (MIT; LICENSE retained).
Only the synchronization, input queue, game input and time-sync core is built.
No upstream UDP transport or peer packet implementation is substituted for the
Skullgirls Steam fork.

Compared with local output/SkullGirls.cpp: Sync::AddLocalInput matches
FUN_006a2c20, AdjustSimulation matches FUN_006a2cc0, SaveCurrentFrame matches
FUN_006a3360, and SetLastConfirmedFrame matches FUN_006a3400. The original
uses a 38-entry state ring (0x26); its session configuration sets the prediction
limit to 36 (0x24). Its per-player records are 21 bytes. Those capacities are
adapted here. Original event details and transport protocol parity are not
implied by using the shared synchronization code.

Changes: portable platform/log/assert shim; per-session callback context lives
in src/netplay_session.cpp; 36 prediction frames / 38 saved states; capacity for
21-byte per-player inputs; zero-initialize rollback status; reject invalid seek.

Non-Windows headers define the empty C calling-convention marker. Time-sync
estimator constants remain upstream pending verification against the fork; no
original packet compatibility or live time-sync behavior is claimed.
