# FullPanelDeveloper

The menu's front page with Developer tools on: a Developer section after the notices, which folds, with its filter and a row for each config spec it lists.

- The page's sections in order: its text, Quick launch, Notices, Developer. The Developer section is there while Developer tools is on, even when its filter lists no config spec: the filter is its first row, and the tag says `0 of 12`.
- Its heading, its filter and a config spec's row are drawn as [Developer](../Developer/README.md) says. A press on a config spec's row opens its report ([FullPanelConfigSpecReport](../FullPanelConfigSpecReport/README.md)).
- A config spec that cannot be read is in Notices as not available, as before, and in Developer as `Not loaded`, with the reason in its report.
- The rows scroll with the rest of the page. The filter is written at once, so Discard and Save stay disabled.
