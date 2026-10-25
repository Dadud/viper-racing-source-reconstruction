# Route-based opponent pacing prototype

The current Win32 race runtime no longer supplies every AI entrant with an
unconditional full-throttle action. `decide_route_opponent_action` computes a
deterministic per-fixed-step throttle/brake decision from one immutable route
state snapshot. It finds the nearest participant ahead by cyclic distance on
the normalized ideal line, skips finished/invalid peers, targets a conservative
speed when that gap closes, and holds/releases braking with hysteresis. With no
active leader it accelerates toward the route adapter's prototype target speed.
The player is included in the traffic snapshot at its rendered entrant index.

The controller's state is staged alongside route movement and commits only
after the terrain sample succeeds. Thus failed terrain validation does not
advance AI hysteresis independently of the car. Trace CSVs record each
opponent's valid flag, throttle, brake, target speed, leader slot, and gap;
repeated trace playback checks deterministic output. The isolated
`route_opponent_ai_regression` covers close-leader braking, hysteresis,
open-route acceleration, finish handling, deterministic decisions, and
non-finite input rejection.

This controller is an authored safe-follow policy for the route-motion
prototype. Its speed and gap constants are not inferred from the original game.
It does not execute or approximate native ACAR callback state, use physical
wheel/body motion, steer between route choices, pass opponents, or establish
original AI difficulty/personality. Native AI reconstruction and original
behavior comparison remain required.
