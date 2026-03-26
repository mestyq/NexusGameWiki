# Section Navigation Contract

This file documents the intended behavior for `NexusGameWiki` section navigation so future changes do not reintroduce drift or scroll-state bugs.

## Behavioral contract

- Clicking a contents entry scrolls the article so that the matching section header itself is visible at the top of the article viewport.
- Clicking the same contents entry repeatedly is idempotent: it should land in the same place every time and must not drift downward.
- Jumping must work from the top, middle, or bottom of a long page.
- If the target section is inside collapsed parents, those parents should expand before the jump is fulfilled.
- Contents clicks only navigate within the current page/tab. They must not trigger a different page load.

## Implementation note

- Do not persist numeric section anchor maps across frames for this feature.
- Keep only a pending target section id.
- Fulfill the jump when the real matching section header item is rendered in the article pane, and scroll to that rendered item directly.
- Clear the pending jump immediately after it is fulfilled.
