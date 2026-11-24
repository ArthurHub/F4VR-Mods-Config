# Dynamic pages

A page whose rows the mod's DLL gives when the page is selected. Some settings depend on what the game has right now: the items the player carries, or what other mods registered with the mod at runtime. A config spec can't list those rows ahead of time. A choice can already take its options from the client ([Options from the mod's client](schema.md#options-from-the-mods-client)), but that only works when the rows are fixed and only their options vary. A dynamic page lets the rows themselves vary.

Only a page's content is dynamic. The page itself, with its name, its place in the tree, its sub-pages and its conditions, is in the config spec as usual, so the tree, the crumbs and `openMenu` work as they do for any page.

The worked example is [RobCo PALM's wheel](#example-robco-palms-wheel). The menu's own [Categories page](ui-and-vr-interaction.md#changing-a-mods-category) is one too: its client gives a row for each mod the menu lists.

## In the config spec

```json
{
  "id": "wheelItems",
  "pageDisplayName": "Wheel items",
  "help": "Choose up to eight items for each part of the wheel.",
  "contentFromClientCallback": true
}
```

- `contentFromClientCallback: true` takes the place of `content`. A page with both is an error, and its `content` is not read.
- `id` is required: it is how the menu asks the client for the page's content. Without one the flag is an error, and the page keeps its own `content`.
- `pages` (sub-pages), conditions and the inherited defaults (`defaultFile`, `applies`, `omitDefault`) work as on any page. The rows the client gives inherit them.
- The front page can't be dynamic: it has no `id`.

## Not available

A page whose content can't be had is not available, in one of two ways:

- **The client can't be asked**: the mod's DLL isn't loaded, its client is cut off, or it is built with API version 1. The page is disabled in the tree, with the reason in the help line, as a page whose conditions disable it, and so are its sub-pages. The menu knows this without calling the client, so it marks the page as the mod is shown, and as a search starts. The config spec showcase's dynamic page is one: the showcase has no DLL.
- **The client gives nothing**: false, no `set`, or text that isn't a JSON array. The page still opens, the rows it was given before are dropped, and the page says why in their place. A client that fails at runtime doesn't take the player off the page.

## Native API

`IClient` gains one method, in API version 2:

```cpp
class IContent
{
public:
    virtual void set(const char* json) = 0;
};

virtual bool giveContent(const char* pageId, IContent* content) = 0;
```

- `json` is the page's `content` array as text, in the config spec's format. The host copies it in `set`, and reads it after the call returns.
- false, or no `set`, means the client has no content for the page. The page then shows as [not available](#not-available).
- The call is inside the [failure guard](architecture.md#a-client-that-fails), as every client call is.
- The host asks only clients registered at version 2 or later. An older client's dynamic page is disabled in the tree, with the reason.
- The C++ layer adds `addPageContent(pageId, handler)`, where the handler returns the JSON text. It works like `addFunction` and answers by page id.

## What the client gives

The rows are read as the config spec's own rows are, against the mod's config spec:

- Any content item: settings, sections, buttons, text, spacers, images. Sub-pages aren't content, so the client can't give them.
- A setting's storage is any storage a config spec setting can have. A file must be one of the config spec's `files`.
- Ids are unique across the whole mod. A row whose id is already in use, statically or on another dynamic page, is an error.
- A row's conditions can name any setting of the mod, or a client state. A static row's condition can't name a dynamic row, because that id doesn't exist when the config spec is loaded. It is then a missing setting, handled as any missing setting is ([Conditions](schema.md#conditions)).
- Errors are handled as a config spec's are: the affected item is disabled, and the error goes to the log and to [Developer tools](ui-and-vr-interaction.md#developer-tools). Text that isn't a JSON array makes the whole page [not available](#not-available).

## When the menu asks

The menu asks for a page's content when the page is selected, and keeps it until it asks again:

- It asks again each time the page is shown: selected again from another page, pressed again in the tree while it is shown, Back from a sub-page, a search result. And while the page is shown: after a Save or Discard of the footer that succeeds, on `reloadValues`, and on Restore after the menu was minimized, as the values are read again then. A client whose rows changed while the page is shown (the player picked something up) calls `reloadValues`. A single row's Reset and a failed Save don't ask.
- It never asks while a row of the page itself has a pending change, because new rows would drop that change. Rows of its sub-pages and of other pages don't count. Pending changes can't outlive the mod's session: leaving the mod asks to save them or discard them first.
- It never asks while a row of the page is in use: an editor, a list or the keyboard open over it, a slider dragged, a button's confirmation shown, or a transform tuned with Adjust in world. New rows would leave that pointing at nothing.
- An ask held back is dropped, not kept for later: the Save or Discard that ends the pending change asks, and so does the next show of the page.
- Each ask reads the mod's values again, with those of the new rows. The page keeps where it is scrolled to; its folded sections, focus ring and buttons' results start again, as on a page that is shown.
- Once given, the rows are in the mod's definition like any other rows. Pending markers, the tree's unsaved marks, folded sections' counts, Reset, Save and Discard all work on them unchanged. Their problems replace the page's earlier ones in [Developer tools](ui-and-vr-interaction.md#developer-tools).

## Search

Search finds what is in the definition. A dynamic page's rows are in it once the page has been shown in this run, and they stay as the client last gave them.

- The page's name always matches, as any page's does.
- Its rows match once the page has been shown. Before that, the page can be found only by its name.
- Search never asks a client. A query changes on each key, and a client's answer can depend on the game, so search doesn't ask at all.

## Example: RobCo PALM's wheel

[RobCo PALM](https://github.com/brunocatani/RobCo_PALM) is a wheel menu. Its settings come in three kinds, and only two need its DLL:

- **`PALM.toml`**: the controls, the pointer aim and the Developer switch. These are static rows on ordinary pages, with file storage. The open button is a `cf.inputBinding`.
- **Wheel sections**: which sectors the ring shows. This is a dynamic page. The five item categories and Gestures are always there; other mods' sections are registered with PALM at runtime, up to ten. Each row is a `switcher` kept in a client state (`"storage": { "clientState": "section.<id>" }`), because PALM saves the wheel in the game's co-save, not in a file. The rows are one section whose [`maxOn`](schema.md#a-limit-on-switchers) is the ring's free sectors, with Gestures `countsAs: 2` (a sector for each hand).
- **Wheel items**: up to eight items per category, chosen from what the player carries. This is a dynamic page with a section per category, each with `maxOn: 8`, and a `switcher` per carried item, plus one for each saved item the player no longer carries, with "Not carried" in its help. The item's count or Equipped goes in its `text`. The state is named by the item's stable key (`item.<category>.<plugin>|<id>`). PALM puts a newly chosen item in its lowest free ring slot when the Save reaches it.

The rows can be `applies: live`, so the wheel follows each change as the player makes it, and Discard takes the changes back.

## Showcase and tests

- The config spec showcase has no DLL, so its dynamic page, under Pages, is disabled in the tree with the reason, as its `CallNativeFunction` button is disabled. A `maxOn` section is static, so the showcase shows it on an ordinary page.
- The unit tests give the content through a test client. They cover reading the content, every time the menu asks and doesn't ask (pending changes, Save, Discard, `reloadValues`), and search before and after the page is shown. A render test draws a PALM-like page: two categories, a full one with its other rows disabled, and a not-carried item.
