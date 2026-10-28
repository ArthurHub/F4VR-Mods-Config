# FullPanelHelp

The Help page: how the menu works, with the buttons the player's config has. It tells what the menu does its own way, not what every VR player knows: there is no line for pointing, scrolling or stepping a value. Help (`?`) in the title bar shows it.

**Provides:** the controls of the menu, each a name and a text, with the config's buttons and bindings in words.

- The crumbs are `MODS CONFIG FO4 » Help`: the first is pressed to go to the menu's front page, `Help` is where the player is. The Help button is not drawn selected: the crumb says where the player is.
- The tree keeps the selected mod and page: the Help page is shown in place of the page, and Back shows that page again. A row of the tree goes to its page, as from any page.
- A text item first, in `green-muted`: "How the menu works, with the buttons your config has." Then four sections, each under a [SectionHeader](../SectionHeader/README.md) that does not fold: the page scrolls.
- Each line as the help line has a row's help: `Name: ` in `green`, the text in `green-muted`, in `t-body`, wrapped in the page's width with as many lines as it needs, `space-2` in from the page's edges. A line about a mark has the mark first, as its row draws it, in a 20 px column, `space-2` before the name.
- **Navigation**:
  - Open and close: the binding, and "While the menu is minimized it shows it again." As shipped: "Long press both thumbsticks."
  - Pointing at the menu: "That hand's trigger and thumbstick work the menu, and the game does not get them: you do not fire or turn by accident."
  - Back: the binding, then "while pointing at the menu.": "Tap the grip on either hand while pointing at the menu."
  - Move the menu: the binding, then "while pointing at the menu: it follows the hand until you let go."
  - Recenter: the binding, then "the menu is put in front of you again. It also comes back by itself when you walk away from it."
  - Minimize: "On the title bar: the menu is hidden as you left it, unsaved changes too. Your offhand wrist then has Restore, which shows it again with its values read again, and Exit, which closes it."
- **Changes**:
  - Save: "Nothing is written until you press Save. Changes are kept for each mod, and leaving a mod or closing the menu with unsaved changes asks first."
  - Undo one change, after the undo arrow in `amber`: "A changed value is amber, and the undo arrow after it takes back that change alone. Discard, in the footer, takes back all of them."
  - Reset, after its mark: "Sets the mod's default, which waits for Save too. The help line names the default."
  - Buttons: "A button that runs a mod's script asks you to save first, as the script reads the saved values."
  - When a change takes effect: "A mod can say it, with a mark after the row's name:", then each mark of [TakesEffect](../TakesEffect/README.md) in `amber` with what it means. `Live`: "The mod applies the change as you make it, so you see it in the game before you save. It is still unsaved: Save keeps it, and Discard, or leaving without saving, puts the old value back in the game too." `Save`: "The mod applies it when you save." `Restart`: "It applies after the game is restarted. The menu's front page reminds you until then." `No mark`, after the plain dot: "The mod does not say, as no MCM mod does. Save writes it all the same."
- **Marks**: `Amber value` in `amber`: "Changed, not saved yet."; `Red value` in `red` after the invalid mark: "Invalid: the help line says why."; `Warning` after the warning mark: "Something to know about the value as it is: the help line says what."; `Result` after the info mark: "A button gave a result: point at its row to read it."; `Greyed row` at `opacity-disabled`: "Point at it: the help line says why it cannot be changed."
- **MCM**, always there: Save: "Changes wait for Save here, where MCM's own menu writes each one as you make it."; Hotkeys: "A mod's hotkeys are shown, not edited: change them in MCM."; A mod that is greyed: "It cannot be shown: point at it to see why, and the front page lists it under Notices."
- A button or a binding is said from the player's config and handedness, shorter than the sentence of `InputBinding`: no time or threshold, and a chord of the same button on both hands as one press of both, "Long press both thumbsticks". Back and the move are buttons of the hand that points, so an A button is "the A or X button". A control the config turns off is left out.
- Nothing on the page is pressed. The footer is the selected mod's, with its status.
