# Left Pane Contract

This file documents the intended behavior of the `NexusGameWiki` left pane so future changes do not accidentally undo the current UX.

## Mode strip

- The left pane is controlled by a mode strip with:
  - `Search`
  - `Recent`
  - `Favorites`
- `Search` is the default mode.
- Typing or pasting a viable query automatically switches the left pane back to `Search`.
- `Recent` and `Favorites` are persistent on disk.

## Search normalization

- Query normalization is hidden from the user.
- The visible input stays exactly as typed or pasted.
- Internally, the search path removes outer square brackets, trims whitespace, and strips leading quantity tokens such as:
  - `[25 Superior Sharpening Stones]`
  - `[ 25 Superior Sharpening Stones ]`
  - `x25 Superior Sharpening Stones`
- Search, cache lookup, and refresh all use the cleaned internal query, not the raw pasted text.

## Recent and favorites

- Opening a page records it in `Recent`.
- Opening the same page again should move it to the front instead of duplicating it.
- `Favorites` is controlled by the article-header button:
  - `Favorite`
  - `Unfavorite`
- Removing a `Recent` or `Favorite` entry from the left pane should not affect the actual cached wiki content.

