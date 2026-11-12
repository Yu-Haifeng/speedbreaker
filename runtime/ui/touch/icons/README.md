# Touch control icons

The design handoff's icons, copied from its `design/icons` folder, and the
source `tools/icons/make_icons.py` reads. Each is a 24x24 SVG whose strokes are
`currentColor`, so the deck can draw it in the colour a control calls for.

**Editing one means re-running the generator**: the deck draws
`runtime/ui/touch/touch_icons.h`, a build of these paths (curves and arcs
flattened, fills left as polygons), and nothing reads the SVGs at run time.

    python3 tools/icons/make_icons.py

The header is committed: building the game needs no Python. `steer.svg` is the
one drawn in the steering knob; the rest are the actions, the views, the
shifter's two chevrons and the D-pad's four.

Only the icons the deck uses are here. `expand`, `close`, `touch` and `star`
belong to the handoff's page rather than to its controls.
