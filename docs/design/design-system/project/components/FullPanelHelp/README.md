# FullPanelHelp

The Help page: what each control of the menu does, with the buttons the player's config has for it. Help (`?`) in the title bar shows it.

**Provides:** the controls of the menu, each a name and a text, with the config's buttons and bindings in words.

- The crumbs are `MODS CONFIG VR » Help`: the first is pressed to go to the menu's front page, `Help` is where the player is. The Help button is not drawn selected: the crumb says where the player is.
- The tree keeps the selected mod and page: the Help page is shown in place of the page, and Back shows that page again. A row of the tree goes to its page, as from any page.
- A text item first, in `green-muted`: "How the menu is used, with the buttons your config has." Then a `Controls` heading.
- Each control as the help line has a row's help: `Name: ` in `green`, the text in `green-muted`, in `t-body`, wrapped in the page's width with as many lines as it needs, `space-2` in from the page's edges.
- Then a `Saving` heading: what Save does, and what each mark of when a change takes effect means, each as a control is, after its mark in `amber`: `Live`, `Save`, `Restart`, and `No mark` after the plain dot, for a mod that does not say ([TakesEffect](../TakesEffect/README.md)).
- A button or a binding is said in the sentence of `InputBinding`, from the player's config and handedness. Back and the move are buttons of the hand that points, so an A button is "the A or X button". A control the config turns off is left out.
- Nothing on the page is pressed. The footer is the selected mod's, with its status.
