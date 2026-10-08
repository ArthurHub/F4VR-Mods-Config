# Toggle

A boolean as two cells, Off and On, so the value is said in words as well as shown.

**Provides:** the bool value and its state.

- 176 x 48: two 88 px cells in a `track` frame with a `frame-border` outline.
- On: the On cell solid `green` with `on-green` text. Off: the Off cell in `pressed` with a 2 px `green` ring and `green` text. A solid fill always means on.
- Pending: the chosen cell in `amber` (fill for On, ring for Off).
- The trigger anywhere on the row flips it; pressing a cell sets that value.
