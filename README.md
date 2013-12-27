non
===
This branch was created originally to optimize graphics drawing in Non Sequencer and has since become more general in scope to also include bug fixes and functional improvements.

About Non
===
Non reinvents the DAW.  Powerful enough to form a complete studio, fast and light enough to run on low-end hardware like the eeePC or Raspberry Pi, and so reliable that it can be used live, the Non DAW Studio is a modular system composed of four main parts: Non Timeline, a non-destructive, non-linear audio recorder and arranger. Non Mixer, a live mixer with effects plugin hosting and advanced Ambisonics spatialization control. Non Sequencer, a live, pattern based MIDI sequencer, and finally, the Non Session Manager to tie together these applications and more into cohesive song-level units.


Documentation
===
Here you will find some Non Sequencer user interface documentation, which is evolving as I figure out how to use it.  Also, this branch is currently exploring some fairly drastic user interface changes from the main Non branch.  This documentation is meant to reflect these changes.


Pattern Editor
===
* Left Click to place a note (drag to change placement).
* Left Click on a note and drag to move it (changes time and note).
* ALT-Left Click on a note and drag to change velocity/duration (vertical/horizontal).
* ALT-Left Click on a note without dragging to delete it.
* CTRL-Left Click drag in grid to select a box region.
* Left click drag horizontally on ruler to select range.
* CTRL left click on a note to toggle selection of it.
* Hold down SHIFT while selecting a range/region to add to selection.
* Notes will be pasted to last selection area or the beginning of the pattern if no selection.

Key Bindings
===
* Ctrl-Delete: Delete time
* Ctrl-Insert: Insert time
* Alt-Left: Pan to previous note
* Alt-Right: Pan to next note
* Left: Pan lefts
* Right: Pan right
* Up: Pan up
* Down: Pan down
* f: Pan to playhead
* Ctrl-A: Select all
* Ctrl-Shift-A: Select none
* Ctrl-I: Invert selection
* Ctrl-C: Copy (copy selected notes to clipboard)
* Ctrl-V: Paste (paste clipboard to beginning of current selected range or region)
* Ctrl-X: Cut (copy selected notes to clipboard and delete them)
* Delete: Delete selected Items
* Shift-Delete: Delete all Items in Pattern/Phrase
* Ctrl-Left: Move selected left
* Ctrl-Right: Move selected right
* Ctrl-Up: Move selected up
* Ctrl-Down: Move selected down
* Ctrl-Z: Undo
* Ctrl-Shift-Z: Redo
* C: Crop (delete everything but what is in the selected region/range)
* a: Add new empty Pattern/Phrase
* d: Duplicate current Pattern/Phrase
* D: Duplicate range (creates a new Pattern with the selected notes)
* t: Trim to last note
* [: Previous Pattern/Phrase
* ]: Next Pattern/Phrase
