# Improved Transform Controls

This mod enhances the default <cy>Transform (warp) Controls</c>.

___

### <cj> Mod introduces 4 main features:</c>
- <cg>Anchor</c>: when disabled, all transformations are relative not to the anchor but to the opposite side (like in Photoshop).
- <cg>Snap</c>: snap the anchor to the transform points.
- <cg>Grid snap</c>:
  - snap positions of pivot points to the grid (only when rotation is multiple of 90),
  - snap anchor position to the grid,
  - snap rotation to the multiples of 45 degrees.
- <cg>Free rotation</c>: rotate transform interface without rotating transformed objects.

### <cp> You can use `Ctrl` and `Shift` keys to do certain actions:</c>
- Shift:
  - grid snap
- Ctrl:
  - anchor snap - when dragging the anchor
  - free rotation - when dragging the rotation control
  - transform relative to the center - when anchor is disabled

### <cy> Mod also adds some nice details such as:</c>
- Visible transform rectangle and extra guides
- Keeping transform interface buttons vertical during rotation

### <cl> And fixes some RobTop's bugs:</c>
- Fixed bug when the 'Lock' button sprite size doesn't match the button touch box
- Fixed overlap of buttons with the controls after certain transforms
- Fixed RobTop's <cr>crash</c> when transforming extremely thin objects
- Fixed RobTop's bug with incorrect transforms in Undo-Redo

---

## Contribution

- You can report a bug or suggest a feature on my [Discord server](https://discord.gg/wcWvtKHP8n)
- You can contribute to the code on [GitHub](https://github.com/RazoomGD/geode-improved-transform-controls)