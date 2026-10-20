# FullPanelDeveloper

The menu's front page with Developer tools on: a Developer section after the notices, which folds, with its filter and a row for each package it lists.

- The page's sections in order: its text, Quick launch, Notices, Developer. The Developer section is there while Developer tools is on, even when its filter lists no package: the filter is its first row, and the tag says `0 of 12`.
- Its heading, its filter and a package's row are drawn as [Developer](../Developer/README.md) says. A press on a package's row opens its report ([FullPanelPackageReport](../FullPanelPackageReport/README.md)).
- A package that cannot be read is in Notices as not available, as before, and in Developer as `Not loaded`, with the reason in its report.
- The rows scroll with the rest of the page. The filter is written at once, so Discard and Save stay disabled.
